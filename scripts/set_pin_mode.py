#!/usr/bin/env python3
"""Set a powered-deck GPIO mode through the recycled SET_LIGHT_LEVELS command."""

from __future__ import annotations

import argparse

from can_elegans import CANBusClient


def main() -> None:
    parser = argparse.ArgumentParser(description="Set a powered-deck GPIO mode")
    parser.add_argument("board_id", type=int)
    parser.add_argument("port", type=int, choices=(0, 1), help="0=PORT_A, 1=PORT_B")
    parser.add_argument("pin", type=int, choices=range(16), help="GPIO pin index")
    parser.add_argument("mode", type=int, choices=(0, 1), help="0=input, 1=output")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--client-port", type=int, default=8084)
    args = parser.parse_args()

    c = CANBusClient(host=args.host, port=args.client_port)
    response = c.set_light_levels(args.board_id, args.port, args.pin, args.mode, 0)
    print(response)


if __name__ == "__main__":
    main()
