#!/usr/bin/env python3
"""Protocol test client for firmware/tool_devkit/Core/Src/protocol.c.

This file is intentionally structured like the inverse of protocol.h/protocol.c:

- CanID mirrors the C CanID struct and packs/unpacks the 29-bit extended CAN ID.
- CanFrame mirrors the C CanFrame struct and converts to/from python-can messages.
- CommandID mirrors the C CommandID enum.
- Request/response data classes mirror the packed C payload structs.
- ProtocolTest defines one transmitted frame and one expected response frame.

To add future request/response tests, define the new data struct class and add another
ProtocolTest to build_protocol_tests().
"""

from __future__ import annotations

import argparse
import struct
import time
from dataclasses import dataclass
from enum import IntEnum

import can

# -----------------------------------------------------------------------------
# Constants from protocol.h
# -----------------------------------------------------------------------------

BOARD_ID = 0x101
ALL_CALL_ID = 0x000

ERROR_FLAG_SZ = 1
REQUEST_ID_SZ = 8
COMMAND_ID_SZ = 8
BOARD_ID_SZ = 10
PRIORITY_SZ = 2

CANID_BITS = 29
TOTAL_SZ = ERROR_FLAG_SZ + REQUEST_ID_SZ + COMMAND_ID_SZ + BOARD_ID_SZ + PRIORITY_SZ
BOARD_ID_POSITION = CANID_BITS - PRIORITY_SZ - BOARD_ID_SZ

MAX_FRAME_DATA_LENGTH = 64

assert TOTAL_SZ == CANID_BITS


class CommandID(IntEnum):
    PING = 0x01
    ADD_REQUEST = 0x02
    ADD_RESPONSE = 0x03
    SUBTRACT_REQUEST = 0x04
    SUBTRACT_RESPONSE = 0x05
    MULTIPLY_REQUEST = 0x06
    MULTIPLY_RESPONSE = 0x07
    DIVIDE_REQUEST = 0x08
    DIVIDE_RESPONSE = 0x09


# -----------------------------------------------------------------------------
# CanID / CanFrame mirror protocol.h and protocol.c
# -----------------------------------------------------------------------------


@dataclass(frozen=True)
class CanID:
    priority: int
    board_id: int
    command_id: int
    request_id: int
    error_flag: bool = False

    def as_int(self) -> int:
        """Mirror protocol.c constructCanID()."""
        _require_bits("priority", self.priority, PRIORITY_SZ)
        _require_bits("board_id", self.board_id, BOARD_ID_SZ)
        _require_bits("command_id", self.command_id, COMMAND_ID_SZ)
        _require_bits("request_id", self.request_id, REQUEST_ID_SZ)

        return (
            (
                (self.priority & _width_mask(PRIORITY_SZ))
                << (BOARD_ID_POSITION + BOARD_ID_SZ)
            )
            | ((self.board_id & _width_mask(BOARD_ID_SZ)) << BOARD_ID_POSITION)
            | (
                (self.command_id & _width_mask(COMMAND_ID_SZ))
                << (REQUEST_ID_SZ + ERROR_FLAG_SZ)
            )
            | ((self.request_id & _width_mask(REQUEST_ID_SZ)) << ERROR_FLAG_SZ)
            | (int(self.error_flag) & _width_mask(ERROR_FLAG_SZ))
        )

    @classmethod
    def parse(cls, can_id: int) -> "CanID":
        """Mirror protocol.c parseCanID()."""
        _require_bits("can_id", can_id, CANID_BITS)

        return cls(
            priority=(can_id >> (BOARD_ID_POSITION + BOARD_ID_SZ))
            & _width_mask(PRIORITY_SZ),
            board_id=(can_id >> BOARD_ID_POSITION) & _width_mask(BOARD_ID_SZ),
            command_id=(can_id >> (REQUEST_ID_SZ + ERROR_FLAG_SZ))
            & _width_mask(COMMAND_ID_SZ),
            request_id=(can_id >> ERROR_FLAG_SZ) & _width_mask(REQUEST_ID_SZ),
            error_flag=bool(can_id & _width_mask(ERROR_FLAG_SZ)),
        )

    def describe(self) -> str:
        command = command_name(self.command_id)
        return (
            f"id=0x{self.as_int():08X} priority={self.priority} board=0x{self.board_id:03X} "
            f"command={command} request={self.request_id} error={int(self.error_flag)}"
        )


@dataclass(frozen=True)
class CanFrame:
    id: CanID
    data: bytes = b""

    def __post_init__(self) -> None:
        if len(self.data) > MAX_FRAME_DATA_LENGTH:
            raise ValueError(
                f"CAN-FD payload is {len(self.data)} bytes; max is {MAX_FRAME_DATA_LENGTH}"
            )

    @classmethod
    def create(cls, can_id: CanID, data: bytes | bytearray = b"") -> "CanFrame":
        """Mirror protocol.c createCanFrame()."""
        return cls(can_id, bytes(data[:MAX_FRAME_DATA_LENGTH]))

    @classmethod
    def from_message(cls, message: can.Message) -> "CanFrame | None":
        if (
            not message.is_extended_id
            or message.is_remote_frame
            or message.is_error_frame
        ):
            return None
        return cls(CanID.parse(message.arbitration_id), bytes(message.data))

    def to_message(self, *, is_fd: bool, bitrate_switch: bool) -> can.Message:
        return can.Message(
            arbitration_id=self.id.as_int(),
            is_extended_id=True,
            is_fd=is_fd,
            bitrate_switch=bitrate_switch,
            data=self.data,
            check=True,
        )

    def describe(self) -> str:
        return f"{self.id.describe()} dl={len(self.data)} data={self.data.hex(' ')}"


# -----------------------------------------------------------------------------
# Packed payload structs from protocol.h
# -----------------------------------------------------------------------------


@dataclass(frozen=True)
class EmptyData:
    def pack(self) -> bytes:
        return b""

    @classmethod
    def unpack(cls, data: bytes) -> "EmptyData":
        _require_length(cls.__name__, data, 0)
        return cls()


@dataclass(frozen=True)
class AddRequestData:
    a: int
    b: int

    def pack(self) -> bytes:
        return struct.pack("<BB", _u8(self.a), _u8(self.b))

    @classmethod
    def unpack(cls, data: bytes) -> "AddRequestData":
        _require_length(cls.__name__, data, 2)
        return cls(*struct.unpack("<BB", data))


@dataclass(frozen=True)
class AddResponseData:
    a: int

    def pack(self) -> bytes:
        return struct.pack("<B", _u8(self.a))

    @classmethod
    def unpack(cls, data: bytes) -> "AddResponseData":
        _require_length(cls.__name__, data, 1)
        return cls(*struct.unpack("<B", data))


@dataclass(frozen=True)
class SubtractRequestData:
    a: int
    b: int

    def pack(self) -> bytes:
        return struct.pack("<BB", _u8(self.a), _u8(self.b))

    @classmethod
    def unpack(cls, data: bytes) -> "SubtractRequestData":
        _require_length(cls.__name__, data, 2)
        return cls(*struct.unpack("<BB", data))


@dataclass(frozen=True)
class SubtractResponseData:
    a: int

    def pack(self) -> bytes:
        return struct.pack("<B", _u8(self.a))

    @classmethod
    def unpack(cls, data: bytes) -> "SubtractResponseData":
        _require_length(cls.__name__, data, 1)
        return cls(*struct.unpack("<B", data))


@dataclass(frozen=True)
class MultiplyRequestData:
    a: int
    b: int

    def pack(self) -> bytes:
        return struct.pack("<BB", _u8(self.a), _u8(self.b))

    @classmethod
    def unpack(cls, data: bytes) -> "MultiplyRequestData":
        _require_length(cls.__name__, data, 2)
        return cls(*struct.unpack("<BB", data))


@dataclass(frozen=True)
class MultiplyResponseData:
    a: int

    def pack(self) -> bytes:
        return struct.pack("<B", _u8(self.a))

    @classmethod
    def unpack(cls, data: bytes) -> "MultiplyResponseData":
        _require_length(cls.__name__, data, 1)
        return cls(*struct.unpack("<B", data))


@dataclass(frozen=True)
class DivideRequestData:
    a: int
    b: int

    def pack(self) -> bytes:
        return struct.pack("<BB", _u8(self.a), _u8(self.b))

    @classmethod
    def unpack(cls, data: bytes) -> "DivideRequestData":
        _require_length(cls.__name__, data, 2)
        return cls(*struct.unpack("<BB", data))


@dataclass(frozen=True)
class DivideResponseData:
    a: int

    def pack(self) -> bytes:
        return struct.pack("<B", _u8(self.a))

    @classmethod
    def unpack(cls, data: bytes) -> "DivideResponseData":
        _require_length(cls.__name__, data, 1)
        return cls(*struct.unpack("<B", data))


# -----------------------------------------------------------------------------
# Generic protocol test harness
# -----------------------------------------------------------------------------


@dataclass(frozen=True)
class ProtocolTest:
    name: str
    send_frame: CanFrame
    expected_response_frame: CanFrame


def open_bus(channel: str, *, fd: bool = True) -> can.BusABC:
    return can.interface.Bus(
        interface="socketcan",
        channel=channel,
        fd=fd,
        receive_own_messages=False,
    )


def send_can_frame(
    bus: can.BusABC,
    frame: CanFrame,
    *,
    is_fd: bool,
    bitrate_switch: bool,
    timeout_s: float = 1.0,
) -> None:
    """Python side equivalent of protocol.c sendCanFrame()."""
    bus.send(
        frame.to_message(is_fd=is_fd, bitrate_switch=bitrate_switch), timeout=timeout_s
    )


def drain_rx(bus: can.BusABC) -> None:
    while bus.recv(timeout=0.0) is not None:
        pass


def run_protocol_test(
    bus: can.BusABC,
    test: ProtocolTest,
    *,
    is_fd: bool,
    bitrate_switch: bool,
    timeout_s: float,
) -> bool:
    print(f"SEND {test.name}: {test.send_frame.describe()}")
    send_can_frame(bus, test.send_frame, is_fd=is_fd, bitrate_switch=bitrate_switch)

    deadline = time.monotonic() + timeout_s
    while True:
        remaining_s = deadline - time.monotonic()
        if remaining_s <= 0:
            print(
                f"FAIL {test.name}: timed out waiting for {test.expected_response_frame.describe()}"
            )
            return False

        message = bus.recv(timeout=remaining_s)
        if message is None:
            print(
                f"FAIL {test.name}: timed out waiting for {test.expected_response_frame.describe()}"
            )
            return False

        received_frame = CanFrame.from_message(message)
        if received_frame is None:
            continue

        if received_frame.id.request_id != test.expected_response_frame.id.request_id:
            continue
        if received_frame.id.board_id != test.expected_response_frame.id.board_id:
            continue

        if received_frame == test.expected_response_frame:
            print(f"PASS {test.name}: {received_frame.describe()}")
            return True

        print(f"FAIL {test.name}: received unexpected response")
        print(f"  expected: {test.expected_response_frame.describe()}")
        print(f"  received: {received_frame.describe()}")
        return False


def run_protocol_tests(
    bus: can.BusABC,
    tests: list[ProtocolTest],
    *,
    is_fd: bool,
    bitrate_switch: bool,
    timeout_s: float,
) -> bool:
    all_ok = True
    drain_rx(bus)

    for test in tests:
        ok = run_protocol_test(
            bus,
            test,
            is_fd=is_fd,
            bitrate_switch=bitrate_switch,
            timeout_s=timeout_s,
        )
        all_ok = all_ok and ok

    return all_ok


# -----------------------------------------------------------------------------
# Manual test definitions
# -----------------------------------------------------------------------------


def build_protocol_tests(
    *, board_id: int, priority: int, first_request_id: int
) -> list[ProtocolTest]:
    """Manually define send frames and expected response frames.

    This is the part to extend when protocol.h/protocol.c gains more messages.
    """

    def can_id(
        command_id: CommandID, request_offset: int, *, error: bool = False
    ) -> CanID:
        return CanID(
            priority=priority,
            board_id=board_id,
            command_id=command_id,
            request_id=_u8(first_request_id + request_offset),
            error_flag=error,
        )

    return [
        ProtocolTest(
            name="add",
            send_frame=CanFrame.create(
                can_id(CommandID.ADD_REQUEST, 0),
                AddRequestData(a=11, b=22).pack(),
            ),
            expected_response_frame=CanFrame.create(
                can_id(CommandID.ADD_RESPONSE, 0),
                AddResponseData(a=33).pack(),
            ),
        ),
        ProtocolTest(
            name="subtract",
            send_frame=CanFrame.create(
                can_id(CommandID.SUBTRACT_REQUEST, 1),
                SubtractRequestData(a=50, b=8).pack(),
            ),
            expected_response_frame=CanFrame.create(
                can_id(CommandID.SUBTRACT_RESPONSE, 1),
                SubtractResponseData(a=42).pack(),
            ),
        ),
        ProtocolTest(
            name="multiply",
            send_frame=CanFrame.create(
                can_id(CommandID.MULTIPLY_REQUEST, 2),
                MultiplyRequestData(a=9, b=7).pack(),
            ),
            expected_response_frame=CanFrame.create(
                can_id(CommandID.MULTIPLY_RESPONSE, 2),
                MultiplyResponseData(a=63).pack(),
            ),
        ),
        ProtocolTest(
            name="divide",
            send_frame=CanFrame.create(
                can_id(CommandID.DIVIDE_REQUEST, 3),
                DivideRequestData(a=84, b=6).pack(),
            ),
            expected_response_frame=CanFrame.create(
                can_id(CommandID.DIVIDE_RESPONSE, 3),
                DivideResponseData(a=14).pack(),
            ),
        ),
    ]


# -----------------------------------------------------------------------------
# Small helpers
# -----------------------------------------------------------------------------


def command_name(command_id: int) -> str:
    try:
        return CommandID(command_id).name
    except ValueError:
        return f"UNKNOWN_0x{command_id:02X}"


def _width_mask(width: int) -> int:
    return (1 << width) - 1


def _require_bits(name: str, value: int, bit_count: int) -> None:
    if value < 0 or value >= (1 << bit_count):
        raise ValueError(f"{name}={value} does not fit in {bit_count} bits")


def _require_length(name: str, data: bytes, expected_length: int) -> None:
    if len(data) != expected_length:
        raise ValueError(f"{name} requires {expected_length} bytes, got {len(data)}")


def _u8(value: int) -> int:
    return value & 0xFF


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Send protocol request frames and verify exact expected response frames"
    )
    parser.add_argument(
        "board_id", type=lambda value: int(value, 0), nargs="?", default=BOARD_ID
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
    tests = build_protocol_tests(
        board_id=args.board_id,
        priority=args.priority,
        first_request_id=args.request_id,
    )

    with open_bus(args.channel, fd=is_fd) as bus:
        all_ok = run_protocol_tests(
            bus,
            tests,
            is_fd=is_fd,
            bitrate_switch=bitrate_switch,
            timeout_s=args.timeout,
        )

    raise SystemExit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
