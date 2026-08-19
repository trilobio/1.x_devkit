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
    parser.add_argument(
        "port",
        type=int,
        choices=(0, 1, 2),
        help="0=GPIO port A, 1=GPIO port B, 2=LEDs",
    )
    parser.add_argument("pin", type=int, choices=range(16), help="GPIO pin index")
    parser.add_argument("state", type=int, choices=(0, 1), help="0=LOW, 1=HIGH")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--client-port", type=int, default=8084)
    args = parser.parse_args()

    if args.port == 2 and args.pin > 2:
        parser.error("LED pin must be 0, 1, or 2 (LED1, LED2, or LED3)")

    # The arb-message consumer expects the public API port encoding directly:
    #   port=0 for GPIOA, port=1 for GPIOB, port=2 for LEDs, then pin and state.
    # LED pins are zero-based: pin=0 is LED1, pin=1 is LED2, pin=2 is LED3.
    payload = bytes((args.port, args.pin, args.state))
    floats = bytes_to_floats(payload)

    c = CANBusClient(host=args.host, port=args.client_port)
    response = c.write_encoder_calibration(
        args.board_id,
        0,
        float(len(payload)),
        0.0,
        floats,
    )
    print(f"Error code 1 means success!: \n{response}")


if __name__ == "__main__":
    main()
