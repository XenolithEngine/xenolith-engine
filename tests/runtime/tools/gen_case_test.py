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

"""Turn the UCD case-mapping data into a C++ table.

    ./gen_case_test.py [<path-to-icu4c>] [-o <out.cc>]

Input is the UCD copy in the icu4c checkout that runtime/toolchains/src.mk clones -
the same tree runtime/src/idn/data/gen-tables.py builds the engine's tables from, so
the suite and the tables cannot drift to different Unicode versions.

Three tables come out:

  s_caseSimple       - UnicodeData.txt columns 12/13/14, the simple 1:1 mappings. This
                       is what sprt::unicode::{tolower,toupper,totitle}(char32_t) does.
  s_caseFull         - SpecialCasing.txt unconditional rows, the 1:N mappings
                       (ß -> SS, ﬁ -> FI, ...) as UTF-8. This is what the
                       StringView/WideStringView overloads do.
  s_caseConditional  - SpecialCasing.txt CONDITIONAL rows, with their condition text.

A conditional row cannot be checked from the table alone: the rule only fires for a
particular language, or when the surrounding characters are right, so the input has
to be a purpose-built string rather than the single character in the file. Those
strings are hand-written in unicode.cpp. What the table is for is making sure none
is forgotten - the test asserts that every condition here is claimed by a vector
there, so a rule added to a future UCD fails rather than passing unnoticed.

CaseFolding.txt is deliberately NOT generated: nothing in the current API exposes
case folding, so there would be nothing to compare against.

Generated C++ rather than a data file read at run time, for the same reason as
gen_idna_test.py: there is no fixture plumbing, and no story for getting a data file
onto an Android device, into a wasm sandbox or next to a Wine-hosted .exe.
"""

import argparse
import os
import re
import sys


def cpp_string(s):
	"""A `const char *`, length pair for `s`, as UTF-8 escapes so the file stays ASCII."""
	out = []
	for b in s.encode('utf-8'):
		if b == 0x22:
			out.append('\\"')
		elif b == 0x5C:
			out.append('\\\\')
		elif 0x20 <= b < 0x7F:
			out.append(chr(b))
		else:
			out.append('\\x%02x""' % b)  # "" so a following hex digit is not absorbed
	return '"%s", %d' % (''.join(out), len(s.encode('utf-8')))


def read_version(path, stem):
	"""Pull `X.Y.Z` out of the leading `# <stem>-X.Y.Z.txt` line."""
	with open(path, encoding='utf-8') as f:
		m = re.match(r'#\s*' + re.escape(stem) + r'-(\d+)\.(\d+)\.(\d+)\.txt', f.readline())
	if not m:
		raise SystemExit('%s: no version in the first line' % path)
	return tuple(int(g) for g in m.groups())


def parse_unicode_data(path):
	"""(cp, lower, upper, title) for every code point with a simple case mapping."""
	rows = []
	for line in open(path, encoding='utf-8'):
		cols = line.split(';')
		if len(cols) < 15:
			continue
		cp = int(cols[0], 16)
		upper = int(cols[12], 16) if cols[12].strip() else cp
		lower = int(cols[13], 16) if cols[13].strip() else cp
		title = int(cols[14], 16) if cols[14].strip() else upper
		if (lower, upper, title) != (cp, cp, cp):
			rows.append((cp, lower, upper, title))
	return rows


def parse_special_casing(path):
	"""(src, lower, title, upper) rows, split into unconditional and conditional.

	The conditional ones carry their 5th column verbatim: it is the condition list,
	a language id and/or a context rule ("lt More_Above", "Final_Sigma").
	"""
	rows = []
	conditional = []
	for line in open(path, encoding='utf-8'):
		line = line.split('#')[0].strip()
		if not line:
			continue
		cols = [c.strip() for c in line.rstrip(';').split(';')]
		if len(cols) < 4:
			continue

		def chars(field):
			return ''.join(chr(int(c, 16)) for c in field.split())

		row = (chars(cols[0]), chars(cols[1]), chars(cols[2]), chars(cols[3]))
		if len(cols) > 4 and cols[4]:
			conditional.append(row + (cols[4],))
		else:
			rows.append(row)
	return rows, conditional


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
// GENERATED FILE - do not edit. Produced by tests/runtime/tools/gen_case_test.py
// from the Unicode Character Database (UnicodeData.txt, SpecialCasing.txt).'''


def main():
	here = os.path.dirname(os.path.abspath(__file__))
	root = os.path.abspath(os.path.join(here, '..', '..', '..'))
	ap = argparse.ArgumentParser(description=__doc__)
	ap.add_argument('icu4c', nargs='?',
			default=os.path.join(root, 'runtime/toolchains/src/icu4c'))
	ap.add_argument('-o', '--output',
			default=os.path.join(here, '..', 'runtime', 'data', 'case_test.cc'))
	args = ap.parse_args()

	unidata = os.path.join(args.icu4c, 'source', 'data', 'unidata')
	if not os.path.isdir(unidata):
		raise SystemExit('no UCD files at ' + unidata)

	version = read_version(os.path.join(unidata, 'SpecialCasing.txt'), 'SpecialCasing')
	simple = parse_unicode_data(os.path.join(unidata, 'UnicodeData.txt'))
	full, conditional = parse_special_casing(os.path.join(unidata, 'SpecialCasing.txt'))

	out = [LICENSE]
	out.append('// Unicode %d.%d.%d: %d simple mappings, %d unconditional full mappings,'
			% (version + (len(simple), len(full))))
	out.append('// %d conditional ones. The conditional rows are listed rather than checked'
			% len(conditional))
	out.append('// directly: each needs a purpose-built input string, written by hand in')
	out.append('// unicode.cpp, and this table is what makes sure none of them is forgotten.')
	out.append('')
	out.append('///@ SP_EXCLUDE')
	out.append('')
	out.append('#pragma once')
	out.append('')
	out.append('namespace sprt {')
	out.append('')
	out.append('static constexpr uint8_t s_caseUcdVersion[3] = {%d, %d, %d};' % version)
	out.append('')
	out.append('// UnicodeData.txt columns 12/13/14. Only code points that map somewhere.')
	out.append('struct CaseSimpleEntry {')
	out.append('\tchar32_t cp;')
	out.append('\tchar32_t lower;')
	out.append('\tchar32_t upper;')
	out.append('\tchar32_t title;')
	out.append('};')
	out.append('')
	out.append('static constexpr CaseSimpleEntry s_caseSimple[] = {')
	for cp, lo, up, ti in simple:
		out.append('\t{0x%04X, 0x%04X, 0x%04X, 0x%04X},' % (cp, lo, up, ti))
	out.append('};')
	out.append('')
	out.append('// SpecialCasing.txt, unconditional rows only. UTF-8; a plain pointer and')
	out.append('// length rather than a StringView so the table folds without running')
	out.append('// thousands of constexpr constructors.')
	out.append('struct CaseFullString {')
	out.append('\tconst char *data;')
	out.append('\tuint16_t size;')
	out.append('')
	out.append('\tconstexpr operator StringView() const { return StringView(data, size); }')
	out.append('};')
	out.append('')
	out.append('struct CaseFullEntry {')
	out.append('\tCaseFullString source;')
	out.append('\tCaseFullString lower;')
	out.append('\tCaseFullString title;')
	out.append('\tCaseFullString upper;')
	out.append('};')
	out.append('')
	out.append('static constexpr CaseFullEntry s_caseFull[] = {')
	for src, lo, ti, up in full:
		out.append('\t{{%s}, {%s}, {%s}, {%s}},'
				% (cpp_string(src), cpp_string(lo), cpp_string(ti), cpp_string(up)))
	out.append('};')
	out.append('')
	out.append('// SpecialCasing.txt, conditional rows. `condition` is the file\'s own 5th')
	out.append('// column: a language id, a context rule, or both. The mappings are here for')
	out.append('// reference; what the test uses is the condition, to check it is covered.')
	out.append('struct CaseConditionalEntry {')
	out.append('\tCaseFullString source;')
	out.append('\tCaseFullString lower;')
	out.append('\tCaseFullString title;')
	out.append('\tCaseFullString upper;')
	out.append('\tCaseFullString condition;')
	out.append('};')
	out.append('')
	out.append('static constexpr CaseConditionalEntry s_caseConditional[] = {')
	for src, lo, ti, up, cond in conditional:
		out.append('\t{{%s}, {%s}, {%s}, {%s}, {%s}},'
				% (cpp_string(src), cpp_string(lo), cpp_string(ti), cpp_string(up),
						cpp_string(cond)))
	out.append('};')
	out.append('')
	out.append('} // namespace sprt')

	os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
	with open(args.output, 'w', encoding='utf-8') as f:
		f.write('\n'.join(out) + '\n')
	print('wrote %s: Unicode %d.%d.%d, %d simple, %d full, %d conditional'
			% ((args.output,) + version + (len(simple), len(full), len(conditional))))
	return 0


if __name__ == '__main__':
	sys.exit(main())
