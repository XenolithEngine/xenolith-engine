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

"""Turn the Unicode WordBreakTest.txt conformance file into a C++ table.

    ./gen_wordbreak_test.py [WordBreakTest.txt] [-o <out.cc>]

The default input is the copy in the icu4c checkout that
runtime/toolchains/src.mk clones - the same tree
runtime/src/unicode/data/gen-case-tables.py builds the Word_Break table from, so
the suite and the table cannot drift to different Unicode versions. The default
output is tests/runtime/runtime/data/wordbreak_test.cc.

Each line is a sequence of code points separated by U+00F7 DIVISION SIGN where a
boundary is expected and U+00D7 MULTIPLICATION SIGN where one is not, with a
divider at each end. What comes out is the text itself plus the offsets of the
expected boundaries, both in UTF-16 code units, because that is what the
runtime's iterator works on.

Note that this file is the standard, not ICU: ICU's own break iterator adds
dictionary breaking and locale tailorings on top of UAX #29 and does not pass all
of it (rbbitst.cpp carries a skip list). The runtime implements plain UAX #29, so
every case here must pass.

Generated C++ rather than a data file read at run time, for the same reasons as
gen_idna_test.py: there is no fixture plumbing in tests/runtime, and no story for
getting a data file onto an Android device, into a wasm sandbox or next to a
Wine-hosted .exe.
"""

import argparse
import os
import re
import sys

BREAK = '÷'
NO_BREAK = '×'


def read_version(path):
    """Pull `X.Y.Z` out of the leading `# WordBreakTest-X.Y.Z.txt` line."""
    with open(path, encoding='utf-8') as f:
        m = re.match(r'#\s*WordBreakTest-(\d+)\.(\d+)\.(\d+)\.txt', f.readline())
    if not m:
        raise SystemExit('%s: no version in the first line' % path)
    return tuple(int(g) for g in m.groups())


def parse(path):
    """(code points, expected boundaries in UTF-16 units) per test line."""
    cases = []
    for lineno, line in enumerate(open(path, encoding='utf-8'), 1):
        line = line.split('#')[0].strip()
        if not line:
            continue
        tokens = line.split()
        if not tokens:
            continue
        if tokens[0] != BREAK or tokens[-1] != BREAK:
            raise SystemExit('%s:%d: a case must start and end with a boundary' % (path, lineno))

        units = []
        bounds = []
        expect_divider = True
        for tok in tokens:
            if tok in (BREAK, NO_BREAK):
                if not expect_divider:
                    raise SystemExit('%s:%d: two dividers in a row' % (path, lineno))
                if tok == BREAK:
                    bounds.append(len(units))
                expect_divider = False
            else:
                if expect_divider:
                    raise SystemExit('%s:%d: two code points in a row' % (path, lineno))
                cp = int(tok, 16)
                if cp > 0x10FFFF:
                    raise SystemExit('%s:%d: %04X is not a code point' % (path, lineno, cp))
                if 0xD800 <= cp <= 0xDFFF:
                    raise SystemExit('%s:%d: %04X is a surrogate' % (path, lineno, cp))
                if cp < 0x10000:
                    units.append(cp)
                else:
                    cp -= 0x10000
                    units.append(0xD800 + (cp >> 10))
                    units.append(0xDC00 + (cp & 0x3FF))
                expect_divider = True
        if bounds[0] != 0 or bounds[-1] != len(units):
            raise SystemExit('%s:%d: boundaries do not span the text' % (path, lineno))
        cases.append((units, bounds))
    return cases


LICENSE = '''/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

// © 2025 Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
// GENERATED FILE - do not edit. Produced by tests/runtime/tools/gen_wordbreak_test.py
// from the Unicode Character Database (WordBreakTest.txt).'''


def emit_pool(out, ctype, name, values, per_line):
    out.append('static constexpr %s %s[%d] = {' % (ctype, name, len(values)))
    for i in range(0, len(values), per_line):
        out.append('\t' + ', '.join('0x%x' % v for v in values[i:i + per_line]) + ',')
    out[-1] = out[-1][:-1]
    out.append('};')
    out.append('')


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, '..', '..', '..'))
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', nargs='?',
            default=os.path.join(root,
                    'runtime/toolchains/src/icu4c/source/test/testdata/WordBreakTest.txt'))
    ap.add_argument('-o', '--output',
            default=os.path.join(here, '..', 'runtime', 'data', 'wordbreak_test.cc'))
    args = ap.parse_args()

    if not os.path.isfile(args.input):
        raise SystemExit('no WordBreakTest.txt at ' + args.input)
    version = read_version(args.input)
    cases = parse(args.input)

    # One pool per array rather than an array per case: 1944 separate
    # definitions is a lot of symbols for the linker to carry around for no gain.
    text_pool = []
    bounds_pool = []
    entries = []
    for units, bounds in cases:
        entries.append('\t{%d, %d, %d, %d},'
                % (len(text_pool), len(units), len(bounds_pool), len(bounds)))
        text_pool.extend(units)
        bounds_pool.extend(bounds)

    if len(text_pool) > 0xFFFF or len(bounds_pool) > 0xFFFF:
        raise SystemExit('the pools no longer fit the uint16_t offsets')

    out = [LICENSE]
    out.append('//')
    out.append('// Unicode %d.%d.%d: %d cases, %d code units of text.'
            % (version + (len(cases), len(text_pool))))
    out.append('')
    out.append('///@ SP_EXCLUDE')
    out.append('')
    out.append('#pragma once')
    out.append('')
    out.append('namespace sprt {')
    out.append('')
    out.append('static constexpr uint8_t s_wordBreakUcdVersion[3] = {%d, %d, %d};' % version)
    out.append('')
    out.append('// Offsets into the two pools below. The text is UTF-16 and the boundaries are')
    out.append('// offsets into it in code units, which is what the runtime iterator reports.')
    out.append('struct WordBreakTestCase {')
    out.append('\tuint16_t textOffset;')
    out.append('\tuint16_t textLength;')
    out.append('\tuint16_t boundsOffset;')
    out.append('\tuint16_t boundsCount;')
    out.append('};')
    out.append('')
    emit_pool(out, 'char16_t', 's_wordBreakTestText', text_pool, 16)
    emit_pool(out, 'uint16_t', 's_wordBreakTestBounds', bounds_pool, 24)
    out.append('static constexpr WordBreakTestCase s_wordBreakTestCases[] = {')
    out.extend(entries)
    out.append('};')
    out.append('')
    out.append('} // namespace sprt')

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')
    print('wrote %s: Unicode %d.%d.%d, %d cases, %d code units'
            % ((args.output,) + version + (len(cases), len(text_pool))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
