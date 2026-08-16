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

Input is four files from the icu4c checkout that src.mk clones into
`runtime/toolchains/src/icu4c` - the same tree the UTS-46 tables come from.
Output is `SPRuntimeUnicodeCaseData.cc` in this directory.

  source/common/ucase_props_data.h  the case properties trie and exceptions
  source/common/ucase.cpp           LatinCase, the ASCII/Latin-1 fast path
  source/common/ustrcase.cpp        GreekUpper, the Greek uppercasing data
  source/common/ucase.h,
  source/common/ucasemap_imp.h      the constants the two above are written in

Unlike the normalization data next door, the properties do not have to be taken
apart: since ICU 64 they ship as a generated C header with plain arrays and no
`udata` container, so this script renames them and reads the UTrie2 descriptor
out of `ucase_props_singleton`. It does NOT write that descriptor down: all three
UTrie2 descriptors in the UTS-46 tables changed between ICU 70 and 78, and a
hardcoded indexLength survives a version bump silently, then returns a valid
value for the wrong code point.

LatinCase and GreekUpper live in ICU source files rather than in generated data,
and their entries are written as expressions (`EXC`, `0x0391 | HAS_VOWEL`), so
they are read with a small expression evaluator whose names come from ucase.h and
ucasemap_imp.h. They are read rather than copied for the same reason the trie
descriptor is: they move with the Unicode version, and a transcription slip in a
delta table is not a crash - it is a handful of characters cased wrong.

Two of the four ICU property arrays are deliberately not carried:

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
NOT_IDENT = _idn.NOT_IDENT


def read_constants(text, names):
	"""Read `... NAME = <integer expression>;` definitions out of a C++ header.

	Used for the constants LatinCase and GreekUpper are written in. Values may
	refer to constants read earlier, which is why `names` is both the list of
	what to look for and the namespace the expressions are evaluated in.
	"""
	out = {}
	for name in names:
		m = re.search(NOT_IDENT + re.escape(name) + r"\s*=\s*([^;]+);", text)
		if not m:
			raise SystemExit("constant not found: " + name)
		out[name] = _eval(m.group(1), out)
	return out


def _eval(expr, names):
	"""Evaluate one C integer expression. Only `|` and literals occur in practice;
	the restricted namespace is what keeps this honest if that ever changes."""
	try:
		return eval(expr.strip(), {"__builtins__": {}}, dict(names))
	except Exception as e:
		raise SystemExit("cannot evaluate %r: %s" % (expr, e))


def read_expr_array(text, name, names):
	"""read_array() for initializers whose entries are expressions, not literals."""
	m = re.search(NOT_IDENT + re.escape(name) + r"\s*\[\s*([^\]]*?)\s*\]\s*=\s*\{", text)
	if not m:
		raise SystemExit("array not found: " + name)
	start = m.end()
	end = text.index("};", start)
	# Strip // comments; the ICU arrays carry range headers and block labels.
	body = re.sub(r"//[^\n]*", "", text[start:end])
	values = [_eval(tok, names) for tok in body.split(",") if tok.strip()]
	declared = m.group(1)
	if declared.isdigit() and int(declared) != len(values):
		raise SystemExit("%s: declared %s entries, parsed %d" % (name, declared, len(values)))
	return values

# ucase.h `enum { UCASE_IX_INDEX_TOP, UCASE_IX_LENGTH, UCASE_IX_TRIE_SIZE,
# UCASE_IX_EXC_LENGTH, UCASE_IX_UNFOLD_LENGTH, ..., UCASE_IX_TOP=16 }`.
IX_EXC_LENGTH = 3
IX_UNFOLD_LENGTH = 4
IX_TOP = 16

# The only ucase layout this generator understands. The bit layouts the port
# transcribes (props word, exception slots) are NOT described by the data, so a
# format bump means re-reading ucase.h by hand - see README.adoc.
UCASE_FORMAT_VERSION = 4

# ucase.h namespace LatinCase: four linear delta tables covering U+0000..LIMIT-1.
LATIN_CONSTANTS = ["LIMIT", "LONG_S", "EXC"]
LATIN_ARRAYS = [
	("TO_LOWER_NORMAL", "s_caseLatinToLowerNormal"),
	("TO_LOWER_TR_LT", "s_caseLatinToLowerTrLt"),
	("TO_UPPER_NORMAL", "s_caseLatinToUpperNormal"),
	("TO_UPPER_TR", "s_caseLatinToUpperTr"),
]

# ucasemap_imp.h namespace GreekUpper. Only the bits that ENCODE the data are
# read here; the ones ustrcase.cpp adds while processing (HAS_COMBINING_DIALYTIKA,
# HAS_OTHER_GREEK_DIACRITIC, AFTER_*) belong to the algorithm, not the table, and
# are declared in case_string.cc.
GREEK_CONSTANTS = ["UPPER_MASK", "HAS_VOWEL", "HAS_YPOGEGRAMMENI", "HAS_ACCENT", "HAS_DIALYTIKA"]

# The two ranges ustrcase.cpp's getLetterData() covers, plus the lone Ohm sign.
GREEK_RANGES = [("data0370", "s_caseGreekData0370", 0x370, 0x400),
		("data1F00", "s_caseGreekData1F00", 0x1F00, 0x2000)]

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


def gen_latin(out, icu_common):
	"""LatinCase from ucase.h + ucase.cpp: the ASCII/Latin-1 fast path.

	Four arrays of signed byte deltas indexed by the code point itself, with EXC
	marking the ones that need the full mapping. Two tables per direction, because
	tr/az/lt differ from every other locale below U+0180.
	"""
	head = open(os.path.join(icu_common, "ucase.h"), encoding="utf-8").read()
	body = open(os.path.join(icu_common, "ucase.cpp"), encoding="utf-8").read()

	# The constants are inside `namespace LatinCase`, and TO_LOWER_NORMAL etc. are
	# declared in the header too - cut both files down to the namespace body so a
	# same-named constant elsewhere in ICU cannot be picked up instead.
	consts = read_constants(_latin_scope(head), LATIN_CONSTANTS)
	limit, exc = consts["LIMIT"], consts["EXC"]
	if exc != -0x80:
		raise SystemExit("LatinCase::EXC is %d; the port distinguishes it from a real "
				"delta by that value" % exc)
	if not 0 < consts["LONG_S"] < limit:
		raise SystemExit("LatinCase::LONG_S=%#x is outside [0, LIMIT=%#x)"
				% (consts["LONG_S"], limit))

	scope = _latin_scope(body)
	out.append("// LatinCase (ICU ucase.cpp): case mapping deltas for U+0000..U+%04X, the range"
			% (limit - 1))
	out.append("// the string mappers walk without touching the trie. EXC means \"not a delta:")
	out.append("// ask the full mapping\". Separate tables for tr/az/lt, which differ here.")
	out.append("static constexpr char32_t s_caseLatinLimit = 0x%x;" % limit)
	out.append("static constexpr char32_t s_caseLatinLongS = 0x%x;" % consts["LONG_S"])
	out.append("static constexpr int8_t s_caseLatinExc = %d;" % exc)
	out.append("")
	for icu_name, name in LATIN_ARRAYS:
		values = read_expr_array(scope, icu_name, consts)
		if len(values) != limit:
			raise SystemExit("LatinCase::%s holds %d entries, LIMIT is %d"
					% (icu_name, len(values), limit))
		if any(v < -0x80 or v > 0x7F for v in values):
			raise SystemExit("LatinCase::%s has an entry outside int8_t" % icu_name)
		emit_array(out, "int8_t", name, values, fmt="%d")


def _latin_scope(text):
	"""The body of `namespace LatinCase { ... }`."""
	start = text.index("namespace LatinCase")
	return text[start:text.index("}  // namespace LatinCase", start)]


def gen_greek(out, icu_common):
	"""GreekUpper from ucasemap_imp.h + ustrcase.cpp.

	One uint16_t per code point in two ranges: the uppercase letter in the low
	bits, plus flags saying whether it is a vowel and what it carries. Uppercasing
	Greek drops the tonos and moves the dialytika, which no per-character mapping
	can express, so this table feeds a state machine rather than a lookup.
	"""
	head = open(os.path.join(icu_common, "ucasemap_imp.h"), encoding="utf-8").read()
	body = open(os.path.join(icu_common, "ustrcase.cpp"), encoding="utf-8").read()

	consts = read_constants(_greek_scope(head, "}  // namespace GreekUpper"), GREEK_CONSTANTS)
	scope = _greek_scope(body, "}  // namespace GreekUpper")

	out.append("// GreekUpper (ICU ustrcase.cpp): the uppercase form of each Greek letter plus")
	out.append("// the flags the uppercasing state machine needs. Only the bits that encode the")
	out.append("// table are here; the ones added while processing live in case_string.cc.")
	for name in GREEK_CONSTANTS:
		out.append("static constexpr uint32_t s_caseGreek%s = 0x%x;"
				% (_camel(name), consts[name]))
	out.append("")
	for icu_name, name, first, past in GREEK_RANGES:
		values = read_expr_array(scope, icu_name, consts)
		if len(values) != past - first:
			raise SystemExit("GreekUpper::%s holds %d entries, U+%04X..U+%04X is %d"
					% (icu_name, len(values), first, past - 1, past - first))
		if any(v < 0 or v > 0xFFFF for v in values):
			raise SystemExit("GreekUpper::%s has an entry outside uint16_t" % icu_name)
		out.append("// U+%04X..U+%04X" % (first, past - 1))
		emit_array(out, "uint16_t", name, values)

	m = re.search(NOT_IDENT + r"data2126\s*=\s*([^;]+);", scope)
	if not m:
		raise SystemExit("GreekUpper::data2126 not found")
	out.append("// U+2126 OHM SIGN, the one letter outside the two ranges above.")
	out.append("static constexpr uint16_t s_caseGreekData2126 = 0x%x;" % _eval(m.group(1), consts))
	out.append("")


def _greek_scope(text, closer):
	start = text.index("namespace GreekUpper")
	return text[start:text.index(closer, start)]


def _camel(name):
	"""UPPER_MASK -> UpperMask, for the emitted spelling."""
	return "".join(part.capitalize() for part in name.split("_"))


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

	icu_common = os.path.join(args.icu4c, "source", "common")
	src = os.path.join(icu_common, "ucase_props_data.h")
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
	out.append("// Source: ICU (Unicode %d.%d.%d), three tables:" % version)
	out.append("//")
	out.append("//   `ucase_props_data.h`  the case properties - a UTrie2 over the 16-bit props")
	out.append("//                         word plus the variable-length exceptions it points")
	out.append("//                         into for anything a delta cannot express;")
	out.append("//   `ucase.cpp`           LatinCase, the linear delta tables the string mappers")
	out.append("//                         use below U+0180 instead of the trie;")
	out.append("//   `ustrcase.cpp`        GreekUpper, the data behind Greek uppercasing.")
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

	before_latin = len(out)
	gen_latin(out, icu_common)
	gen_greek(out, icu_common)
	extra = _emitted_bytes(out[before_latin:])

	out.append("} // namespace sprt::unicode::detail")

	path = os.path.join(args.out, "SPRuntimeUnicodeCaseData.cc")
	_idn.write(path, out)
	props_bytes = 2 * (len(trie_index) + len(exceptions))
	print("Unicode %d.%d.%d; %d B of data (trie %d + exceptions %d + latin/greek %d)"
			% (version + (props_bytes + extra, 2 * len(trie_index), 2 * len(exceptions), extra)))
	return 0


def _emitted_bytes(lines):
	"""Size of the arrays in the emitted lines, for the summary the caller prints."""
	total = 0
	for line in lines:
		m = re.match(r"static constexpr (u?int(8|16)_t) \w+\[(\d+)\]", line)
		if m:
			total += int(m.group(3)) * (1 if m.group(1).endswith("8_t") else 2)
	return total


if __name__ == "__main__":
	sys.exit(main())
