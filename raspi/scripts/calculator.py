#!/usr/bin/env python3
"""Exercise the calculator commands implemented by firmware/tool_devkit/Core/Src/protocol.c.

CAN ID layout, from most-significant bit to least-significant bit:

    Priority   Board ID   Command ID   Request ID   Error
    2 bits     10 bits    8 bits       8 bits       1 bit

Request payloads are packed like protocol.h:

    uint8_t a;
    uint8_t b;

Response payloads are:

    uint8_t a;
"""

from __future__ import annotations

import argparse
import struct
import time
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


# Must match firmware/tool_devkit/Core/Inc/protocol.h.
TOOL_DEVKIT_ID = 0x101
PDM_1_ID = 0x102
PDM_2_ID = 0x103


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


def _u8(value: int) -> int:
    """Match protocol.c's uint8_t wraparound arithmetic."""
    return value & 0xFF


def open_bus(channel: str, *, fd: bool = True) -> can.BusABC:
    """Open a SocketCAN bus.

    For CAN-FD, python-can needs fd=True on the SocketCAN bus object,
    matching can-elegans' use of ThreadSafeBus(..., fd=True).
    """
    return can.interface.Bus(
        interface="socketcan",
        channel=channel,
        fd=fd,
        receive_own_messages=False,
    )


def send_frame(
    bus: can.BusABC,
    can_id: TriloCanId,
    data: bytes = b"",
    *,
    is_fd: bool = True,
    bitrate_switch: bool = True,
    timeout_s: float = 1.0,
) -> None:
    message = can.Message(
        arbitration_id=can_id.pack(),
        is_extended_id=True,
        is_fd=is_fd,
        bitrate_switch=bitrate_switch,
        data=data,
        check=True,
    )
    bus.send(message, timeout=timeout_s)


def drain_rx(bus: can.BusABC) -> None:
    """Discard stale frames so each test sees fresh responses."""
    while bus.recv(timeout=0.0) is not None:
        pass


@dataclass(frozen=True)
class CalculatorTest:
    name: str
    request_command: ToolDevkitCommand
    response_command: ToolDevkitCommand
    a: int
    b: int
    expected: int
    expect_error: bool = False


def send_calculation_request(
    bus: can.BusABC,
    test: CalculatorTest,
    *,
    board_id: int,
    priority: int,
    request_id: int,
    is_fd: bool,
    bitrate_switch: bool,
) -> TriloCanId:
    can_id = TriloCanId(
        priority=priority,
        board_id=board_id,
        command_id=test.request_command,
        request_id=request_id,
        error=False,
    )
    data = struct.pack("<BB", _u8(test.a), _u8(test.b))
    send_frame(bus, can_id, data, is_fd=is_fd, bitrate_switch=bitrate_switch)
    print(
        f"sent {test.name:<8} request_id={request_id:3d} "
        f"{test.a} {_calculation_symbol(test)} {test.b}  "
        f"id=0x{can_id.pack():08X} data={data.hex(' ')}"
    )
    return can_id


def wait_for_calculation_response(
    bus: can.BusABC,
    test: CalculatorTest,
    *,
    board_id: int,
    request_id: int,
    timeout_s: float,
) -> bool:
    deadline = time.monotonic() + timeout_s

    while True:
        remaining_s = deadline - time.monotonic()
        if remaining_s <= 0:
            print(f"FAIL {test.name:<8} timed out waiting for response")
            return False

        message = bus.recv(timeout=remaining_s)
        if message is None:
            print(f"FAIL {test.name:<8} timed out waiting for response")
            return False

        if not message.is_extended_id:
            continue

        try:
            response_id = TriloCanId.unpack(message.arbitration_id)
        except ValueError:
            continue

        if response_id.board_id != board_id:
            continue
        if response_id.request_id != request_id:
            continue
        if response_id.command_id != test.response_command:
            continue

        actual = message.data[0] if len(message.data) >= 1 else None
        expected = _u8(test.expected)
        error_ok = response_id.error == test.expect_error
        value_ok = actual == expected
        ok = error_ok and value_ok

        status = "PASS" if ok else "FAIL"
        print(
            f"{status} {test.name:<8} response_id=0x{message.arbitration_id:08X} "
            f"error={int(response_id.error)} expected_error={int(test.expect_error)} "
            f"value={actual} expected={expected} data={bytes(message.data).hex(' ')}"
        )
        return ok


def run_calculator_tests(
    bus: can.BusABC,
    *,
    board_id: int,
    priority: int,
    first_request_id: int,
    is_fd: bool,
    bitrate_switch: bool,
    timeout_s: float,
) -> bool:
    tests = [
        CalculatorTest(
            name="add",
            request_command=ToolDevkitCommand.ADD_REQUEST,
            response_command=ToolDevkitCommand.ADD_RESPONSE,
            a=11,
            b=22,
            expected=11 + 22,
        ),
        CalculatorTest(
            name="subtract",
            request_command=ToolDevkitCommand.SUBTRACT_REQUEST,
            response_command=ToolDevkitCommand.SUBTRACT_RESPONSE,
            a=50,
            b=8,
            expected=50 - 8,
        ),
        CalculatorTest(
            name="multiply",
            request_command=ToolDevkitCommand.MULTIPLY_REQUEST,
            response_command=ToolDevkitCommand.MULTIPLY_RESPONSE,
            a=9,
            b=7,
            expected=9 * 7,
        ),
        CalculatorTest(
            name="divide",
            request_command=ToolDevkitCommand.DIVIDE_REQUEST,
            response_command=ToolDevkitCommand.DIVIDE_RESPONSE,
            a=84,
            b=6,
            expected=84 // 6,
        ),
    ]

    all_ok = True
    drain_rx(bus)

    for offset, test in enumerate(tests):
        request_id = _u8(first_request_id + offset)
        send_calculation_request(
            bus,
            test,
            board_id=board_id,
            priority=priority,
            request_id=request_id,
            is_fd=is_fd,
            bitrate_switch=bitrate_switch,
        )
        ok = wait_for_calculation_response(
            bus,
            test,
            board_id=board_id,
            request_id=request_id,
            timeout_s=timeout_s,
        )
        all_ok = all_ok and ok

    return all_ok


def _calculation_symbol(test: CalculatorTest) -> str:
    return {
        "add": "+",
        "subtract": "-",
        "multiply": "*",
        "divide": "/",
    }[test.name]


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Send ADD/SUBTRACT/MULTIPLY/DIVIDE CAN-FD requests and verify responses"
    )
    parser.add_argument(
        "board_id", type=lambda value: int(value, 0), nargs="?", default=TOOL_DEVKIT_ID
    )
    parser.add_argument("--channel", default="can1")
    parser.add_argument("--priority", type=int, default=0)
    parser.add_argument(
        "--request-id", type=int, default=1, help="first request ID to use"
    )
    parser.add_argument(
        "--timeout", type=float, default=1.0, help="seconds to wait for each response"
    )
    parser.add_argument(
        "--classic",
        action="store_true",
        help="send as classic CAN instead of CAN-FD",
    )
    parser.add_argument(
        "--no-bitrate-switch",
        action="store_true",
        help="disable CAN-FD bitrate switch on transmitted requests",
    )
    args = parser.parse_args()

    is_fd = not args.classic
    bitrate_switch = is_fd and not args.no_bitrate_switch

    with open_bus(args.channel, fd=is_fd) as bus:
        all_ok = run_calculator_tests(
            bus,
            board_id=args.board_id,
            priority=args.priority,
            first_request_id=args.request_id,
            is_fd=is_fd,
            bitrate_switch=bitrate_switch,
            timeout_s=args.timeout,
        )

    raise SystemExit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
