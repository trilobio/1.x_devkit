import logging
import math
import numbers
import struct
import subprocess
import sys
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import Optional, Tuple

import can
import plac
from can.typechecking import Channel
from progress.bar import ShadyBar

logger = logging.getLogger(__name__)

BAR_TEXT_LEN = 20

# Constants
RESPONSE_ID = 0x111  # Devices always responds with this message ID
ACK = 0x79
NACK = 0x1F


class BootloaderCommand(IntEnum):
    GET = 0x00
    GET_VERSION = 0x01
    GET_ID = 0x02
    READ_MEMORY = 0x11
    GO = 0x21
    WRITE_MEMORY = 0x31
    ERASE_MEMORY = 0x44
    SPECIAL = 0x50
    EXTENDED_SPECIAL = 0x51
    WRITE_PROTECT = 0x63
    WRITE_UNPROTECT = 0x73
    READOUT_PROTECT = 0x82
    READOUT_UNPROTECT = 0x92


@dataclass
class GetResponse:
    protocol_version: int = -1
    supports_get: bool = False
    supports_get_version: bool = False
    supports_get_id: bool = False
    supports_read_memory: bool = False
    supports_go: bool = False
    supports_write_memory: bool = False
    supports_erase_memory: bool = False
    supports_special: bool = False
    supports_extended_special: bool = False
    supports_write_protect: bool = False
    supports_write_unprotect: bool = False
    supports_readout_protect: bool = False
    supports_readout_unprotect: bool = False

    def parse_commands(self, commands: list[int]):
        self.supports_get = BootloaderCommand.GET in commands
        self.supports_get_version = BootloaderCommand.GET_VERSION in commands
        self.supports_get_id = BootloaderCommand.GET_ID in commands
        self.supports_read_memory = BootloaderCommand.READ_MEMORY in commands
        self.supports_go = BootloaderCommand.GO in commands
        self.supports_write_memory = BootloaderCommand.WRITE_MEMORY in commands
        self.supports_erase_memory = BootloaderCommand.ERASE_MEMORY in commands
        self.supports_special = BootloaderCommand.SPECIAL in commands
        self.supports_extended_special = BootloaderCommand.EXTENDED_SPECIAL in commands
        self.supports_write_protect = BootloaderCommand.WRITE_PROTECT in commands
        self.supports_write_unprotect = BootloaderCommand.WRITE_UNPROTECT in commands
        self.supports_readout_protect = BootloaderCommand.READOUT_PROTECT in commands
        self.supports_readout_unprotect = (
            BootloaderCommand.READOUT_UNPROTECT in commands
        )


def parse_auto_int(s: str) -> int:
    s = s.strip()
    if s.lower().startswith("0x"):
        return int(s[2:], 16)
    elif s.lower().startswith("0b"):
        return int(s[2:], 2)
    elif s.isdigit() or (s.startswith("-") and s[1:].isdigit()):
        return int(s, 10)
    else:
        raise ValueError(f"Invalid integer format: {s}")


def parse_auto_bytes(s: str) -> bytes:
    s = s.strip().lower()
    if s.startswith("0x"):
        hex_str = s[2:]
        if len(hex_str) % 2:  # Make sure it's even length
            hex_str = "0" + hex_str
        return bytes.fromhex(hex_str)
    elif s.startswith("0b"):
        bin_str = s[2:]
        if len(bin_str) % 8:  # Pad to byte boundary
            bin_str = bin_str.zfill((len(bin_str) + 7) // 8 * 8)
        return int(bin_str, 2).to_bytes(len(bin_str) // 8, byteorder="big")
    elif s.isdigit() or (s.startswith("-") and s[1:].isdigit()):
        num = int(s, 10)
        if num < 0:
            raise ValueError(
                "Negative decimal numbers not supported for byte conversion"
            )
        length = (num.bit_length() + 7) // 8
        return num.to_bytes(length or 1, byteorder="big")
    else:
        raise ValueError(f"Invalid input format for bytes: {s}")


def int_to_hex(data: list[int]) -> list[str]:
    res = []
    for el in data:
        res.append("0x%0.2X" % el)
    return res


def send_command(
    bus: can.BusABC, cmd_id: int, data: bytes = bytes(), timeout: float = 1
):
    """Send command with cmd_id as arbitration ID."""

    msg = can.Message(
        arbitration_id=cmd_id,
        is_extended_id=False,
        is_fd=True,
        bitrate_switch=True,
        data=data,
    )
    logger.debug(f"Sending command ID 0x{cmd_id:02X} with data: {data}")
    bus.send(msg, timeout=timeout)


def recv_response(bus: can.BusABC, timeout=1.0) -> Tuple[bool, Optional[can.Message]]:
    """Wait for a response from the bootloader on RESPONSE_ID."""
    msg = bus.recv(timeout)
    if msg is None:
        logger.error("Timeout waiting for response")
        return False, None

    result = 0
    for i in range(msg.dlc):
        result = (result << 8) + msg.data[i]
    logger.debug(f"Message received: {result}")
    return True, msg


def GET(bus: can.BusABC, timeout: float = 1.0) -> Tuple[bool, Optional[GetResponse]]:

    send_command(bus, BootloaderCommand.GET, timeout=timeout)
    success, get_ack = recv_response(bus)
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK GET command")
        return False, None

    # Receive the number of bytes
    success, num_bytes = recv_response(bus, timeout=timeout)
    if not success:
        logger.error("Bootloader failed to get number of bytes")
        return False, None
    num_bytes = num_bytes.data[0]

    bar = ShadyBar("Getting Commands".ljust(BAR_TEXT_LEN), max=num_bytes + 1)
    res = GetResponse()

    # Receive the protocol version
    success, protocol_version = recv_response(bus, timeout=timeout)
    if not success:
        logger.error("Bootloader failed to get protocol version")
        return False, None
    bar.next()

    res.protocol_version = int.from_bytes(
        protocol_version.data, byteorder="big", signed=False
    )

    # Receive the commands availiable
    commands = []
    for i in range(num_bytes):
        success, command = recv_response(bus, timeout=timeout)
        if not success:
            logger.error(f"Bootloader failed to get command {i}")
            return False, None
        commands.append(command.data[0])
        bar.next()
    logger.info(f"Commands Availiable: {int_to_hex(commands)}")
    res.parse_commands(commands)

    success, get_ack = recv_response(bus)
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK GET command finish")
        return False, None
    bar.finish()
    return True, res


def ERASE_MEMORY(
    bus: can.BusABC,
    data: bytes,
    base_address: int,
    start_address: int,
    page_size: int,
    timeout=1.0,
) -> bool:
    num_pages = math.ceil(len(data) / page_size)
    bar = ShadyBar("Erasing Memory".ljust(BAR_TEXT_LEN), max=num_pages)
    num_pages = num_pages.to_bytes(2, byteorder="big")  # Number of pages to erase
    send_command(bus, BootloaderCommand.ERASE_MEMORY, num_pages, timeout=timeout)

    # We expect two sequential ACKs, one because the command was received
    # correctly and one because the page is valid
    success, get_ack = recv_response(bus)
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK ERASE_MEMORY command")
        return False

    success, get_ack = recv_response(bus)
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK ERASE_MEMORY command page validation")
        return False

    pages: bytearray = bytearray()
    for i in range(0, len(data), page_size):
        is_page_aligned = (start_address + i) % page_size == 0
        if not is_page_aligned:
            logger.error(
                f"Page to erase is not aligned : address -> {start_address+i}, page_size -> {page_size}"
            )
            return False
        page_number = int((start_address - base_address + i) / page_size)
        pages += page_number.to_bytes(2, byteorder="big")
        bar.next()

    send_command(bus, BootloaderCommand.ERASE_MEMORY, pages)

    success, get_ack = recv_response(bus)
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK ERASE_MEMORY command list of pages")
        return False
    bar.finish()
    return True


def WRITE_FILE(bus: can.BusABC, data: bytes, start_address: int, timeout=1.0) -> bool:
    bar = ShadyBar("Uploading".ljust(BAR_TEXT_LEN), max=math.ceil(len(data) / 64))
    # Iterate through in 256 byte chuncks
    for i in range(0, len(data), 256):
        # Yes this can go "off" the end of the array, but python truncates it
        # nicely, so stop complaining
        data_to_write = data[i : i + 256]
        addr = bytearray((start_address + i).to_bytes(4, byteorder="big"))

        send_command(
            bus,
            BootloaderCommand.WRITE_MEMORY,
            addr
            + bytearray(
                [len(data_to_write) - 1]
            ),  # Concatenate length of data - 1 to data packet
            timeout=timeout,
        )

        success, get_ack = recv_response(bus)
        if not success or get_ack.data[0] != ACK:
            logger.error(f"Bootloader failed to ACK WRITE_MEMORY command")
            return False

        for j in range(0, len(data_to_write), 64):
            # Write data
            send_command(
                bus,
                BootloaderCommand.WRITE_MEMORY,
                data_to_write[j : j + 64],
                timeout=timeout,
            )
            bar.next()

        success, get_ack = recv_response(bus)
        if not success or get_ack.data[0] != ACK:
            logger.error(
                f"Bootloader failed to ACK Sending data via WRITE_MEMORY command"
            )
            return False
    bar.finish()
    return True


def VERIFY_UPLOAD(
    bus: can.BusABC, data: bytes, start_address: int, timeout=1.0
) -> bool:
    bar = ShadyBar("Verifying".ljust(BAR_TEXT_LEN), max=math.ceil(len(data) / 64))
    # Iterate through in 256 byte chuncks
    for i in range(0, len(data), 256):
        # Yes this can go "off" the end of the array, but python truncates it
        # nicely, so stop complaining
        data_to_verify = data[i : i + 256]
        cmd_data = struct.pack(">IB", start_address + i, len(data_to_verify) - 1)
        send_command(
            bus,
            BootloaderCommand.READ_MEMORY,
            cmd_data,
            timeout=timeout,
        )

        success, get_ack = recv_response(bus)
        if not success or get_ack.data[0] != ACK:
            logger.error(f"Bootloader failed to ACK READ_MEMORY command")
            return False

        for j in range(0, len(data_to_verify), 64):
            # Read data
            success, resp = recv_response(bus)
            if not success:
                logger.error(
                    f"Bootloader failed to READ_MEMORY during data acquisition"
                )
                return False
            if not all(
                x == y for x, y in zip(resp.data, bytearray(data_to_verify[j : j + 64]))
            ):
                logger.error(f"Data mismatch during read")
                return False
            bar.next()

        success, get_ack = recv_response(bus)
        if not success or get_ack.data[0] != ACK:
            logger.error(
                f"Bootloader failed to ACK Receving data via READ_MEMORY command"
            )
            return False
    bar.finish()
    return True


def GO(bus: can.BusABC, address: int, chip_id: Optional[bytes] = None, timeout=1.0):
    bar = ShadyBar("Go".ljust(BAR_TEXT_LEN), max=3)
    cmd_data = struct.pack(">I", address)
    if chip_id is not None:
        cmd_data += chip_id

    send_command(bus, BootloaderCommand.GO, cmd_data, timeout=timeout)
    bar.next()

    success, get_ack = recv_response(bus)
    bar.next()
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK GO command.")
        return False

    success, get_ack = recv_response(bus)
    bar.next()
    if not success or get_ack.data[0] != ACK:
        logger.error("Bootloader failed to ACK GO command parameters")
        return False

    bar.finish()
    return True


@plac.pos(
    "can_channel",
    help="CAN channel the MCU is attached to. If the channel is down, this tool will try to bring up the channel.",
)
@plac.opt("binary", help="Binary file to write", abbrev="b", type=Path)
@plac.opt(
    "start_address",
    help="Start address to upload the binary file (accepts hex as [0x,0X] binary as [0b,0B] and base 10 as just integers)",
    abbrev="sa",
    type=str,
)
@plac.opt(
    "base_address",
    help="Base address of Flash, used to calculate page number (accepts hex as [0x,0X] binary as [0b,0B] and base 10 as just integers)",
    abbrev="ba",
    type=str,
)
@plac.opt(
    "page_size",
    help="Page size of the MCU (accepts hex as [0x,0X] binary as [0b,0B] and base 10 as just integers)",
    abbrev="p",
    type=str,
)
@plac.opt(
    "chip_uuid",
    help="Unique Identifier of the STM32 device (accepts hex as [0x,0X] binary as [0b,0B] and base 10 as just integers)",
    abbrev="c",
    type=str,
)
@plac.opt(
    "log_level",
    help="Log level (string such as 'NOTSET', 'DEBUG', 'INFO', 'WARNING', or 'ERROR', 'CRITICAL' or integer)",
    abbrev="l",
    type=str,
)
def main(
    can_channel: Channel | None,
    binary: Path,
    start_address: str = "0x08008000",
    base_address: str = "0x08000000",
    page_size: str = "0x2000",
    chip_uuid: Optional[str] = None,
    log_level: str | int = logging.INFO,
):
    """
    Upload a program to STM32H523 over FDCAN using OpenBootloader
    """
    if not isinstance(log_level, numbers.Number):
        if isinstance(log_level, str):
            if log_level.isdigit():
                log_level_int = int(log_level)
                logging.basicConfig(
                    level=log_level_int, format="%(levelname)s: %(message)s"
                )

    start_address_int = parse_auto_int(start_address)
    base_address_int = parse_auto_int(base_address)
    page_size_int = parse_auto_int(page_size)
    chip_uuid_bytes: Optional[bytes] = None
    if chip_uuid is not None:
        chip_uuid_bytes = parse_auto_bytes(chip_uuid)

    logger.info(f"Opening CAN channel: {can_channel}")
    try:
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
        sys.exit(1)

    # Step 1: Send GET command
    success, res = GET(bus)
    if not success:
        sys.exit(1)
    logger.info(f"GET Response: {res}")
    if binary is not None:
        with open(binary, mode="rb") as file:
            content = file.read()
        success = ERASE_MEMORY(
            bus, content, base_address_int, start_address_int, page_size_int
        )
        if not success:
            logger.error(f"Failed to erase sectors")
            sys.exit(1)

        success = WRITE_FILE(bus, content, start_address_int)
        if not success:
            logger.error(f"Failed to write file")
            sys.exit(1)

        success = VERIFY_UPLOAD(bus, content, start_address_int)
        if not success:
            logger.error(f"Failed to verify upload")
            sys.exit(1)
        # Maybe should expose this to the CLI
        success = GO(bus, start_address_int, chip_uuid_bytes)
        if not success:
            logger.error(f"Failed to jump to application")
            sys.exit(1)

    bus.shutdown()
    return


if __name__ == "__main__":
    plac.call(main)
