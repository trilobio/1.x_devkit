#!/usr/bin/env python3
"""Set a powered-deck GPIO state through the existing arb-message tunnel."""

from __future__ import annotations

import argparse

from can_elegans import CANBusClient


BYTES_PER_FLOAT = 3


def bytes_to_floats(payload: bytes) -> list[float]:
    """Pack each three-byte little-endian group into an exact-integer float."""
    return [
        float(int.from_bytes(payload[i : i + BYTES_PER_FLOAT], "little"))
        for i in range(0, len(payload), BYTES_PER_FLOAT)
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description="Set a powered-deck GPIO state")
    parser.add_argument("board_id", type=int)
    parser.add_argument("port", type=int, choices=(0, 1), help="0=PORT_A, 1=PORT_B")
    parser.add_argument("pin", type=int, choices=range(16), help="GPIO pin index")
    parser.add_argument("state", type=int, choices=(0, 1), help="0=LOW, 1=HIGH")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--client-port", type=int, default=8084)
    args = parser.parse_args()

    # The arb-message consumer expects:
    #   group=1 for PORT_A, group=2 for PORT_B, then pin and state.
    payload = bytes((args.port + 1, args.pin, args.state))
    floats = bytes_to_floats(payload)

    c = CANBusClient(host=args.host, port=args.client_port)
    response = c.write_encoder_calibration(
        args.board_id,
        0,
        float(len(payload)),
        0.0,
        floats,
    )
    print(response)


if __name__ == "__main__":
    main()
