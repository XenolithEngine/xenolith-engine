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

"""Turn the Unicode IdnaTestV2.txt conformance file into a C++ table.

    ./gen_idna_test.py [IdnaTestV2.txt] [-o <out.cc>]

The default input is the copy in the icu4c checkout that
runtime/toolchains/src.mk clones - the same tree runtime/src/idn/data/gen-tables.py
builds the engine's tables from, so the suite and the tables cannot drift to
different Unicode versions; the default output is
tests/runtime/runtime/data/idna_test_v2.cc.

Why generated C++ rather than a data file read at run time: tests/runtime has no
fixture plumbing (LOCAL_SRCS_DIRS is source directories only), and there is no
story for getting a data file onto an Android device, into a wasm sandbox or next
to a Wine-hosted .exe. Parsing the escape syntax at run time would also be a second
implementation that can itself be wrong, which is exactly what a conformance test
must not have.

Rows whose source contains an unpaired surrogate are dropped: the engine's entry
point takes UTF-8 bytes, which cannot carry one, so those rows test something this
API cannot express.
"""

import argparse
import os
import re
import sys

# What a status column means.
#
# The letter codes ([B5 B6], [P1 V6], ...) are informational: ICU's own conformance
# driver (icu4c source/test/intltest/uts46test.cpp, checkIdnaTestResult) does NOT map them to
# error codes. It asserts exactly two things - whether the operation was expected to
# fail, and, when it was not, that the output string matches. Anything more would be
# asserting a rule attribution the file does not actually specify, which is how a
# "conformance" test ends up testing the test.
#
# Which rule fired IS pinned, per case, in tests/runtime/runtime/idn.cpp, where each
# expectation was checked against the reference implementation by hand.


def unescape(s):
	def rep(m):
		return chr(int(m.group(1) or m.group(2), 16))
	return re.sub(r'\\u([0-9A-Fa-f]{4})|\\x\{([0-9A-Fa-f]+)\}', rep, s)


def cpp_string(s):
	"""A `const char *`, length pair for `s`, as UTF-8 escapes so the file stays ASCII.

	Deliberately NOT a StringView: a constexpr table of 6000+ rows x 4 views makes the
	compiler evaluate that many constructors, which blows the -fconstexpr-steps limit.
	A plain pointer and length costs nothing to fold and the test wraps them at use.
	"""
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


def expects_error(field):
	"""True when the status column marks the operation as expected to fail."""
	field = field.strip()
	return bool(field) and field != '[]'


def is_root_label_only(field):
	"""True when the status column is exactly [A4_2] - the empty root label rule.

	Since the 2025 revision the file marks a trailing dot (the empty root label) as an
	A4_2 VerifyDnsLength error; there are 712 such rows where the 2021 file had 10.
	ICU does not report it, and neither does this engine, so ICU's own driver clears
	the expectation for exactly this case (uts46test.cpp checkIdnaTestResult, "ICU
	workaround", ICU-22882). The condition also needs the actual result, which only
	the C++ side has, so the flag is carried per row and applied there.
	"""
	return field.strip() == '[A4_2]'


def main():
	here = os.path.dirname(os.path.abspath(__file__))
	root = os.path.abspath(os.path.join(here, '..', '..', '..'))
	ap = argparse.ArgumentParser(description=__doc__)
	ap.add_argument('input', nargs='?',
			default=os.path.join(root,
					'runtime/toolchains/src/icu4c/source/test/testdata/IdnaTestV2.txt'))
	ap.add_argument('-o', '--output',
			default=os.path.join(here, '..', 'runtime', 'data', 'idna_test_v2.cc'))
	args = ap.parse_args()

	version = ''
	rows = []
	dropped = 0
	for line in open(args.input, encoding='utf-8'):
		if not version and line.startswith('# Date:'):
			version = line[1:].strip()
		line = line.split('#')[0].strip()
		if not line:
			continue
		cols = [c.strip() for c in line.split(';')]
		if len(cols) < 7:
			continue
		try:
			source = unescape(cols[0])
			to_unicode = unescape(cols[1]) or source
			to_ascii_n = unescape(cols[3]) or to_unicode
			to_ascii_t = unescape(cols[5]) or to_ascii_n
			source.encode('utf-8')
			to_unicode.encode('utf-8')
			to_ascii_n.encode('utf-8')
			to_ascii_t.encode('utf-8')
		except UnicodeEncodeError:
			dropped += 1
			continue

		st_u = expects_error(cols[2])
		st_n = expects_error(cols[4]) if cols[4].strip() else st_u
		st_t = expects_error(cols[6]) if cols[6].strip() else st_n
		# A blank toAsciiT status means "same as toAsciiN", which for the empty-root
		# rule means the same status string, not just the same boolean.
		root_n = is_root_label_only(cols[4])
		root_t = is_root_label_only(cols[6]) if cols[6].strip() else root_n
		rows.append((source, to_unicode, st_u, to_ascii_n, st_n, root_n,
				to_ascii_t, st_t, root_t))

	out = []
	out.append('''/**
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

// © 2021 Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
// GENERATED FILE - do not edit. Produced by tests/runtime/tools/gen_idna_test.py
// from the Unicode IdnaTestV2.txt conformance data.''')
	out.append('// Source: %s' % version)
	out.append('// %d rows; %d dropped for containing an unpaired surrogate.' % (len(rows), dropped))
	out.append('')
	out.append('///@ SP_EXCLUDE')
	out.append('')
	out.append('#pragma once')
	out.append('')
	out.append('namespace sprt {')
	out.append('')

	def flag(v):
		return 'true' if v else 'false'

	entries = []
	for source, tu, su, tan, sn, rn, tat, st, rt in rows:
		entries.append('\t{{%s}, {%s}, {%s}, {%s}, %s, %s, %s, %s, %s},'
				% (cpp_string(source), cpp_string(tu), cpp_string(tan), cpp_string(tat),
						flag(su), flag(sn), flag(st), flag(rn), flag(rt)))

	out.append('struct IdnaTestString {')
	out.append('\tconst char *data;')
	out.append('\tuint16_t size;')
	out.append('')
	out.append('\tconstexpr operator StringView() const { return StringView(data, size); }')
	out.append('};')
	out.append('')
	out.append('struct IdnaTestCase {')
	out.append('\tIdnaTestString source;')
	out.append('\tIdnaTestString toUnicode;')
	out.append('\tIdnaTestString toAsciiN; // nontransitional')
	out.append('\tIdnaTestString toAsciiT; // transitional')
	out.append('\t// Whether the standard expects the operation to fail. Which rule it')
	out.append('\t// blames is not specified by this file - see the note in the generator.')
	out.append('\tbool unicodeFails;')
	out.append('\tbool asciiNFails;')
	out.append('\tbool asciiTFails;')
	out.append('\t// Status was exactly [A4_2]: the only expected error is the empty root')
	out.append('\t// label, which neither ICU nor this engine reports. See the generator.')
	out.append('\tbool asciiNRootLabelOnly;')
	out.append('\tbool asciiTRootLabelOnly;')
	out.append('};')
	out.append('')
	out.append('static constexpr IdnaTestCase s_idnaTestCases[] = {')
	out.extend(entries)
	out.append('};')
	out.append('')
	out.append('} // namespace sprt')

	os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
	with open(args.output, 'w', encoding='utf-8') as f:
		f.write('\n'.join(out) + '\n')
	print('wrote %s: %d cases, %d dropped' % (args.output, len(rows), dropped))
	return 0


if __name__ == '__main__':
	sys.exit(main())
