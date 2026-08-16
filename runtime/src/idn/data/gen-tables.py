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

"""Regenerate the Unicode tables the UTS-46 engine needs.

Input is an icu4c checkout, which src.mk already clones into
`runtime/toolchains/src/icu4c`.  Usage:

    ./gen-tables.py [<path-to-icu4c>] [--out <dir>]

Everything comes straight out of the ICU tree, no intermediate extract:

    source/data/in/uts46.nrm        the UTS-46 mapping + NFC composition data
    source/common/uchar_props_data.h  General_Category, property vectors, Script_Extensions
    source/common/ubidi_props_data.h  Bidi_Class and Joining_Type

Three of the four outputs are a straight copy of an ICU array with a new name.
The fourth, the normalization data, is NOT copied verbatim: ICU ships the
serialized `uts46.nrm` container as an opaque blob and takes it apart at run time
with reinterpret_cast.  That cannot be done in a constant expression, so this
script takes it apart here instead and emits the pieces the normalizer actually
reads - two `uint16_t` trie arrays, the extra data, the small-FCD table and the
scalar thresholds - as typed, `constexpr` objects.  The engine therefore has no
run-time initialization, no endianness assumption and no error path for the data,
and the container header and padding do not reach the binary.

Every UTrie2 descriptor is read out of the ICU initializer next to its array, not
written down here: a hardcoded indexLength survives a version bump silently, and a
trie with the wrong descriptor returns a *valid* value for the wrong code point.

See README.adoc for how the ICU side of the pipeline works.
"""

import argparse
import os
import re
import struct
import sys

# --- offsets into the .nrm indexes array (Normalizer2Impl::IX_*) --------------

IX_NORM_TRIE_OFFSET = 0
IX_EXTRA_DATA_OFFSET = 1
IX_SMALL_FCD_OFFSET = 2
IX_TOTAL_SIZE = 7
IX_MIN_DECOMP_NO_CP = 8
IX_MIN_COMP_NO_MAYBE_CP = 9
IX_MIN_YES_NO = 10
IX_MIN_NO_NO = 11
IX_LIMIT_NO_NO = 12
IX_MIN_MAYBE_YES = 13
IX_MIN_YES_NO_MAPPINGS_ONLY = 14
IX_MIN_NO_NO_COMP_BOUNDARY_BEFORE = 15
IX_MIN_NO_NO_COMP_NO_MAYBE_CC = 16
IX_MIN_NO_NO_EMPTY = 17
IX_MIN_LCCC_CP = 18
# New in formatVersion 5: two-way mappings whose first character combines backward.
IX_MIN_MAYBE_NO = 20
IX_MIN_MAYBE_NO_COMBINES_FWD = 21

# The only .nrm layout this generator understands.  formatVersion 5 moved the
# maybeYes compositions into the ordinary extra data and addresses them through
# minMaybeNo, which changes what the normalizer must do with every norm16 above
# limitNoNo - so an older container is refused rather than silently mis-read.
NRM2_FORMAT_VERSION = 5

# --- UCPTrie constants (ICU ucptrie_impl.h) -----------------------------------

UCPTRIE_SIG = 0x54726933
UCPTRIE_TYPE_FAST = 0
UCPTRIE_VALUE_BITS_16 = 0
UCPTRIE_OPTIONS_DATA_LENGTH_MASK = 0xF000
UCPTRIE_OPTIONS_DATA_NULL_OFFSET_MASK = 0xF00
UCPTRIE_OPTIONS_RESERVED_MASK = 0x38
UCPTRIE_OPTIONS_VALUE_BITS_MASK = 7
UCPTRIE_SHIFT_2 = 9
UCPTRIE_HIGH_VALUE_NEG_DATA_OFFSET = 2

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
// GENERATED FILE - do not edit.  Produced by runtime/src/idn/data/gen-tables.py
// from the icu4c checkout in runtime/toolchains/src/icu4c, which carries the
// tables ICU builds from the Unicode Character Database.  See data/README.adoc
// to regenerate.
"""

# A name is only a match when it is not a suffix of a longer identifier:
# `propsTrie_index` must not be found inside `propsVectorsTrie_index`.
NOT_IDENT = r"(?<![A-Za-z0-9_])"


def read_array(text, name):
	"""Pull one `... name[N]={ ... };` initializer out of a C source file."""
	m = re.search(NOT_IDENT + re.escape(name) + r"\s*\[\s*(\d*)\s*\]\s*=\s*\{", text)
	if not m:
		raise SystemExit("array not found: " + name)
	start = m.end()
	end = text.index("};", start)
	body = text[start:end]
	values = []
	for tok in body.replace("\n", "").split(","):
		tok = tok.strip()
		if tok:
			values.append(int(tok, 0))
	declared = m.group(1)
	if declared and int(declared) != len(values):
		raise SystemExit(
			"%s: declared %s entries, parsed %d" % (name, declared, len(values)))
	return values


def read_int(text, name):
	"""Pull one `... name=VALUE;` scalar definition out of a C source file."""
	m = re.search(NOT_IDENT + re.escape(name) + r"\s*=\s*([0-9a-fA-Fx]+)\s*;", text)
	if not m:
		raise SystemExit("scalar not found: " + name)
	return int(m.group(1), 0)


def read_version(text, name):
	"""Pull one `UVersionInfo name={a,b,c,d};` out of a C source file."""
	m = re.search(NOT_IDENT + re.escape(name) + r"\s*=\s*\{([^}]*)\}", text)
	if not m:
		raise SystemExit("version not found: " + name)
	return tuple(int(tok.strip(), 0) for tok in m.group(1).split(",")[:3])


def parse_utrie2(text, index_name, array_length):
	"""Read the UTrie2 descriptor that ICU writes next to `index_name`.

	Located by the index array rather than by the struct name, so it works both
	for a standalone `static const UTrie2 x={...}` and for the trie nested inside
	a *_props_singleton initializer.  Field order is utrie2.h `struct UTrie2`:

	    index, data16, data32, indexLength, dataLength, index2NullOffset,
	    dataNullOffset, initialValue, errorValue, highStart, highValueIndex, ...
	"""
	m = re.search(
		NOT_IDENT + re.escape(index_name) + r"\s*,\s*"
		+ re.escape(index_name) + r"\s*\+\s*(\d+)\s*,", text)
	if not m:
		raise SystemExit("UTrie2 descriptor not found for: " + index_name)
	data16_offset = int(m.group(1))

	fields = [tok.strip() for tok in text[m.end():].split(",")[:9]]
	if fields[0] != "nullptr" and fields[0] != "NULL":
		raise SystemExit("%s: expected a 16-bit trie (data32 must be null)" % index_name)
	values = [int(tok, 0) for tok in fields[1:9]]
	trie = {
		"indexLength": values[0],
		"dataLength": values[1],
		"index2NullOffset": values[2],
		"dataNullOffset": values[3],
		"initialValue": values[4],
		"errorValue": values[5],
		"highStart": values[6],
		"highValueIndex": values[7],
	}

	# The runtime's constexpr Utrie2 reaches the data as `index + indexLength`
	# (uts46/trie.cc), which is only true because ICU serializes them into one
	# array in that order.  Both halves of that assumption are checked here.
	if data16_offset != trie["indexLength"]:
		raise SystemExit("%s: data16 is at +%d but indexLength is %d"
				% (index_name, data16_offset, trie["indexLength"]))
	if array_length != trie["indexLength"] + trie["dataLength"]:
		raise SystemExit("%s: array holds %d entries, descriptor says %d + %d"
				% (index_name, array_length, trie["indexLength"], trie["dataLength"]))
	return trie


def emit_array(out, ctype, name, values, fmt="0x%x", per_line=16):
	out.append("static constexpr %s %s[%d] = {" % (ctype, name, len(values)))
	for i in range(0, len(values), per_line):
		chunk = values[i:i + per_line]
		out.append("\t" + ", ".join(fmt % v for v in chunk) + ",")
	out[-1] = out[-1][:-1]  # drop the trailing comma of the last row
	out.append("};")
	out.append("")


def emit_utrie2(out, prefix, trie):
	out.append("static constexpr int32_t %sTrieIndexLength = %d;"
			% (prefix, trie["indexLength"]))
	out.append("static constexpr int32_t %sTrieDataLength = %d;" % (prefix, trie["dataLength"]))
	out.append("static constexpr char32_t %sTrieHighStart = 0x%x;" % (prefix, trie["highStart"]))
	out.append("static constexpr int32_t %sTrieHighValueIndex = 0x%x;"
			% (prefix, trie["highValueIndex"]))
	out.append("")


def parse_ucptrie(blob):
	"""Take apart a serialized UCPTrie (fast, 16-bit values).

	Mirrors ucptrie_openFromBinary() in ICU ucptrie.cpp.
	"""
	signature, options, index_length, data_length_low, index3_null_offset, \
		data_null_offset_low, shifted_high_start = struct.unpack_from("<IHHHHHH", blob, 0)
	if signature != UCPTRIE_SIG:
		raise SystemExit("normalization trie: bad signature 0x%08x" % signature)
	if (options >> 6) & 3 != UCPTRIE_TYPE_FAST:
		raise SystemExit("normalization trie: expected a fast trie")
	if options & UCPTRIE_OPTIONS_VALUE_BITS_MASK != UCPTRIE_VALUE_BITS_16:
		raise SystemExit("normalization trie: expected 16-bit values")
	if options & UCPTRIE_OPTIONS_RESERVED_MASK:
		raise SystemExit("normalization trie: reserved option bits set")

	data_length = ((options & UCPTRIE_OPTIONS_DATA_LENGTH_MASK) << 4) | data_length_low
	data_null_offset = \
		((options & UCPTRIE_OPTIONS_DATA_NULL_OFFSET_MASK) << 8) | data_null_offset_low
	high_start = shifted_high_start << UCPTRIE_SHIFT_2

	header_size = 16  # sizeof(UCPTrieHeader)
	index = list(struct.unpack_from("<%dH" % index_length, blob, header_size))
	data = list(struct.unpack_from(
		"<%dH" % data_length, blob, header_size + index_length * 2))

	null_value_offset = data_null_offset
	if null_value_offset >= data_length:
		null_value_offset = data_length - UCPTRIE_HIGH_VALUE_NEG_DATA_OFFSET

	return {
		"index": index,
		"data": data,
		"indexLength": index_length,
		"dataLength": data_length,
		"highStart": high_start,
		"shifted12HighStart": (high_start + 0xFFF) >> 12,
		"index3NullOffset": index3_null_offset,
		"dataNullOffset": data_null_offset,
		"nullValue": data[null_value_offset],
	}


def gen_norm(icu_dir, out_dir):
	path = os.path.join(icu_dir, "source", "data", "in", "uts46.nrm")
	with open(path, "rb") as f:
		blob = f.read()

	header_size, magic1, magic2 = struct.unpack_from("<HBB", blob, 0)
	if magic1 != 0xDA or magic2 != 0x27:
		raise SystemExit("uts46.nrm: bad udata magic")
	info_size, _reserved_word, is_big_endian, _charset, _sizeof_uchar, _reserved_byte = \
		struct.unpack_from("<HHBBBB", blob, 4)
	data_format = blob[12:16]
	format_version = blob[16:20]
	data_version = blob[20:24]
	if is_big_endian:
		raise SystemExit("uts46.nrm: big-endian data is not supported")
	if info_size < 20 or data_format != b"Nrm2":
		raise SystemExit("uts46.nrm: unexpected data format")
	if format_version[0] != NRM2_FORMAT_VERSION:
		raise SystemExit("uts46.nrm: Nrm2 formatVersion %d, this generator and "
				"uts46/normalizer.cc implement %d"
				% (format_version[0], NRM2_FORMAT_VERSION))

	body = blob[header_size:]
	indexes_count = struct.unpack_from("<i", body, 0)[0] // 4
	if indexes_count <= IX_MIN_MAYBE_NO_COMBINES_FWD:
		raise SystemExit("uts46.nrm: not enough indexes")
	indexes = list(struct.unpack_from("<%di" % indexes_count, body, 0))

	trie = parse_ucptrie(
		body[indexes[IX_NORM_TRIE_OFFSET]:indexes[IX_EXTRA_DATA_OFFSET]])

	extra_bytes = body[indexes[IX_EXTRA_DATA_OFFSET]:indexes[IX_SMALL_FCD_OFFSET]]
	extra = list(struct.unpack_from("<%dH" % (len(extra_bytes) // 2), extra_bytes, 0))
	small_fcd = list(body[indexes[IX_SMALL_FCD_OFFSET]:indexes[IX_TOTAL_SIZE]])
	if len(small_fcd) != 0x100:
		raise SystemExit("uts46.nrm: smallFCD is %d bytes, expected 256" % len(small_fcd))

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `source/data/in/uts46.nrm` (Unicode %d.%d.%d), the combined UTS-46"
			% (data_version[0], data_version[1], data_version[2]))
	out.append("// mapping and NFC composition data.  The serialized container has been taken")
	out.append("// apart by the generator; what follows is exactly what Normalizer2 reads.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	out.append("// Unicode version the tables were built from, as major.minor.patch.")
	out.append("static constexpr uint8_t s_unicodeVersion[3] = {%d, %d, %d};"
			% (data_version[0], data_version[1], data_version[2]))
	out.append("")
	out.append("// Code point thresholds and norm16 boundaries (the .nrm indexes array).")
	for name, ix in (
			("s_normMinDecompNoCp", IX_MIN_DECOMP_NO_CP),
			("s_normMinCompNoMaybeCp", IX_MIN_COMP_NO_MAYBE_CP),
			("s_normMinLcccCp", IX_MIN_LCCC_CP)):
		out.append("static constexpr char16_t %s = 0x%x;" % (name, indexes[ix]))
	for name, ix in (
			("s_normMinYesNo", IX_MIN_YES_NO),
			("s_normMinYesNoMappingsOnly", IX_MIN_YES_NO_MAPPINGS_ONLY),
			("s_normMinNoNo", IX_MIN_NO_NO),
			("s_normMinNoNoCompBoundaryBefore", IX_MIN_NO_NO_COMP_BOUNDARY_BEFORE),
			("s_normMinNoNoCompNoMaybeCc", IX_MIN_NO_NO_COMP_NO_MAYBE_CC),
			("s_normMinNoNoEmpty", IX_MIN_NO_NO_EMPTY),
			("s_normLimitNoNo", IX_LIMIT_NO_NO),
			("s_normMinMaybeNo", IX_MIN_MAYBE_NO),
			("s_normMinMaybeNoCombinesFwd", IX_MIN_MAYBE_NO_COMBINES_FWD),
			("s_normMinMaybeYes", IX_MIN_MAYBE_YES)):
		out.append("static constexpr uint16_t %s = 0x%x;" % (name, indexes[ix]))
	# mapAlgorithmic() packs the delta into norm16 bits above DELTA_SHIFT, which is
	# only exact while minMaybeNo is 8-aligned (ICU asserts the same in init()).
	if indexes[IX_MIN_MAYBE_NO] & 7:
		raise SystemExit("uts46.nrm: minMaybeNo 0x%x is not 8-aligned"
				% indexes[IX_MIN_MAYBE_NO])
	out.append("")
	out.append("// UCPTrie (fast, 16-bit values) over the norm16 values.")
	out.append("static constexpr int32_t s_normTrieIndexLength = %d;" % trie["indexLength"])
	out.append("static constexpr int32_t s_normTrieDataLength = %d;" % trie["dataLength"])
	out.append("static constexpr char32_t s_normTrieHighStart = 0x%x;" % trie["highStart"])
	out.append("static constexpr uint16_t s_normTrieShifted12HighStart = 0x%x;"
			% trie["shifted12HighStart"])
	out.append("static constexpr int32_t s_normTrieIndex3NullOffset = 0x%x;"
			% trie["index3NullOffset"])
	out.append("static constexpr int32_t s_normTrieDataNullOffset = 0x%x;"
			% trie["dataNullOffset"])
	out.append("static constexpr uint16_t s_normTrieNullValue = 0x%x;" % trie["nullValue"])
	out.append("")
	emit_array(out, "uint16_t", "s_normTrieIndex", trie["index"])
	emit_array(out, "uint16_t", "s_normTrieData", trie["data"])
	out.append("// Mapping and composition data addressed by the norm16 values above.")
	emit_array(out, "uint16_t", "s_normExtraData", extra)
	out.append("// One byte per 32 code points: the lccc/tccc quick test for FCD.")
	emit_array(out, "uint8_t", "s_normSmallFcd", small_fcd, fmt="0x%02x", per_line=16)
	out.append("} // namespace sprt::idn::detail")

	write(os.path.join(out_dir, "SPRuntimeIdnDataNorm.cc"), out)
	return tuple(data_version[:3])


def gen_props(text, out_dir):
	index = read_array(text, "propsTrie_index")
	trie = parse_utrie2(text, "propsTrie_index", len(index))

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `source/common/uchar_props_data.h`, the General_Category column.")
	out.append("// UTS-46 tests exactly one thing against it - whether a label starts with a")
	out.append("// combining mark - but the trie is shared with nothing else, so it is carried")
	out.append("// whole.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	out.append("// UTrie2: index and 16-bit data live in one array, data starting at this offset.")
	emit_utrie2(out, "s_charType", trie)
	emit_array(out, "uint16_t", "s_charTypeTrieIndex", index)
	out.append("} // namespace sprt::idn::detail")

	write(os.path.join(out_dir, "SPRuntimeIdnDataProps.cc"), out)


def gen_script(text, out_dir):
	index = read_array(text, "propsVectorsTrie_index")
	trie = parse_utrie2(text, "propsVectorsTrie_index", len(index))
	vectors = read_array(text, "propsVectors")
	extensions = read_array(text, "scriptExtensions")
	columns = read_int(text, "propsVectorsColumns")
	count = read_int(text, "countPropsVectors")
	if count != len(vectors):
		raise SystemExit("propsVectors: countPropsVectors says %d, array holds %d"
				% (count, len(vectors)))

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `source/common/uchar_props_data.h`, the property-vectors trie and")
	out.append("// the Script_Extensions table.  Read only by the CheckContextO rules (RFC 5892")
	out.append("// appendix A.3, A.6 and A.7), which need the Script property of a code point.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	emit_utrie2(out, "s_script", trie)
	out.append("// propsVectors is a row-major table; column 0 carries the Script bit field.")
	out.append("static constexpr int32_t s_scriptVectorColumns = %d;" % columns)
	out.append("")
	emit_array(out, "uint16_t", "s_scriptTrieIndex", index)
	emit_array(out, "uint32_t", "s_scriptVectors", vectors)
	emit_array(out, "uint16_t", "s_scriptExtensions", extensions)
	out.append("} // namespace sprt::idn::detail")

	write(os.path.join(out_dir, "SPRuntimeIdnDataScript.cc"), out)


def gen_bidi(text, out_dir):
	index = read_array(text, "ubidi_props_trieIndex")
	trie = parse_utrie2(text, "ubidi_props_trieIndex", len(index))

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `source/common/ubidi_props_data.h`.  Carries both the Bidi_Class")
	out.append("// (for the IDNA Bidi Rule, RFC 5893) and the Joining_Type (for CheckContextJ) of")
	out.append("// every code point; the mirroring and joining-group tables ICU ships alongside")
	out.append("// are not read by UTS-46 and are not carried here.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	emit_utrie2(out, "s_bidi", trie)
	emit_array(out, "uint16_t", "s_bidiTrieIndex", index)
	out.append("} // namespace sprt::idn::detail")

	write(os.path.join(out_dir, "SPRuntimeIdnDataBidi.cc"), out)


def write(path, lines):
	with open(path, "w", encoding="utf-8") as f:
		f.write("\n".join(lines))
		if not lines[-1].endswith("\n"):
			f.write("\n")
	print("wrote %s (%d bytes)" % (path, os.path.getsize(path)))


def main():
	here = os.path.dirname(os.path.abspath(__file__))
	default_src = os.path.abspath(
		os.path.join(here, "..", "..", "..", "toolchains", "src", "icu4c"))

	ap = argparse.ArgumentParser(description=__doc__)
	ap.add_argument("icu4c", nargs="?", default=default_src,
			help="path to an icu4c checkout (default: %s)" % default_src)
	ap.add_argument("--out", default=here, help="output directory (default: %s)" % here)
	args = ap.parse_args()

	common = os.path.join(args.icu4c, "source", "common")
	if not os.path.isdir(common):
		raise SystemExit("no icu4c sources at " + common)

	uchar = open(os.path.join(common, "uchar_props_data.h"), encoding="utf-8").read()
	ubidi = open(os.path.join(common, "ubidi_props_data.h"), encoding="utf-8").read()

	# A half-updated checkout is the failure this catches: the tables would each be
	# self-consistent and disagree with one another.
	versions = {
		"uts46.nrm": gen_norm(args.icu4c, args.out),
		"uchar_props_data.h": read_version(uchar, "dataVersion"),
		"ubidi_props_data.h": read_version(ubidi, "ubidi_props_dataVersion"),
	}
	if len(set(versions.values())) != 1:
		raise SystemExit("Unicode version mismatch across ICU tables: "
				+ ", ".join("%s=%d.%d.%d" % ((k,) + v) for k, v in versions.items()))

	gen_props(uchar, args.out)
	gen_script(uchar, args.out)
	gen_bidi(ubidi, args.out)
	print("Unicode %d.%d.%d" % versions["uts46.nrm"])
	return 0


if __name__ == "__main__":
	sys.exit(main())
