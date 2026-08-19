#!/usr/bin/env python3
"""Read a powered-deck GPIO state through the recycled READ_PROBE command."""

from __future__ import annotations

import argparse

from can_elegans import CANBusClient


def main() -> None:
    parser = argparse.ArgumentParser(description="Read a powered-deck GPIO state")
    parser.add_argument("board_id", type=int)
    parser.add_argument("port", type=int, choices=(0, 1), help="0=PORT_A, 1=PORT_B")
    parser.add_argument("pin", type=int, choices=range(16), help="GPIO pin index")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--client-port", type=int, default=8084)
    args = parser.parse_args()

    selector = (args.port << 4) | args.pin
    c = CANBusClient(host=args.host, port=args.client_port)
    response = c.read_probe(args.board_id, selector)
    print("HIGH" if response.contact else "LOW")


if __name__ == "__main__":
    main()
