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

"""Regenerate the Unicode case-mapping tables.

    ./gen-case-tables.py [<path-to-icu4c>] [--out <dir>]

Input is `source/common/ucase_props_data.h` from the icu4c checkout that
src.mk clones into `runtime/toolchains/src/icu4c` - the same tree the UTS-46
tables come from. Output is `SPRuntimeUnicodeCaseData.cc` in this directory.

Unlike the normalization data next door, nothing here has to be taken apart:
since ICU 64 the case properties ship as a generated C header with plain arrays
and no `udata` container, so this script renames them and reads the UTrie2
descriptor out of `ucase_props_singleton`. It does NOT write that descriptor
down: all three UTrie2 descriptors in the UTS-46 tables changed between ICU 70
and 78, and a hardcoded indexLength survives a version bump silently, then
returns a valid value for the wrong code point.

Two of the four ICU arrays are deliberately not carried:

  ucase_props_unfold   reverse full case folding, read only by ucase_addCaseClosure
                       (UnicodeSet); the case mapper does not implement closure.
  ucase_props_indexes  the array lengths, which are checked here at generation
                       and then have nothing left to say at run time.

The helpers are imported from the UTS-46 generator rather than copied - in
particular parse_utrie2(), whose invariants are the whole point of this step.
"""

import argparse
import importlib.util
import os
import re
import sys

# The one place the two generators are coupled. Importing rather than copying
# keeps a single implementation of the UTrie2 descriptor checks.
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", ".."))
_IDN_GEN = os.path.join(_ROOT, "runtime", "src", "idn", "data", "gen-tables.py")


def _load_idn_gen():
	spec = importlib.util.spec_from_file_location("idn_gen_tables", _IDN_GEN)
	if spec is None or spec.loader is None:
		raise SystemExit("cannot load the UTS-46 generator at " + _IDN_GEN)
	mod = importlib.util.module_from_spec(spec)
	# Do not drop a __pycache__ into the source tree for a one-shot tool.
	previous = sys.dont_write_bytecode
	sys.dont_write_bytecode = True
	try:
		spec.loader.exec_module(mod)
	finally:
		sys.dont_write_bytecode = previous
	return mod


_idn = _load_idn_gen()
read_array = _idn.read_array
read_version = _idn.read_version
parse_utrie2 = _idn.parse_utrie2
emit_array = _idn.emit_array
emit_utrie2 = _idn.emit_utrie2

# ucase.h `enum { UCASE_IX_INDEX_TOP, UCASE_IX_LENGTH, UCASE_IX_TRIE_SIZE,
# UCASE_IX_EXC_LENGTH, UCASE_IX_UNFOLD_LENGTH, ..., UCASE_IX_TOP=16 }`.
IX_EXC_LENGTH = 3
IX_UNFOLD_LENGTH = 4
IX_TOP = 16

# The only ucase layout this generator understands. The bit layouts the port
# transcribes (props word, exception slots) are NOT described by the data, so a
# format bump means re-reading ucase.h by hand - see README.adoc.
UCASE_FORMAT_VERSION = 4

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

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2016, International Business Machines Corporation and others.
// All Rights Reserved.
//
// GENERATED FILE - do not edit.  Produced by runtime/src/unicode/data/gen-case-tables.py
// from the icu4c checkout in runtime/toolchains/src/icu4c, which carries the
// tables ICU builds from the Unicode Character Database.  See data/README.adoc
// to regenerate."""


def read_idn_unicode_version(root):
	"""The Unicode version the UTS-46 tables were generated from, or None."""
	path = os.path.join(root, "runtime", "src", "idn", "data", "SPRuntimeIdnDataNorm.cc")
	if not os.path.isfile(path):
		return None
	text = open(path, encoding="utf-8").read()
	m = re.search(r"s_unicodeVersion\[3\]\s*=\s*\{([^}]*)\}", text)
	if not m:
		return None
	return tuple(int(tok.strip(), 0) for tok in m.group(1).split(","))


def main():
	ap = argparse.ArgumentParser(description=__doc__)
	ap.add_argument("icu4c", nargs="?",
			default=os.path.join(_ROOT, "runtime", "toolchains", "src", "icu4c"),
			help="path to an icu4c checkout")
	ap.add_argument("--out", default=_HERE, help="output directory (default: %s)" % _HERE)
	args = ap.parse_args()

	src = os.path.join(args.icu4c, "source", "common", "ucase_props_data.h")
	if not os.path.isfile(src):
		raise SystemExit("no ucase data at " + src)
	text = open(src, encoding="utf-8").read()

	version = read_version(text, "ucase_props_dataVersion")
	trie_index = read_array(text, "ucase_props_trieIndex")
	exceptions = read_array(text, "ucase_props_exceptions")
	unfold = read_array(text, "ucase_props_unfold")
	indexes = read_array(text, "ucase_props_indexes")
	trie = parse_utrie2(text, "ucase_props_trieIndex", len(trie_index))

	# What the generator refuses to guess at.
	if len(indexes) != IX_TOP:
		raise SystemExit("ucase_props_indexes has %d entries, expected UCASE_IX_TOP=%d"
				% (len(indexes), IX_TOP))
	if indexes[IX_EXC_LENGTH] != len(exceptions):
		raise SystemExit("exceptions: indexes say %d, array holds %d"
				% (indexes[IX_EXC_LENGTH], len(exceptions)))
	if indexes[IX_UNFOLD_LENGTH] != len(unfold):
		raise SystemExit("unfold: indexes say %d, array holds %d"
				% (indexes[IX_UNFOLD_LENGTH], len(unfold)))

	m = re.search(r"ucase_props_singleton\s*=\s*\{.*?\},\s*\{\s*([0-9]+)\s*,", text, re.S)
	if not m:
		raise SystemExit("ucase_props_singleton: no formatVersion")
	if int(m.group(1)) != UCASE_FORMAT_VERSION:
		raise SystemExit("ucase formatVersion %s, this generator and "
				"runtime/src/unicode/case_props.cc implement %d"
				% (m.group(1), UCASE_FORMAT_VERSION))

	# A version skew here would mean the case mapper and the UTS-46 engine
	# disagree about which Unicode they implement, in the same binary.
	idn_version = read_idn_unicode_version(_ROOT)
	if idn_version is not None and idn_version != version:
		raise SystemExit("Unicode version mismatch: ucase is %d.%d.%d but the UTS-46 "
				"tables are %d.%d.%d - regenerate both from one icu4c checkout"
				% (version + idn_version))

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `source/common/ucase_props_data.h` (Unicode %d.%d.%d), the case"
			% version)
	out.append("// properties: a UTrie2 over the 16-bit props word plus the variable-length")
	out.append("// exceptions the props word points into for anything a delta cannot express.")
	out.append("//")
	out.append("// Not carried: ucase_props_unfold (%d entries, reverse full case folding - only"
			% len(unfold))
	out.append("// ucase_addCaseClosure reads it, and case closure is not ported) and")
	out.append("// ucase_props_indexes (array lengths, checked at generation).")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::unicode::detail {")
	out.append("")
	out.append("// Unicode version the tables were built from, as major.minor.patch.")
	out.append("static constexpr uint8_t s_caseUnicodeVersion[3] = {%d, %d, %d};" % version)
	out.append("")
	out.append("// UTrie2: index and 16-bit data live in one array, data starting at this offset.")
	emit_utrie2(out, "s_case", trie)
	emit_array(out, "uint16_t", "s_caseTrieIndex", trie_index)
	out.append("// Addressed by (props >> UCASE_EXC_SHIFT) when the props word has the")
	out.append("// exception bit: the mappings that do not fit in a delta.")
	emit_array(out, "uint16_t", "s_caseExceptions", exceptions)
	out.append("} // namespace sprt::unicode::detail")

	path = os.path.join(args.out, "SPRuntimeUnicodeCaseData.cc")
	_idn.write(path, out)
	print("Unicode %d.%d.%d; %d B of data (trie %d + exceptions %d)"
			% (version + (2 * (len(trie_index) + len(exceptions)),
					2 * len(trie_index), 2 * len(exceptions))))
	return 0


if __name__ == "__main__":
	sys.exit(main())
