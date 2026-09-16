#!/usr/bin/env python3
"""
Compare two PNGs pixel by pixel for software-rasterizer / GPU parity.

The criterion is the last bit and nothing more, and it is applied to each channel on
its own - never to their sum: a result is accepted when every channel of every pixel
is off by at most --tolerance, which defaults to 1. How MANY pixels sit at that last
bit does not matter - a one-step difference is the smallest an 8-bit channel can
express, and reaching it everywhere still means the two implementations agree on the
value. A difference of 2 or more is a real divergence no matter how few pixels carry
it, which is why there is no share-of-pixels threshold: it would let a genuine error
through as long as it stayed rare.

--exact drops the tolerance to zero, for the cases that must be byte-identical:
solid fills, nearest-filtered axis-aligned sampling, and the two self-comparisons
(--glyph-paths, --baseline) where a single differing bit means a real defect.

Usage:
  imgdiff.py REFERENCE ACTUAL [--tolerance N] [--exact] [--out-diff PATH]

Exit status is 0 when the images match within the tolerance, 1 when they do not,
and 2 when they cannot be compared at all (missing file, size mismatch).
"""

import argparse
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError as e:  # pragma: no cover - environment problem, not a test failure
    print("imgdiff.py needs numpy and Pillow: %s" % e, file=sys.stderr)
    sys.exit(2)


def load(path):
    with Image.open(path) as img:
        return np.asarray(img.convert("RGBA"), dtype=np.int16)


def main():
    parser = argparse.ArgumentParser(add_help=True, description=__doc__)
    parser.add_argument("reference")
    parser.add_argument("actual")
    parser.add_argument("--tolerance", type=int, default=1,
            help="largest difference allowed in a single channel (default 1)")
    parser.add_argument("--exact", action="store_true",
            help="require a byte-identical match (tolerance 0)")
    parser.add_argument("--out-diff", help="write an amplified difference map here")
    args = parser.parse_args()

    tolerance = 0 if args.exact else args.tolerance

    try:
        ref = load(args.reference)
        act = load(args.actual)
    except (OSError, ValueError) as e:
        print("cannot read images: %s" % e, file=sys.stderr)
        return 2

    if ref.shape != act.shape:
        print("size mismatch: %s is %s, %s is %s"
                % (args.reference, ref.shape, args.actual, act.shape), file=sys.stderr)
        return 2

    delta = np.abs(ref - act)
    # max over the channel axis, so the test is per channel: a pixel offends when any
    # ONE of its channels exceeds the tolerance, never because several small ones add up
    per_pixel = delta.max(axis=2)
    total = per_pixel.size
    # every pixel that is not identical, and the subset that exceeds the tolerance -
    # the first number tells noise apart from a real divergence, the second decides
    touched = int(np.count_nonzero(per_pixel))
    offending = int(np.count_nonzero(per_pixel > tolerance))
    worst = int(per_pixel.max()) if total else 0

    if args.out_diff:
        # amplify so a 1/255 difference is actually visible
        amplified = np.clip(delta[:, :, :3].astype(np.int32) * 32, 0, 255).astype(np.uint8)
        Image.fromarray(amplified, mode="RGB").save(args.out_diff)

    # "0 differing pixels of 76 800, max channel delta 0" is the phrasing M0 reported
    print("%d differing pixels of %d, %d over tolerance %d, max channel delta %d"
            % (touched, total, offending, tolerance, worst))

    return 0 if offending == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
