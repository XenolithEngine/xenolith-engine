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

"""Turn the UCA conformance files into a compact table for runtime_collation_conformance.

    ./gen_collation_test.py [<path-to-icu4c>] [--out <dir>]

Input is the two official conformance sets from the same icu4c checkout the
collation tables come from:

    source/test/testdata/CollationTest_NON_IGNORABLE_SHORT.txt   206 298 sequences
    source/test/testdata/CollationTest_SHIFTED_SHORT.txt         227 809 sequences

Each file is a list of code point sequences in sorted order, one per line, and
the contract is simply that each line sorts at or after the one before it. The
first is for alternate=non-ignorable (the CLDR default), the second for
alternate=shifted; those are the two settings that change what a *primary*
difference is, which is why they get their own files.

The encoding is prefix-delta plus varints, which is what makes 434 000 sequences
fit in 1.3 MB instead of 3.7. Consecutive lines of a sorted list share long
prefixes - that is the whole reason the file is hard for a collator - so each
record stores only how many code points it shares with the line before and the
ones that differ:

    byte 0:  shared << 4 | new       (both are at most 5; a line is at most 5 cps)
    then `new` varints:  0xxxxxxx                     for cp <= 0x7F
                         10xxxxxx xxxxxxxx            for cp <= 0x3FFF
                         11xxxxxx xxxxxxxx xxxxxxxx   for the rest

The decoder is in the test, and it has to walk the records in order - which it
does anyway, since every record is compared against its predecessor.
"""

import argparse
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))

MAX_SEQUENCE_LENGTH = 15  # what the 4-bit fields allow


def read_sequences(path):
	sequences = []
	version = None
	with open(path, encoding="utf-8") as f:
		for line in f:
			if version is None and "UCA Version:" in line:
				version = line.split(":", 1)[1].strip()
			line = line.split("#")[0].strip()
			if not line or line.startswith("@"):
				continue
			try:
				cps = [int(c, 16) for c in line.split()]
			except ValueError:
				continue
			if not cps:
				continue
			if len(cps) > MAX_SEQUENCE_LENGTH:
				raise SystemExit("%s: a sequence of %d code points does not fit the encoding"
						% (os.path.basename(path), len(cps)))
			sequences.append(cps)
	if not sequences:
		raise SystemExit("no sequences in " + path)
	return sequences, version


def encode(sequences):
	out = bytearray()
	previous = []
	for cps in sequences:
		shared = 0
		while (shared < len(previous) and shared < len(cps)
				and previous[shared] == cps[shared]):
			shared += 1
		new = len(cps) - shared
		out.append((shared << 4) | new)
		for cp in cps[shared:]:
			if cp <= 0x7F:
				out.append(cp)
			elif cp <= 0x3FFF:
				out.append(0x80 | (cp >> 8))
				out.append(cp & 0xFF)
			else:
				out.append(0xC0 | (cp >> 16))
				out.append((cp >> 8) & 0xFF)
				out.append(cp & 0xFF)
		previous = cps
	return out


def decode(blob):
	"""The reference decoder, so the generator can check its own output."""
	sequences = []
	previous = []
	i = 0
	while i < len(blob):
		header = blob[i]
		i += 1
		shared = header >> 4
		new = header & 0xF
		cps = previous[:shared]
		for _ in range(new):
			b = blob[i]
			i += 1
			if b < 0x80:
				cps.append(b)
			elif b < 0xC0:
				cps.append(((b & 0x3F) << 8) | blob[i])
				i += 1
			else:
				cps.append(((b & 0x3F) << 16) | (blob[i] << 8) | blob[i + 1])
				i += 2
		sequences.append(cps)
		previous = cps
	return sequences


LICENSE = """/**
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
// GENERATED FILE - do not edit. Produced by tests/runtime/tools/gen_collation_test.py
// from the UCA conformance sets in the icu4c checkout.
//
// Each entry is prefix-delta coded against the one before it; see the generator
// for the format and collation_conformance.cpp for the decoder."""


def emit_bytes(out, name, blob):
	out.append("static constexpr uint8_t %s[%d] = {" % (name, len(blob)))
	for i in range(0, len(blob), 24):
		out.append("\t" + ",".join(str(b) for b in blob[i:i + 24]) + ",")
	out[-1] = out[-1][:-1]
	out.append("};")
	out.append("")


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("icu4c", nargs="?",
			default=os.path.join(_ROOT, "runtime", "toolchains", "src", "icu4c"))
	parser.add_argument("--out", default=os.path.join(_HERE, "..", "runtime", "data"))
	args = parser.parse_args()

	testdata = os.path.join(args.icu4c, "source", "test", "testdata")
	files = [
		("NonIgnorable", "CollationTest_NON_IGNORABLE_SHORT.txt"),
		("Shifted", "CollationTest_SHIFTED_SHORT.txt"),
	]

	out = [LICENSE, "", "///@ SP_EXCLUDE", "", "#pragma once", "", "namespace sprt {", ""]
	version = None
	for name, filename in files:
		path = os.path.join(testdata, filename)
		if not os.path.exists(path):
			raise SystemExit("no " + path)
		sequences, fileVersion = read_sequences(path)
		if version is None:
			version = fileVersion
		elif fileVersion != version:
			raise SystemExit("%s is UCA %s but the other file is %s"
					% (filename, fileVersion, version))
		blob = encode(sequences)
		if decode(blob) != sequences:
			raise SystemExit("%s: the encoding does not round-trip" % filename)
		print("%-14s %7d sequences -> %d bytes" % (name, len(sequences), len(blob)))
		out.append("// %s: %d sequences, in %d bytes." % (filename, len(sequences), len(blob)))
		emit_bytes(out, "s_ucaTest" + name, blob)
		out.append("static constexpr int32_t s_ucaTest%sCount = %d;" % (name, len(sequences)))
		out.append("")

	out.append("static constexpr char s_ucaTestVersion[] = \"%s\";" % version)
	out.append("")
	out.append("} // namespace sprt")

	path = os.path.join(args.out, "collation_test.cc")
	text = "\n".join(out).rstrip("\n") + "\n"
	with open(path, "w", encoding="utf-8") as f:
		f.write(text)
	print("wrote %s (%d bytes)" % (path, len(text)))


if __name__ == "__main__":
	main()
