import logging
from pathlib import Path
import re
import struct
import subprocess
from typing import Optional, Tuple

import can
import plac
from can.typechecking import Channel

from stm32_fdcan_loader.main import (
    ACK,
    ERASE_MEMORY,
    GET,
    GO,
    VERIFY_UPLOAD,
    WRITE_FILE,
    BootloaderCommand,
    int_to_hex,
    parse_auto_bytes,
    parse_auto_int,
    recv_response,
    send_command,
)

logger = logging.getLogger(__name__)

UUID_ADDRESS = 0x08FFF800


def get_all_chip_uuids(
    bus: can.BusABC, uuid_address: int, timeout: float = 0.1
) -> Tuple[bool, list[bytearray]]:

    cmd_data = struct.pack(
        ">IB", uuid_address, 12 - 1
    )  # UUID length is 12 bytes, we send N-1
    send_command(
        bus,
        BootloaderCommand.READ_MEMORY,
        cmd_data,
        timeout=timeout,
    )

    chip_ids = []
    while True:
        success, resp = recv_response(bus, timeout=timeout)
        if success and resp is not None:
            if resp.dlc == 64:
                chip_ids.append(resp.data[:12])
        else:
            break

    return True, chip_ids


def get_one_chip_uuid(
    bus: can.BusABC, uuid_address: int, timeout: float = 1
) -> Tuple[bool, Optional[can.Message]]:
    cmd_data = struct.pack(">IB", uuid_address, 12 - 1)
    send_command(
        bus,
        BootloaderCommand.READ_MEMORY,
        cmd_data,
        timeout=timeout,
    )

    success, get_ack = recv_response(bus)
    print(success, get_ack.data[0])
    if not success or get_ack.data[0] != ACK:
        logger.error(f"Bootloader failed to ACK READ_MEMORY command")
        return False, None

    # Read data
    success, resp = recv_response(bus)
    if not success:
        logger.error(f"Bootloader failed to READ_MEMORY during data acquisition")
        return False, None

    success, get_ack = recv_response(bus)
    if not success or get_ack.data[0] != ACK:
        logger.error(f"Bootloader failed to ACK Receving data via READ_MEMORY command")
        return False, None
    return True, resp


@plac.pos("can_channel", help="CAN channel the MCU is attached to")
@plac.flg(
    "discover",
    help="Discover the existing UUIDs of boards in bootloader mode on the bus (accepts hex as [0x,0X] binary as [0b,0B] and base 10 as just integers)",
    abbrev="d",
)
@plac.opt(
    "binaries",
    help="One or more binary files to upload (place multiple paths in a comma or whitespace separated string)",
    abbrev="b",
    metavar="PATHS",
    type=str,
)
@plac.opt(
    "uuids",
    help="One or more STM32 UUIDs to upload each corresponding binary file to (place multiple uuids in a comma or whitespace separated string)",
    abbrev="u",
    metavar="UUIDS",
    type=str,
)
@plac.opt(
    "start_address",
    help="Start address to upload the binary file (accepts hex as [0x,0X] binary as [0b,0B] and base 10 as just integers)",
    abbrev="sa",
    type=str,
)
@plac.opt(
    "log_level",
    help="Log level (string such as 'NOTSET', 'DEBUG', 'INFO', 'WARNING', or 'ERROR', 'CRITICAL' or integer)",
    abbrev="l",
    type=str,
)
def main(
    can_channel: Channel,
    discover: bool = False,
    binaries: Optional[str] = None,
    uuids: Optional[str] = None,
    start_address: str = "0x08008000",
    base_address: str = "0x08000000",
    page_size: str = "0x2000",
    log_level: str | int = logging.INFO,
):
    """Upload new code to boards on a multi-board bus"""
    logging.basicConfig(level=log_level, format="%(levelname)s: %(message)s")
    logger.info(f"Opening CAN channel: {can_channel}")
    start_address_int = parse_auto_int(start_address)
    base_address_int = parse_auto_int(base_address)
    page_size_int = parse_auto_int(page_size)
    path_content = []
    byte_uuids: list[bytes] = []
    if binaries is not None:
        paths = [
            Path(p) for p in re.split(r"[,\s]+", binaries)
        ]  # splits on comma or whitespace

        if uuids is None:
            raise RuntimeError("No UUIDs provided, but Binaries are present")
        byte_uuids += [parse_auto_bytes(u) for u in re.split(r"[,\s]+", uuids)]
        for p in paths:
            with open(p, mode="rb") as file:
                path_content.append(file.read())
        if len(paths) != len(byte_uuids):
            raise RuntimeError(
                f"Number of UUIDs and Files should match. Got {len(paths)} files and {len(byte_uuids)} UUIDs"
            )
    try:
        subprocess.run(["ifconfig", str(can_channel), "up"])
        bus = can.interface.Bus(
            channel=str(can_channel),
            interface="socketcan",
            can_filters=None,
            fd=True,
            err_reporting=True,
            receive_own_messages=False,
            local_loopback=False,
        )
    except Exception as e:
        logger.error(f"Failed to open CAN channel {can_channel}: {e}")
        return

    success, chip_ids = get_all_chip_uuids(bus, UUID_ADDRESS, timeout=0.1)
    if not success:
        raise RuntimeError("Failed to find any boards in bootloader mode on the bus")
    if discover:
        print(f"    Chip IDs Present: {["0x" + c.hex() for c in chip_ids]}")

    # Upload chip id
    for cid_up_idx in range(len(byte_uuids)):
        # Jump chip id
        for cid_jmp_idx in range(len(chip_ids)):
            if chip_ids[cid_jmp_idx] == byte_uuids[cid_up_idx]:
                continue
            GO(bus, start_address_int, chip_ids[cid_jmp_idx])
            while True:
                # Make sure to purge the bus of a NACK from other boards
                success, _ = recv_response(bus, timeout=0.1)
                if not success:
                    break

        success = ERASE_MEMORY(
            bus,
            path_content[cid_up_idx],
            base_address_int,
            start_address_int,
            page_size_int,
        )
        if not success:
            logger.error(f"Failed to erase sectors")
            return

        success = WRITE_FILE(bus, path_content[cid_up_idx], start_address_int)
        if not success:
            logger.error(f"Failed to write file")
            return

        success = VERIFY_UPLOAD(bus, path_content[cid_up_idx], start_address_int)
        if not success:
            logger.error(f"Failed to verify upload")
            return

        # Maybe should expose this to the CLI
        success = GO(bus, start_address_int, byte_uuids[cid_up_idx])
        if not success:
            logger.error(f"Failed to jump to application")
            return

    bus.shutdown()


if __name__ == "__main__":
    plac.call(main)
