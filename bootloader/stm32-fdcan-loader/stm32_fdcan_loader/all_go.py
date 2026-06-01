import logging
import struct
import subprocess

import can
import plac
from can.typechecking import Channel

from stm32_fdcan_loader.main import (
    BootloaderCommand,
    parse_auto_int,
    recv_response,
    send_command,
)

logger = logging.getLogger(__name__)


def ALL_GO(bus: can.BusABC, address: int, timeout=1.0):
    cmd_data = struct.pack(">I", address)
    send_command(bus, BootloaderCommand.GO, cmd_data, timeout=timeout)
    response_count = 0
    while True:
        success, resp = recv_response(bus, timeout=timeout)
        if success and resp is not None:
            print(f"   Success: {success}, Resp: {resp}")
            response_count += 0.5
        else:
            break

    print(f"{response_count} boards responded")
    return True


@plac.pos("can_channel", help="CAN channel the MCU is attached to")
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
    start_address: str = "0x08008000",
    log_level: str | int = logging.INFO,
):
    """Send the GO command to all boards on the bus"""
    logging.basicConfig(level=log_level, format="%(levelname)s: %(message)s")
    logger.info(f"Opening CAN channel: {can_channel}")
    start_address_int = parse_auto_int(start_address)
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

    ALL_GO(bus, start_address_int)

    bus.shutdown()


if __name__ == "__main__":
    plac.call(main)
