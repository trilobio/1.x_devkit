#!/usr/bin/env python3
"""Send simple Trilo CAN frames from a Raspberry Pi.
CAN ID layout, from most-significant bit to least-significant bit:
    Priority   Board ID   Command ID   Request ID   Error
    2 bits     10 bits    8 bits       8 bits       1 bit

"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from enum import IntEnum

import can


class ToolDevkitCommand(IntEnum):
    PING = 0x01
    ADD_REQUEST = 0x02
    ADD_RESPONSE = 0x03
    SUBTRACT_REQUEST = 0x04
    SUBTRACT_RESPONSE = 0x05
    MULTIPLY_REQUEST = 0x06
    MULTIPLY_RESPONSE = 0x07
    DIVIDE_REQUEST = 0x08
    DIVIDE_RESPONSE = 0x09


TOOL_DEVKIT_ID = 101
PDM_1_ID = 102
PDM_2_ID = 103


@dataclass(frozen=True)
class TriloCanId:
    priority: int
    board_id: int
    command_id: int
    request_id: int
    error: bool = False

    def pack(self) -> int:
        """Pack this ID into a 29-bit extended CAN arbitration ID."""
        _require_bits("priority", self.priority, 2)
        _require_bits("board_id", self.board_id, 10)
        _require_bits("command_id", self.command_id, 8)
        _require_bits("request_id", self.request_id, 8)

        return (
            (self.priority << 27)
            | (self.board_id << 17)
            | (self.command_id << 9)
            | (self.request_id << 1)
            | int(self.error)
        )

    @classmethod
    def unpack(cls, arbitration_id: int) -> "TriloCanId":
        """Decode a 29-bit extended ID into fields."""
        _require_bits("arbitration_id", arbitration_id, 29)

        return cls(
            priority=(arbitration_id >> 27) & 0b11,
            board_id=(arbitration_id >> 17) & 0x3FF,
            command_id=(arbitration_id >> 9) & 0xFF,
            request_id=(arbitration_id >> 1) & 0xFF,
            error=bool(arbitration_id & 0b1),
        )


def _require_bits(name: str, value: int, bit_count: int) -> None:
    if value < 0 or value >= (1 << bit_count):
        raise ValueError(f"{name}={value} does not fit in {bit_count} bits")


def open_bus(channel: str) -> can.BusABC:
    """Open a SocketCAN bus."""
    return can.interface.Bus(interface="socketcan", channel=channel)


def send_frame(
    bus: can.BusABC,
    can_id: TriloCanId,
    data: bytes = b"",
    *,
    timeout_s: float = 1.0,
) -> None:
    message = can.Message(
        arbitration_id=can_id.pack(),
        is_extended_id=True,
        is_fd=True,
        data=data,
    )
    bus.send(message, timeout=timeout_s)


def send_ping(
    bus: can.BusABC,
    board_id: int,
    *,
    priority: int = 0,
    request_id: int = 0,
) -> None:
    """Send an empty PING command to one board."""
    can_id = TriloCanId(
        priority=priority,
        board_id=board_id,
        command_id=ToolDevkitCommand.PING,
        request_id=request_id,
        error=False,
    )
    send_frame(bus, can_id)
    print(f"sent PING to board {board_id} with CAN ID 0x{can_id.pack():08X}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Send a Trilo CAN ping frame")
    parser.add_argument("board_id", type=int, nargs="?", default=TOOL_DEVKIT_ID)
    parser.add_argument("--channel", default="can1")
    parser.add_argument("--priority", type=int, default=0)
    parser.add_argument("--request-id", type=int, default=0)
    args = parser.parse_args()

    with open_bus(args.channel) as bus:
        send_ping(
            bus,
            args.board_id,
            priority=args.priority,
            request_id=args.request_id,
        )


if __name__ == "__main__":
    main()
