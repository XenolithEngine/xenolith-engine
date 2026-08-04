#!/usr/bin/env python3
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
"""Frame driver for the rasterizer benchmark.

Why this is not xlclient.py: that client captures a settled frame, and to do so it
asks for `frame` with a count. The inspector implements a count by calling
setReadyForNextFrame() in a loop, and that is a flag rather than a queue - N calls
before the loop has produced anything still yield ONE frame. Fine for a capture,
useless for a benchmark, which needs N frames to actually be rasterized.

So this driver asks for one frame at a time and gives the loop a moment to run it.
The pause has to exceed the frame time or the requests coalesce again - at 1080p a
scalar frame is tens of milliseconds, so the default is deliberately generous. It does
not verify the count itself: the engine reports `frames=` in its own profile line, and
the benchmark reads that. If the two disagree, the profile is right.

Usage: benchclient.py --address unix:/tmp/xl.sock --frames 240
                      [--invoke NAME=JSON]... [--interval SECONDS] [--quit]
"""

import argparse
import json
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xlclient import Session, open_session  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", required=True)
    parser.add_argument("--invoke", action="append", default=[], metavar="NAME=JSON")
    parser.add_argument("--frames", type=int, default=240)
    parser.add_argument("--interval", type=float, default=0.025,
                        help="pause between frame requests, seconds (default 0.025)")
    parser.add_argument("--wait", type=float, default=20.0)
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--quit", action="store_true")
    args = parser.parse_args()

    session = open_session(args.address, args.wait, args.timeout)
    try:
        for spec in args.invoke:
            name, _, payload = spec.partition("=")
            session.call("invoke", name=name, args=json.loads(payload) if payload else {})

        # Settle first: the scene commands above dirty the graph, and the frames that
        # apply them are not the frames we want to time.
        for _ in range(4):
            session.call("frame", count=1)
            time.sleep(0.01)

        for _ in range(args.frames):
            session.call("frame", count=1)
            if args.interval > 0:
                time.sleep(args.interval)

        if args.quit:
            session.call("quit")
    finally:
        session.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
