#!/usr/bin/env python3
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Finds the lists a port has to join: places that name a reference platform but not the new one.

A platform or an architecture lives in the engine as entries in many lists: #if ladders, make
conditionals, arrays of names. A port forgets some of them. This script takes a supported reference
and the new name, and lists every line that mentions the reference with no mention of the new name
within a few lines around it:

    utils/porting/find-refs.py --like riscv64 --new loongarch64
    utils/porting/find-refs.py --like SPRT_NUTTX --new SPRT_MYOS --new SPRT_HOSTED_RTOS
    utils/porting/find-refs.py --like NUTTX --new MYOS --dirs make runtime/toolchains

Tokens match as whole words, but underscores may touch them (riscv64 matches riscv64_sprt).
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--like", action="append", required=True, help="a token of the reference (repeatable)")
    ap.add_argument("--new", action="append", default=[], help="a token of the new port (repeatable); also any "
                    "group macro that already covers it")
    ap.add_argument("--window", type=int, default=12, help="lines around a reference that count as nearby")
    ap.add_argument("--dirs", nargs="*", help="directories or files to scan (default: the code, make and CI)")
    ap.add_argument("--docs", action="store_true", help="also scan docs/ and *.adoc/*.md files")
    ap.add_argument("--covered", action="store_true", help="also list the references that are already covered")
    ap.add_argument("--case", action="store_true", help="do not add upper/lower case variants of the tokens")
    pl.add_common_args(ap)
    args = ap.parse_args()
    eng = pl.Engine(args.engine)

    def variants(tokens):
        out = set(tokens)
        if not args.case:
            out |= {t.upper() for t in tokens} | {t.lower() for t in tokens}
        return sorted(out)

    like, new = variants(args.like), variants(args.new)
    rep = pl.make_report(args, "references to %s without %s" % (" / ".join(like), " / ".join(new) or "anything new"))
    missing, covered = pl.scan_refs(eng, like, new, window=args.window, docs=args.docs, dirs=args.dirs)
    rep.section("missing", "%d line(s) in %d file(s) name the reference only"
                % (len(missing), len(pl.group_by_file(missing))))
    for path, lines in sorted(pl.group_by_file(missing).items()):
        rep.warn("%s: %d line(s)" % (path, len(lines)), details=["%5d  %s" % (n, t) for n, t in lines])
    if args.covered:
        rep.section("covered", "%d line(s) already have the new name nearby" % len(covered))
        for path, lines in sorted(pl.group_by_file(covered).items()):
            rep.ok("%s: %d line(s)" % (path, len(lines)), details=["%5d  %s" % (n, t) for n, t in lines])
    rep.section("note", "")
    rep.info("each line is a candidate: decide per list whether the new port belongs in it")
    rep.finish()
    return 0


if __name__ == "__main__":
    sys.exit(main())
