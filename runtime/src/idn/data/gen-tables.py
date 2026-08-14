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

Input is a libuidna checkout (https://github.com/SBKarr/libuidna), which carries
the ICU tables as C arrays.  Usage:

    ./gen-tables.py <path-to-libuidna> [--out <dir>]

The default input path is the toolchain checkout that src.mk clones:
    runtime/toolchains/src/libuidna

Three of the four outputs are a straight copy of an ICU array with a new name.
The fourth, the normalization data, is NOT copied verbatim: upstream ships the
serialized ICU `uts46.nrm` container as one `uint8_t` blob and takes it apart at
run time with reinterpret_cast.  That cannot be done in a constant expression, so
this script takes it apart here instead and emits the pieces the normalizer
actually reads - two `uint16_t` trie arrays, the extra data, the small-FCD table
and the scalar thresholds - as typed, `constexpr` objects.  The engine therefore
has no run-time initialization, no endianness assumption and no error path for
the data, and the container header and padding do not reach the binary.

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

# --- UCPTrie constants (u_trie.h) --------------------------------------------

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
// from https://github.com/SBKarr/libuidna, which carries the tables ICU builds
// from the Unicode Character Database.  See data/README.adoc to regenerate.
"""


def read_array(text, name):
	"""Pull one `... name[N]={ ... };` initializer out of a C source file."""
	m = re.search(re.escape(name) + r"\s*\[\s*(\d*)\s*\]\s*=\s*\{", text)
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


def emit_array(out, ctype, name, values, fmt="0x%x", per_line=16):
	out.append("static constexpr %s %s[%d] = {" % (ctype, name, len(values)))
	for i in range(0, len(values), per_line):
		chunk = values[i:i + per_line]
		out.append("\t" + ", ".join(fmt % v for v in chunk) + ",")
	out[-1] = out[-1][:-1]  # drop the trailing comma of the last row
	out.append("};")
	out.append("")


def parse_ucptrie(blob):
	"""Take apart a serialized UCPTrie (fast, 16-bit values).

	Mirrors ucptrie_openFromBinary() in libuidna src/u_trie.cc.
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


def gen_norm(src_dir, out_dir):
	text = open(os.path.join(src_dir, "u_uts46data.cc"), encoding="utf-8").read()
	blob = bytes(read_array(text, "uts46_data"))

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
	if info_size < 20 or data_format != b"Nrm2" or format_version[0] != 4:
		raise SystemExit("uts46.nrm: unexpected data format")

	body = blob[header_size:]
	indexes_count = struct.unpack_from("<i", body, 0)[0] // 4
	if indexes_count <= IX_MIN_LCCC_CP:
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
	out.append("// Source: ICU `uts46.nrm` (Unicode %d.%d.%d), the combined UTS-46 mapping and"
			% (data_version[0], data_version[1], data_version[2]))
	out.append("// NFC composition data.  The serialized container has been taken apart by the")
	out.append("// generator; what follows is exactly what Normalizer2 reads.")
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
			("s_normMinMaybeYes", IX_MIN_MAYBE_YES)):
		out.append("static constexpr uint16_t %s = 0x%x;" % (name, indexes[ix]))
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
	return data_version


def gen_props(src_dir, out_dir):
	text = open(os.path.join(src_dir, "u_char.cc"), encoding="utf-8").read()
	index = read_array(text, "propsTrie_index")

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `uprops.icu`, the General_Category column.  UTS-46 tests exactly")
	out.append("// one thing against it - whether a label starts with a combining mark - but the")
	out.append("// trie is shared with nothing else, so it is carried whole.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	out.append("// UTrie2: index and 16-bit data live in one array, data starting at this offset.")
	out.append("static constexpr int32_t s_charTypeTrieIndexLength = 4656;")
	out.append("static constexpr int32_t s_charTypeTrieDataLength = 18032;")
	out.append("static constexpr char32_t s_charTypeTrieHighStart = 0x110000;")
	out.append("static constexpr int32_t s_charTypeTrieHighValueIndex = 0x589c;")
	out.append("")
	emit_array(out, "uint16_t", "s_charTypeTrieIndex", index)
	out.append("} // namespace sprt::idn::detail")

	write(os.path.join(out_dir, "SPRuntimeIdnDataProps.cc"), out)


def gen_script(src_dir, out_dir):
	text = open(os.path.join(src_dir, "u_char.cc"), encoding="utf-8").read()
	index = read_array(text, "propsVectorsTrie_index")
	vectors = read_array(text, "propsVectors")
	extensions = read_array(text, "scriptExtensions")

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `uprops.icu`, the property-vectors trie and the Script_Extensions")
	out.append("// table.  Read only by the CheckContextO rules (RFC 5892 appendix A.3, A.6 and")
	out.append("// A.7), which need the Script property of a code point.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	out.append("static constexpr int32_t s_scriptTrieIndexLength = 5188;")
	out.append("static constexpr int32_t s_scriptTrieDataLength = 26872;")
	out.append("static constexpr char32_t s_scriptTrieHighStart = 0x110000;")
	out.append("static constexpr int32_t s_scriptTrieHighValueIndex = 0x7d38;")
	out.append("// propsVectors is a row-major table; column 0 carries the Script bit field.")
	out.append("static constexpr int32_t s_scriptVectorColumns = 3;")
	out.append("")
	emit_array(out, "uint16_t", "s_scriptTrieIndex", index)
	emit_array(out, "uint32_t", "s_scriptVectors", vectors)
	emit_array(out, "uint16_t", "s_scriptExtensions", extensions)
	out.append("} // namespace sprt::idn::detail")

	write(os.path.join(out_dir, "SPRuntimeIdnDataScript.cc"), out)


def gen_bidi(src_dir, out_dir):
	text = open(os.path.join(src_dir, "u_bidi.cc"), encoding="utf-8").read()
	index = read_array(text, "ubidi_props_trieIndex")

	out = [LICENSE]
	out.append("//")
	out.append("// Source: ICU `ubidi.icu`.  Carries both the Bidi_Class (for the IDNA Bidi Rule,")
	out.append("// RFC 5893) and the Joining_Type (for CheckContextJ) of every code point; the")
	out.append("// mirroring and joining-group tables ICU ships alongside are not read by UTS-46")
	out.append("// and are not carried here.")
	out.append("")
	out.append("///@ SP_EXCLUDE")
	out.append("")
	out.append("#pragma once")
	out.append("")
	out.append("namespace sprt::idn::detail {")
	out.append("")
	out.append("static constexpr int32_t s_bidiTrieIndexLength = 3612;")
	out.append("static constexpr int32_t s_bidiTrieDataLength = 9264;")
	out.append("static constexpr char32_t s_bidiTrieHighStart = 0x110000;")
	out.append("static constexpr int32_t s_bidiTrieHighValueIndex = 0x3248;")
	out.append("")
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
		os.path.join(here, "..", "..", "..", "toolchains", "src", "libuidna"))

	ap = argparse.ArgumentParser(description=__doc__)
	ap.add_argument("libuidna", nargs="?", default=default_src,
			help="path to a libuidna checkout (default: %s)" % default_src)
	ap.add_argument("--out", default=here, help="output directory (default: %s)" % here)
	args = ap.parse_args()

	src_dir = os.path.join(args.libuidna, "src")
	if not os.path.isdir(src_dir):
		raise SystemExit("no libuidna sources at " + src_dir)

	version = gen_norm(src_dir, args.out)
	gen_props(src_dir, args.out)
	gen_script(src_dir, args.out)
	gen_bidi(src_dir, args.out)
	print("Unicode %d.%d.%d" % (version[0], version[1], version[2]))
	return 0


if __name__ == "__main__":
	sys.exit(main())
