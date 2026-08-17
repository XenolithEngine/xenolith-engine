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

"""Regenerate the CLDR collation tables.

Input is an icu4c checkout, which src.mk already clones into
`runtime/toolchains/src/icu4c`.  Usage:

    ./gen-collation-tables.py [<path-to-icu4c>] [--out <dir>]
    ./gen-collation-tables.py --report      # sizes only, writes nothing

Two things come out of ICU here, and neither is a Unicode data file:

    source/data/in/coll/ucadata-unihan.icu   the CLDR root collation
    source/data/in/icudt78l.dat              per-locale tailorings, as
                                             %%CollationBin inside coll/<loc>.res

Both are ICU *build output* in ICU's own serialization (`dataFormat="UCol"`,
formatVersion 5), which is why the format version is checked on every read: it
can change between ICU releases with no other visible sign.

What this script does that ICU does at run time (`CollationDataReader::read`):

  * splits the serialized container into its sections and emits them as typed
    `constexpr` arrays, so the engine has no header parsing, no allocation, no
    endianness assumption and no error path for the data;
  * computes the root collator's unsafe-backward set.  ICU builds it on load out
    of "every code point with a non-zero leading combining class", plus trail
    surrogates, plus the ranges in the file, plus every lead surrogate whose
    supplementary block contains one.  The lccc half comes from UnicodeData.txt
    here rather than from the normalizer, so the set is a constant - and so that
    the ported `getFCD16` can be checked against it later;
  * drops `rootElements` (28 KB).  Only the rule compiler reads it, and the rule
    compiler is not ported: tailorings arrive already built.

The text of the CLDR rules (`Sequence` in each resource bundle) is not carried
either, for the same reason.  It is more than half of the bundle - `coll/root.res`
is 342 KB of which the built root part is 32 bytes - and without a rule compiler
it is dead weight.

See README.adoc for the rest of the pipeline.
"""

import argparse
import importlib.util
import os
import struct
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", ".."))
_IDN_GEN = os.path.join(_ROOT, "runtime", "src", "idn", "data", "gen-tables.py")


def _load_idn_gen():
	spec = importlib.util.spec_from_file_location("idn_gen_tables", _IDN_GEN)
	if spec is None or spec.loader is None:
		raise SystemExit("cannot load the UTS-46 generator at " + _IDN_GEN)
	mod = importlib.util.module_from_spec(spec)
	previous = sys.dont_write_bytecode
	sys.dont_write_bytecode = True
	try:
		spec.loader.exec_module(mod)
	finally:
		sys.dont_write_bytecode = previous
	return mod


_idn = _load_idn_gen()
emit_array = _idn.emit_array


# --- the serialized collation container ---------------------------------------
#
# Offsets into indexes[], from CollationDataReader::IX_* (collationdatareader.h).
# Every one of them is a byte offset from the start of indexes[]; the length of a
# section is the next offset minus this one, which is why the names run in order
# and why RESERVED slots have to be kept.

IX_INDEXES_LENGTH = 0
IX_OPTIONS = 1
IX_RESERVED2 = 2
IX_RESERVED3 = 3
IX_JAMO_CE32S_START = 4
IX_REORDER_CODES_OFFSET = 5
IX_REORDER_TABLE_OFFSET = 6
IX_TRIE_OFFSET = 7
IX_RESERVED8_OFFSET = 8
IX_CES_OFFSET = 9
IX_RESERVED10_OFFSET = 10
IX_CE32S_OFFSET = 11
IX_ROOT_ELEMENTS_OFFSET = 12
IX_CONTEXTS_OFFSET = 13
IX_UNSAFE_BWD_OFFSET = 14
IX_FAST_LATIN_TABLE_OFFSET = 15
IX_SCRIPTS_OFFSET = 16
IX_COMPRESSIBLE_BYTES_OFFSET = 17
IX_RESERVED18_OFFSET = 18
IX_TOTAL_SIZE = 19

# CollationFastLatin::VERSION (collationfastlatin.h), checked both against the
# byte in indexes[IX_OPTIONS] and against the first word of the table itself.
FAST_LATIN_VERSION = 2

# Collation::MERGE_SEPARATOR_BYTE + 1 and Collation::TRAIL_WEIGHT_BYTE, the two
# scriptStarts sentinels CollationDataReader asserts on.
MERGE_SEPARATOR_BYTE = 0x02
TRAIL_WEIGHT_BYTE = 0xFF

UTRIE2_SIG = 0x54726932  # "Tri2"
UTRIE2_32_VALUE_BITS = 1  # UTrie2ValueBits: 16-bit is 0, 32-bit is 1
UTRIE2_INDEX_SHIFT = 2
UTRIE2_DATA_GRANULARITY = 1 << UTRIE2_INDEX_SHIFT


def uca_version_key(data_version):
	"""CollationTailoring::getUCAVersion - what the base/tailoring match is on.

	Only these ten bits are compared, so a tailoring built for the same UCA but a
	different CLDR release still loads.
	"""
	return (data_version[1] << 4) | (data_version[2] >> 6)


def uca_version_text(data_version):
	"""The same field, read as a Unicode version: major in the high 5 bits."""
	return "%d.%d" % (data_version[1] >> 3, data_version[1] & 7)


def read_udata_header(blob, expect_format, name):
	"""Strip and check a udata header (udata.h: DataHeader + UDataInfo)."""
	header_size, magic1, magic2 = struct.unpack_from("<HBB", blob, 0)
	if magic1 != 0xDA or magic2 != 0x27:
		raise SystemExit("%s: not a udata container" % name)
	(info_size, _reserved, is_big_endian, charset_family, sizeof_uchar,
			_reserved2) = struct.unpack_from("<HHBBBB", blob, 4)
	if is_big_endian or charset_family != 0 or sizeof_uchar != 2:
		raise SystemExit("%s: not little-endian ASCII UTF-16 data" % name)
	data_format = blob[12:16]
	format_version = tuple(blob[16:20])
	data_version = tuple(blob[20:24])
	if data_format != expect_format:
		raise SystemExit("%s: dataFormat is %r, expected %r"
				% (name, data_format, expect_format))
	return header_size, format_version, data_version, info_size


def parse_utrie2_32(blob, name):
	"""Take apart a serialized 32-bit-value UTrie2.

	Mirrors utrie2_openFromSerialized() in ICU utrie2.cpp for
	UTRIE2_32_VALUE_BITS.  The collation trie is the only 32-bit UTrie2 the
	runtime reads; the 16-bit ones next door go through the IDN generator.
	"""
	(signature, options, index_length, shifted_data_length, index2_null_offset,
			data_null_offset, shifted_high_start) = struct.unpack_from("<IHHHHHH", blob, 0)
	if signature != UTRIE2_SIG:
		raise SystemExit("%s: bad UTrie2 signature 0x%08x" % (name, signature))
	if (options & 0xf) != UTRIE2_32_VALUE_BITS:
		raise SystemExit("%s: expected 32-bit trie values" % name)
	if (options >> 4) != 0:
		raise SystemExit("%s: reserved UTrie2 option bits set" % name)

	# Both lengths are stored shifted; see UTrie2Header in utrie2_impl.h.
	data_length = shifted_data_length << UTRIE2_INDEX_SHIFT
	high_start = shifted_high_start << 11  # UTRIE2_SHIFT_1
	header_size = 16  # sizeof(UTrie2Header)
	index = list(struct.unpack_from("<%dH" % index_length, blob, header_size))
	data = list(struct.unpack_from(
			"<%dI" % data_length, blob, header_size + index_length * 2))

	total = header_size + index_length * 2 + data_length * 4
	if total > len(blob):
		raise SystemExit("%s: UTrie2 runs past the end of its section" % name)

	return {
		"index": index,
		"data": data,
		"indexLength": index_length,
		"dataLength": data_length,
		"highStart": high_start,
		# utrie2_openFromSerialized: for 32-bit values highValueIndex is an index
		# into data32 and the index array is NOT added to it.
		"highValueIndex": data_length - UTRIE2_DATA_GRANULARITY,
		"index2NullOffset": index2_null_offset,
		"dataNullOffset": data_null_offset,
		"serializedLength": total,
	}


def split_collation(blob, name):
	"""Split a serialized CollationData into its sections.

	`blob` starts at indexes[]; the udata header, if any, is already stripped.
	Returns a dict of section name -> bytes, plus the scalar indexes.
	"""
	indexes_length = struct.unpack_from("<i", blob, 0)[0]
	if indexes_length < 2:
		raise SystemExit("%s: only %d indexes" % (name, indexes_length))
	indexes = list(struct.unpack_from("<%di" % indexes_length, blob, 0))

	def get(index):
		# CollationDataReader::getIndex: anything past the end reads as the
		# previous offset, i.e. an empty section.
		if index < indexes_length:
			return indexes[index]
		return indexes[indexes_length - 1] if indexes_length > IX_REORDER_CODES_OFFSET else 0

	def section(index):
		start = get(index)
		end = get(index + 1)
		return blob[start:end] if end > start else b""

	return {
		"indexes": indexes,
		"options": indexes[IX_OPTIONS],
		"jamoCE32sStart": indexes[IX_JAMO_CE32S_START] if indexes_length > IX_JAMO_CE32S_START else -1,
		"reorderCodes": section(IX_REORDER_CODES_OFFSET),
		"reorderTable": section(IX_REORDER_TABLE_OFFSET),
		"trie": section(IX_TRIE_OFFSET),
		"ces": section(IX_CES_OFFSET),
		"ce32s": section(IX_CE32S_OFFSET),
		"rootElements": section(IX_ROOT_ELEMENTS_OFFSET),
		"contexts": section(IX_CONTEXTS_OFFSET),
		"unsafeBwd": section(IX_UNSAFE_BWD_OFFSET),
		"fastLatin": section(IX_FAST_LATIN_TABLE_OFFSET),
		"scripts": section(IX_SCRIPTS_OFFSET),
		"compressibleBytes": section(IX_COMPRESSIBLE_BYTES_OFFSET),
	}


def read_root(icu4c, han_type):
	path = os.path.join(icu4c, "source", "data", "in", "coll",
			"ucadata-%s.icu" % han_type)
	with open(path, "rb") as f:
		blob = f.read()
	header_size, format_version, data_version, _ = read_udata_header(
			blob, b"UCol", os.path.basename(path))
	if format_version[0] != 5:
		raise SystemExit("%s: UCol formatVersion %d, the port implements 5 - "
				"re-read collationdatareader.h before regenerating"
				% (os.path.basename(path), format_version[0]))
	data = split_collation(blob[header_size:], os.path.basename(path))
	data["ucaVersion"] = data_version
	data["path"] = path
	return data


# --- ICU .dat package and resource bundles ------------------------------------
#
# Only enough of each format to reach the one binary resource we want. Both are
# read-only and fixed by ICU's serializer, and both are checked rather than
# trusted: a silent format change here would produce a table that loads and
# sorts wrong.

def read_dat_package(path):
	"""name -> bytes for every item in an ICU .dat package."""
	with open(path, "rb") as f:
		blob = f.read()
	header_size, _fv, _dv, _ = read_udata_header(blob, b"CmnD", os.path.basename(path))
	base = header_size
	count = struct.unpack_from("<I", blob, base)[0]
	entries = [struct.unpack_from("<II", blob, base + 4 + i * 8) for i in range(count)]

	def name_at(offset):
		end = blob.index(b"\0", base + offset)
		return blob[base + offset:end].decode("ascii")

	items = {}
	for i, (name_offset, data_offset) in enumerate(entries):
		end = entries[i + 1][1] if i + 1 < count else (len(blob) - base)
		items[name_at(name_offset)] = blob[base + data_offset:base + end]
	return items


RES_STRING = 0
RES_BINARY = 1
RES_TABLE = 2
RES_ALIAS = 3
RES_TABLE32 = 4


def read_res_binaries(blob, name):
	"""path -> bytes for every Binary resource in a .res bundle.

	The bundle is a tree of tables; we walk it and collect binaries by their key
	path, which is how `/collations/standard/%%CollationBin` is addressed.
	"""
	header_size, format_version, _dv, _ = read_udata_header(blob, b"ResB", name)
	if format_version[0] not in (2, 3):
		raise SystemExit("%s: ResB formatVersion %d is not 2 or 3"
				% (name, format_version[0]))
	body = blob[header_size:]
	root = struct.unpack_from("<I", body, 0)[0]
	out = {}

	def key_at(offset):
		end = body.index(b"\0", offset)
		return body[offset:end].decode("ascii")

	def walk(res, path, depth):
		if depth > 8:
			raise SystemExit("%s: resource tree deeper than 8" % name)
		res_type = res >> 28
		offset = res & 0x0FFFFFFF
		if res_type == RES_BINARY:
			if offset == 0:
				return
			length = struct.unpack_from("<i", body, offset * 4)[0]
			out[path] = body[offset * 4 + 4:offset * 4 + 4 + length]
		elif res_type == RES_TABLE:
			count = struct.unpack_from("<H", body, offset * 4)[0]
			keys_at = offset * 4 + 2
			res_at = (keys_at + count * 2 + 3) & ~3
			for i in range(count):
				key = key_at(struct.unpack_from("<H", body, keys_at + i * 2)[0])
				walk(struct.unpack_from("<I", body, res_at + i * 4)[0],
						path + "/" + key, depth + 1)
		elif res_type == RES_TABLE32:
			count = struct.unpack_from("<i", body, offset * 4)[0]
			keys_at = offset * 4 + 4
			res_at = keys_at + count * 4
			for i in range(count):
				key = key_at(struct.unpack_from("<i", body, keys_at + i * 4)[0])
				walk(struct.unpack_from("<I", body, res_at + i * 4)[0],
						path + "/" + key, depth + 1)
		# Strings, aliases, integers and arrays cannot contain a CollationBin.

	walk(root, "", 0)
	return out


def read_tailorings(icu4c):
	"""(locale, variant) -> built collation blob, for every locale ICU ships."""
	path = os.path.join(icu4c, "source", "data", "in", "icudt78l.dat")
	if not os.path.exists(path):
		# The version is in the file name; find whichever one is there.
		in_dir = os.path.join(icu4c, "source", "data", "in")
		candidates = [f for f in os.listdir(in_dir)
				if f.startswith("icudt") and f.endswith(".dat")]
		if len(candidates) != 1:
			raise SystemExit("expected exactly one icudt*.dat in %s, found %r"
					% (in_dir, candidates))
		path = os.path.join(in_dir, candidates[0])

	items = read_dat_package(path)
	out = {}
	for name, blob in items.items():
		parts = name.split("/")
		if len(parts) != 3 or parts[1] != "coll" or not parts[2].endswith(".res"):
			continue
		locale = parts[2][:-4]
		if locale == "res_index":
			continue
		for res_path, data in read_res_binaries(blob, name).items():
			bits = res_path.split("/")
			if len(bits) == 4 and bits[1] == "collations" and bits[3] == "%%CollationBin":
				out[(locale, bits[2])] = data
	if not out:
		raise SystemExit("%s: no collation tailorings found" % path)
	return out


def parse_tailoring(blob, key, uca_key):
	"""Split one %%CollationBin, checking the header the same way ICU does."""
	name = "%s/%s" % key
	header_size, format_version, data_version, _ = read_udata_header(blob, b"UCol", name)
	if format_version[0] != 5:
		raise SystemExit("%s: UCol formatVersion %d, the port implements 5"
				% (name, format_version[0]))
	if uca_version_key(data_version) != uca_key:
		raise SystemExit("%s: built for UCA %s, the root is %s - the two came from "
				"different ICU trees" % (name, uca_version_text(data_version), uca_key))
	data = split_collation(blob[header_size:], name)
	data["ucaVersion"] = data_version
	return data


# --- the root unsafe-backward set ---------------------------------------------

def read_ccc_and_decomp(icu4c):
	"""ccc[] and the canonical decomposition mapping, from UnicodeData.txt."""
	path = os.path.join(icu4c, "source", "data", "unidata", "UnicodeData.txt")
	ccc = {}
	decomp = {}
	with open(path, encoding="utf-8") as f:
		for line in f:
			fields = line.split(";")
			if len(fields) < 6:
				continue
			cp = int(fields[0], 16)
			if fields[3] != "0":
				ccc[cp] = int(fields[3])
			mapping = fields[5].strip()
			if mapping and not mapping.startswith("<"):
				decomp[cp] = [int(x, 16) for x in mapping.split()]
	return ccc, decomp


def lead_combining_classes(icu4c):
	"""The set of code points whose full canonical decomposition starts with a
	non-zero combining class - ICU's lccc, and the half of the root collator's
	unsafe-backward set that it builds on load rather than reading from the file.

	Hangul syllables decompose to jamo, all of which have ccc 0, so the
	algorithmic decompositions do not have to be expanded here.
	"""
	ccc, decomp = read_ccc_and_decomp(icu4c)

	memo = {}

	def first_of(cp):
		if cp in memo:
			return memo[cp]
		memo[cp] = ccc.get(cp, 0)  # break cycles; the UCD has none
		mapping = decomp.get(cp)
		value = first_of(mapping[0]) if mapping else ccc.get(cp, 0)
		memo[cp] = value
		return value

	return {cp for cp in set(list(ccc) + list(decomp)) if first_of(cp) != 0}


def parse_serialized_set(blob, name):
	"""Ranges out of a serialized UnicodeSet (uset_getSerializedSet)."""
	if len(blob) < 2:
		return []
	words = list(struct.unpack_from("<%dH" % (len(blob) // 2), blob, 0))
	length = words[0]
	if length & 0x8000:
		# Has supplementary code points: the BMP length is in the next word.
		length &= 0x7FFF
		bmp_length = words[1]
		array = words[2:2 + length]
	else:
		bmp_length = length
		array = words[1:1 + length]
	if len(array) < length:
		raise SystemExit("%s: serialized set is truncated" % name)

	values = []
	i = 0
	while i < bmp_length:
		values.append(array[i])
		i += 1
	while i + 1 < length:
		values.append((array[i] << 16) | array[i + 1])
		i += 2

	ranges = []
	for i in range(0, len(values) - 1, 2):
		ranges.append((values[i], values[i + 1] - 1))
	if len(values) % 2 == 1:
		ranges.append((values[-1], 0x10FFFF))
	return ranges


def root_unsafe_backward(root, icu4c):
	"""The root collator's unsafe-backward set, as sorted inclusive ranges.

	CollationDataReader builds this on load out of four parts; all four are
	computable here, so the engine gets a constant instead of a set to construct.
	"""
	points = set()
	points.update(lead_combining_classes(icu4c))
	points.update(range(0xDC00, 0xE000))  # trail surrogates
	for start, end in parse_serialized_set(root["unsafeBwd"], "root unsafeBwd"):
		points.update(range(start, end + 1))
	# A lead surrogate is unsafe if any of its 1024 supplementary code points is.
	for lead in range(0xD800, 0xDC00):
		first = 0x10000 + (lead - 0xD800) * 0x400
		if any(cp in points for cp in range(first, first + 0x400)):
			points.add(lead)

	ranges = []
	for cp in sorted(points):
		if ranges and cp == ranges[-1][1] + 1:
			ranges[-1][1] = cp
		else:
			ranges.append([cp, cp])
	return [tuple(r) for r in ranges]


# --- emission -----------------------------------------------------------------

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
// GENERATED FILE - do not edit.  Produced by
// runtime/src/unicode/data/gen-collation-tables.py from the icu4c checkout in
// runtime/toolchains/src/icu4c.  See data/README.adoc to regenerate."""


def u32(values):
	return [v & 0xFFFFFFFF for v in values]


def as_u16(blob):
	return list(struct.unpack_from("<%dH" % (len(blob) // 2), blob, 0))


def as_u32(blob):
	return list(struct.unpack_from("<%dI" % (len(blob) // 4), blob, 0))


def as_i64(blob):
	return list(struct.unpack_from("<%dq" % (len(blob) // 8), blob, 0))


def emit_i64_array(out, name, values):
	"""Collation elements, as int64_t.

	They are weights, and the top bit of a primary weight is set for most of the
	code space, so the hex literal is unsigned and does not narrow to int64_t on
	its own - hence the explicit conversion on every entry. Hex rather than the
	signed decimal it would become: the fields inside a CE are read by hand often
	enough that the decimal form would be useless.
	"""
	out.append("static constexpr int64_t %s[%d] = {" % (name, len(values)))
	for i in range(0, len(values), 5):
		chunk = values[i:i + 5]
		out.append("\t" + ", ".join(
				"int64_t(0x%xULL)" % (v & 0xFFFFFFFFFFFFFFFF) for v in chunk) + ",")
	out[-1] = out[-1][:-1]
	out.append("};")
	out.append("")


def emit_trie(out, prefix, trie):
	emit_array(out, "uint16_t", prefix + "TrieIndex", trie["index"], per_line=16)
	emit_array(out, "uint32_t", prefix + "TrieData", trie["data"], per_line=8)
	out.append("static constexpr int32_t %sTrieIndexLength = %d;" % (prefix, trie["indexLength"]))
	out.append("static constexpr int32_t %sTrieDataLength = %d;" % (prefix, trie["dataLength"]))
	out.append("static constexpr char32_t %sTrieHighStart = 0x%x;" % (prefix, trie["highStart"]))
	out.append("static constexpr int32_t %sTrieHighValueIndex = %d;"
			% (prefix, trie["highValueIndex"]))
	out.append("")


def emit_collation_sections(out, prefix, data, name):
	"""The sections of one CollationData, as typed arrays.

	Empty sections are skipped: the C++ side then leaves that pointer null and
	falls back to the base, which is exactly what CollationDataReader does.
	"""
	present = {}

	if data["trie"]:
		trie = parse_utrie2_32(data["trie"], name + " trie")
		emit_trie(out, prefix, trie)
		present["trie"] = True

	if data["ce32s"]:
		emit_array(out, "uint32_t", prefix + "Ce32s", as_u32(data["ce32s"]), per_line=8)
		present["ce32s"] = len(data["ce32s"]) // 4
	if data["ces"]:
		emit_i64_array(out, prefix + "Ces", as_i64(data["ces"]))
		present["ces"] = len(data["ces"]) // 8
	if data["contexts"]:
		emit_array(out, "char16_t", prefix + "Contexts", as_u16(data["contexts"]), per_line=12)
		present["contexts"] = len(data["contexts"]) // 2
	if data["fastLatin"]:
		table = as_u16(data["fastLatin"])
		if (table[0] >> 8) != FAST_LATIN_VERSION:
			raise SystemExit("%s: fast Latin table version %d, the port implements %d"
					% (name, table[0] >> 8, FAST_LATIN_VERSION))
		emit_array(out, "uint16_t", prefix + "FastLatin", table, per_line=12)
		present["fastLatin"] = len(table)
		emit_fast_latin_primaries(out, prefix, table, bool(data["reorderCodes"]))
		present["fastLatinPrimaries"] = True
	if data["scripts"]:
		scripts = as_u16(data["scripts"])
		num_scripts = scripts[0]
		starts_offset = 1 + num_scripts + 16
		starts_length = len(scripts) - starts_offset
		if starts_length <= 2 or starts_length > 256:
			raise SystemExit("%s: %d script ranges" % (name, starts_length))
		if not (scripts[starts_offset] == 0
				and scripts[starts_offset + 1] == ((MERGE_SEPARATOR_BYTE + 1) << 8)
				and scripts[-1] == (TRAIL_WEIGHT_BYTE << 8)):
			raise SystemExit("%s: scriptStarts sentinels are wrong" % name)
		emit_array(out, "uint16_t", prefix + "Scripts", scripts, per_line=12)
		out.append("static constexpr uint16_t %sNumScripts = %d;" % (prefix, num_scripts))
		out.append("static constexpr int32_t %sScriptStartsOffset = %d;" % (prefix, starts_offset))
		out.append("static constexpr int32_t %sScriptStartsLength = %d;" % (prefix, starts_length))
		out.append("")
		present["scripts"] = True
	if data["compressibleBytes"]:
		if len(data["compressibleBytes"]) < 256:
			raise SystemExit("%s: compressibleBytes is %d bytes"
					% (name, len(data["compressibleBytes"])))
		emit_array(out, "uint8_t", prefix + "Compressible",
				list(data["compressibleBytes"][:256]), fmt="%d", per_line=32)
		present["compressibleBytes"] = True

	out.append("static constexpr uint32_t %sNumericPrimary = 0x%x;"
			% (prefix, data["options"] & 0xFF000000))
	out.append("static constexpr int32_t %sOptions = 0x%x;" % (prefix, data["options"] & 0xFFFF))
	out.append("static constexpr int32_t %sJamoCe32sStart = %d;" % (prefix, data["jamoCE32sStart"]))
	out.append("")
	return present


# CollationFastLatin constants, for the precomputed primaries below.
FAST_LATIN_LIMIT = 0x180
FAST_LATIN_MIN_LONG = 0xC00
FAST_LATIN_MIN_SHORT = 0x1000
FAST_LATIN_SHORT_PRIMARY_MASK = 0xFC00
FAST_LATIN_LONG_PRIMARY_MASK = 0xFFF8


def emit_fast_latin_primaries(out, prefix, table, has_reordering):
	"""The Latin fast path's primary weights, precomputed.

	ICU computes these once when a collator object is created and keeps them in
	it. This API has no collator object - a comparison takes a locale and options
	and nothing persists between calls - so computing them per call would cost far
	more than the fast path saves. They are constants of the table, so they are
	computed here instead.

	Only the default alternate handling is precomputed. With alternate=shifted the
	threshold moves and the array changes, so the fast path is simply not used
	there; the same goes for numeric collation, where the digits would have to be
	knocked out. Both are rare, and both remain correct - just not accelerated.

	A locale that reorders scripts gets its digits knocked out unconditionally.
	ICU works out whether the reordering actually moved the digits relative to
	their neighbours; being conservative here costs those locales the fast path on
	digits and nothing else.
	"""
	# With alternate=non-ignorable nothing is variable, and the threshold sits just
	# below the lowest long mini primary.
	mini_var_top = FAST_LATIN_MIN_LONG - 1
	body = table[table[0] & 0xFF:]
	primaries = []
	for c in range(FAST_LATIN_LIMIT):
		p = body[c]
		if p >= FAST_LATIN_MIN_SHORT:
			p &= FAST_LATIN_SHORT_PRIMARY_MASK
		elif p > mini_var_top:
			p &= FAST_LATIN_LONG_PRIMARY_MASK
		else:
			p = 0
		primaries.append(p)
	if has_reordering:
		for c in range(0x30, 0x3A):
			primaries[c] = 0
	emit_array(out, "uint16_t", prefix + "FastLatinPrimaries", primaries, per_line=12)
	out.append("static constexpr int32_t %sFastLatinMiniVarTop = 0x%x;" % (prefix, mini_var_top))
	out.append("")


def emit_unsafe_backward(out, prefix, ranges):
	"""The unsafe-backward set as a flat sorted boundary array.

	`contains(c)` is "the number of boundaries at or below c is odd", which is one
	binary search and no branch on the range shape.
	"""
	bounds = []
	for start, end in ranges:
		bounds.append(start)
		bounds.append(end + 1)
	emit_array(out, "uint32_t", prefix + "UnsafeBwd", bounds, per_line=12)
	out.append("static constexpr int32_t %sUnsafeBwdLength = %d;" % (prefix, len(bounds)))
	out.append("")


HANGUL_BASE = 0xAC00
JAMO_L_BASE = 0x1100
JAMO_V_BASE = 0x1161
JAMO_T_BASE = 0x11A7
JAMO_V_COUNT = 21
JAMO_T_COUNT = 28
HANGUL_COUNT = 19 * JAMO_V_COUNT * JAMO_T_COUNT


def check_nfd(icu4c, full, ccc):
	"""Reproduce every NFD column of NormalizationTest.txt from the tables.

	The tables are the whole of the decomposing normalizer's knowledge, so if NFD
	comes out right for all 19 000 rows of the conformance file, a mistake in
	building them is not what a later failure means. The same file is asserted at
	run time against the C++ implementation; this catches the generator half
	early, and separately.
	"""
	path = os.path.join(icu4c, "source", "data", "unidata", "NormalizationTest.txt")
	if not os.path.exists(path):
		raise SystemExit("no NormalizationTest.txt at " + path)

	def decompose(cps):
		out = []
		for cp in cps:
			index = cp - HANGUL_BASE
			if 0 <= index < HANGUL_COUNT:
				out.append(JAMO_L_BASE + index // (JAMO_V_COUNT * JAMO_T_COUNT))
				out.append(JAMO_V_BASE + (index % (JAMO_V_COUNT * JAMO_T_COUNT)) // JAMO_T_COUNT)
				trail = index % JAMO_T_COUNT
				if trail != 0:
					out.append(JAMO_T_BASE + trail)
			elif cp in full:
				out.extend(full[cp])
			else:
				out.append(cp)
		# canonical ordering: a stable insertion sort by combining class
		i = 1
		while i < len(out):
			cc = ccc.get(out[i], 0)
			if cc != 0:
				j = i
				while j > 0 and 0 < ccc.get(out[j - 1], 0) > cc:
					out[j - 1], out[j] = out[j], out[j - 1]
					j -= 1
			i += 1
		return out

	rows = 0
	with open(path, encoding="utf-8") as f:
		for line in f:
			line = line.split("#")[0].strip()
			if not line or line.startswith("@"):
				continue
			columns = [c.strip() for c in line.split(";") if c.strip()]
			if len(columns) < 3:
				continue
			source = [int(x, 16) for x in columns[0].split()]
			expected = [int(x, 16) for x in columns[2].split()]
			if decompose(source) != expected:
				raise SystemExit("NormalizationTest.txt: NFD of %s came out %s, expected %s"
						% (columns[0],
							" ".join("%04X" % c for c in decompose(source)), columns[2]))
			rows += 1
	print("NFD checked against %d rows of NormalizationTest.txt" % rows)


def gen_norm(icu4c, out_dir):
	"""Canonical decomposition and FCD, straight from the UCD.

	Collation needs the *decomposing* half of normalization - NFD for a segment
	that is not in FCD, and the lccc/tccc pair for the FCD check itself. The
	UTS-46 engine next door carries the *composing* half, over ICU's serialized
	nfc data; the two halves share no code and this one is not a transcription of
	ICU's norm16 arithmetic, so there is nothing here that could drift out of step
	with it. NFD is a short, exactly specified algorithm (UAX #15 D68 plus
	canonical ordering), the tables come from UnicodeData.txt directly, and
	NormalizationTest.txt checks the result.

	Hangul is algorithmic and is not in the tables: 11 172 syllables would be four
	fifths of them.
	"""
	ccc, decomp = read_ccc_and_decomp(icu4c)

	def expand(cp):
		mapping = decomp.get(cp)
		if not mapping:
			return [cp]
		out = []
		for part in mapping:
			out.extend(expand(part))
		return out

	full = {}
	for cp in decomp:
		if 0xAC00 <= cp <= 0xD7A3:
			continue  # Hangul syllables decompose algorithmically
		full[cp] = expand(cp)

	codepoints = sorted(full)
	pool = []
	offsets = []
	for cp in codepoints:
		offsets.append(len(pool))
		for part in full[cp]:
			if part < 0x10000:
				pool.append(part)
			else:
				part -= 0x10000
				pool.append(0xD800 + (part >> 10))
				pool.append(0xDC00 + (part & 0x3FF))
	offsets.append(len(pool))
	if len(pool) > 0xFFFF:
		raise SystemExit("decomposition pool no longer fits a uint16 offset")

	# fcd16: the ccc of the first and last code point of the full decomposition,
	# or the code point's own ccc when it does not decompose.
	def fcd16(cp):
		mapping = full.get(cp)
		if not mapping:
			c = ccc.get(cp, 0)
			return (c << 8) | c
		return (ccc.get(mapping[0], 0) << 8) | ccc.get(mapping[-1], 0)

	fcd_starts = [0]
	fcd_values = [0]
	for cp in sorted(set(list(ccc) + list(full))):
		value = fcd16(cp)
		if value != fcd_values[-1]:
			fcd_starts.append(cp)
			fcd_values.append(value)
		if fcd16(cp + 1) != value:
			fcd_starts.append(cp + 1)
			fcd_values.append(fcd16(cp + 1))
	# Collapse the runs the loop above may have duplicated.
	starts, values = [fcd_starts[0]], [fcd_values[0]]
	for start, value in zip(fcd_starts[1:], fcd_values[1:]):
		if value == values[-1]:
			continue
		starts.append(start)
		values.append(value)

	# The CollationFCD fast path: one bit per BMP code point, plus lead surrogates
	# standing in for their supplementary blocks. Generated from the same table
	# rather than copied from ICU's collationfcd.cpp, so there is one source.
	def build_bits(select):
		blocks = []
		for block in range(2048):
			mask = 0
			for bit in range(32):
				cp = block * 32 + bit
				if 0xD800 <= cp < 0xDC00:
					# A lead surrogate stands in for its whole supplementary block.
					# hasLccc(lead) asks only about lccc, but hasTccc(lead) asks
					# about either: the fast path may be pessimistic, never
					# optimistic, and a lead whose block has any non-inert code
					# point must leave it.
					first = 0x10000 + (cp - 0xD800) * 0x400
					probe = select if select is select_lccc else (lambda v: v != 0)
					hit = any(probe(fcd16(x)) for x in range(first, first + 0x400))
				elif 0xDC00 <= cp < 0xE000:
					hit = select is select_lccc  # every trail surrogate has lccc set
				else:
					hit = select(fcd16(cp))
				if hit:
					mask |= 1 << bit
			blocks.append(mask)
		bits = [0]
		index = []
		for mask in blocks:
			if mask == 0:
				index.append(0)
				continue
			if mask not in bits:
				bits.append(mask)
			index.append(bits.index(mask))
		if len(bits) > 255:
			raise SystemExit("FCD bit table needs %d distinct words" % len(bits))
		return index, bits

	def select_lccc(value):
		return (value >> 8) != 0

	def select_tccc(value):
		return (value & 0xFF) != 0

	lccc_index, lccc_bits = build_bits(select_lccc)
	tccc_index, tccc_bits = build_bits(select_tccc)

	check_nfd(icu4c, full, ccc)

	out = [LICENSE, "",
		"// Canonical decomposition (NFD) and the FCD quick test, from",
		"// source/data/unidata/UnicodeData.txt. See gen-collation-tables.py for why",
		"// this is generated from the UCD rather than taken from ICU's nfc.nrm.",
		"//",
		"// Hangul is not here: its decomposition is arithmetic.",
		"",
		"///@ SP_EXCLUDE",
		"",
		"#pragma once",
		"",
		"namespace sprt::unicode::detail {",
		"",
		"// Code points with a canonical decomposition, sorted, and where each one's",
		"// units start in the pool. The length is the next entry's offset minus this",
		"// one, which is why there is one more offset than code point.",
	]
	emit_array(out, "uint32_t", "s_nfdCodepoints", codepoints, per_line=10)
	emit_array(out, "uint16_t", "s_nfdOffsets", offsets, per_line=12)
	emit_array(out, "char16_t", "s_nfdPool", pool, per_line=12)
	out.append("static constexpr int32_t s_nfdCount = %d;" % len(codepoints))
	out.append("")
	out.append("// fcd16 = lccc << 8 | tccc, as a sorted range table: the value applies from")
	out.append("// s_fcdStarts[i] until s_fcdStarts[i + 1].")
	emit_array(out, "uint32_t", "s_fcdStarts", starts, per_line=10)
	emit_array(out, "uint16_t", "s_fcdValues", values, per_line=12)
	out.append("static constexpr int32_t s_fcdCount = %d;" % len(starts))
	out.append("")
	out.append("// One bit per BMP code point for the FCD fast path (ICU CollationFCD).")
	out.append("// Index 0 means the whole 32-code-point block is zero.")
	emit_array(out, "uint8_t", "s_fcdLcccIndex", lccc_index, fmt="%d", per_line=32)
	emit_array(out, "uint32_t", "s_fcdLcccBits", lccc_bits, per_line=8)
	emit_array(out, "uint8_t", "s_fcdTcccIndex", tccc_index, fmt="%d", per_line=32)
	emit_array(out, "uint32_t", "s_fcdTcccBits", tccc_bits, per_line=8)
	out.append("} // namespace sprt::unicode::detail")

	write(os.path.join(out_dir, "SPRuntimeCollationNormData.cc"), out)


ROOT = "s_collRoot"


def emit_collation_instance(out, prefix, present, base):
	"""The CollationData aggregate over the arrays emitted above.

	It lives in a separate file from the arrays because it names a type, and the
	type is declared in collation_data.cc, which the compile unit includes after
	the tables.

	A tailoring only carries the sections it changes. ICU leaves the others null
	and follows `base` from the accessors; here the inherited pointer is written
	in directly, which is the same thing with one less indirection on every
	lookup - and there is exactly one base to inherit from, so nothing can go
	stale.
	"""
	is_root = base == "nullptr"

	def array(name, key):
		if present.get(key):
			return prefix + name
		return "nullptr" if is_root else ROOT + name

	def length(name, key, count):
		if present.get(key):
			return str(count) if isinstance(count, int) else count
		return "0" if is_root else (ROOT + name)

	out.append("static constexpr CollationData %sData = {" % prefix)
	if present.get("trie"):
		out.append("\t{%sTrieIndex, %sTrieData, %sTrieIndexLength, %sTrieDataLength,"
				% (prefix, prefix, prefix, prefix))
		out.append("\t\t%sTrieHighStart, %sTrieHighValueIndex}," % (prefix, prefix))
	elif is_root:
		out.append("\t{nullptr, nullptr, 0, 0, 0, 0},")
	else:
		# A settings-only tailoring: the base trie answers every character.
		out.append("\t{%sTrieIndex, %sTrieData, %sTrieIndexLength, %sTrieDataLength,"
				% (ROOT, ROOT, ROOT, ROOT))
		out.append("\t\t%sTrieHighStart, %sTrieHighValueIndex}," % (ROOT, ROOT))
	out.append("\t%s, %s," % (array("Ce32s", "ce32s"),
			present.get("ce32s", 0) if present.get("ce32s") or is_root else "6153"))
	out.append("\t%s, %s," % (array("Ces", "ces"),
			present.get("ces", 0) if present.get("ces") or is_root else "2041"))
	out.append("\t%s, %s," % (array("Contexts", "contexts"),
			present.get("contexts", 0) if present.get("contexts") or is_root else "4619"))
	out.append("\t%s," % base)
	if present.get("ce32s") and present.get("jamo", -1) >= 0:
		out.append("\t%sCe32s + %sJamoCe32sStart," % (prefix, prefix))
	elif is_root:
		out.append("\tnullptr,")
	else:
		out.append("\t%sCe32s + %sJamoCe32sStart," % (ROOT, ROOT))
	out.append("\t%sNumericPrimary," % (prefix if present.get("numericPrimary", True) else ROOT))
	out.append("\t%s," % array("Compressible", "compressibleBytes"))
	out.append("\t%s, %s," % (array("UnsafeBwd", "unsafeBwd"),
			(prefix if present.get("unsafeBwd") else ROOT) + "UnsafeBwdLength"))
	out.append("\t%s, %s," % (array("FastLatin", "fastLatin"),
			present.get("fastLatin", 0) if present.get("fastLatin") or is_root else "480"))
	out.append("\t%s," % array("FastLatinPrimaries", "fastLatinPrimaries"))
	if present.get("scripts"):
		out.append("\t%sNumScripts, %sScripts + 1, %sScripts + %sScriptStartsOffset,"
				% (prefix, prefix, prefix, prefix))
		out.append("\t\t%sScriptStartsLength," % prefix)
	elif is_root:
		out.append("\t0, nullptr, nullptr, 0,")
	else:
		out.append("\t%sNumScripts, %sScripts + 1, %sScripts + %sScriptStartsOffset,"
				% (ROOT, ROOT, ROOT, ROOT))
		out.append("\t\t%sScriptStartsLength," % ROOT)
	out.append("};")
	out.append("")


def gen_root(icu4c, out_dir, han_type):
	root = read_root(icu4c, han_type)
	uca_key = uca_version_key(root["ucaVersion"])
	unsafe = root_unsafe_backward(root, icu4c)

	out = [LICENSE, "",
		"// The CLDR root collation: the order every locale starts from, and the only",
		"// table that carries weights for the whole code space. Built by ICU from",
		"// source/data/unidata/FractionalUCA.txt; taken here as %s.icu, which orders" % ("ucadata-" + han_type),
		"// Han %s." % ("by Unihan radical-stroke" if han_type == "unihan"
				else "by code point (implicit weights)"),
		"//",
		"// `rootElements` is deliberately absent: 28 KB that only the rule compiler",
		"// reads, and the rule compiler is not ported - tailorings arrive built.",
		"",
		"///@ SP_EXCLUDE",
		"",
		"#pragma once",
		"",
		"namespace sprt::unicode::detail {",
		"",
		"// UCA version, as CollationTailoring::getUCAVersion computes it. A tailoring",
		"// whose key differs was built against a different root and must not be loaded.",
		"static constexpr int32_t s_collUcaVersion = %d; // Unicode %s"
				% (uca_key, uca_version_text(root["ucaVersion"])),
		"",
	]

	present = emit_collation_sections(out, "s_collRoot", root, "root")
	emit_unsafe_backward(out, "s_collRoot", unsafe)
	present["unsafeBwd"] = len(unsafe)
	present["jamo"] = root["jamoCE32sStart"]

	out.append("} // namespace sprt::unicode::detail")
	write(os.path.join(out_dir, "SPRuntimeCollationRootData.cc"), out)

	return root, uca_key, present


def gen_tables(out_dir, root_present, locale_entries, files):
	"""The aggregates and the locale table, in include order after the code."""
	tables = [LICENSE, "",
		"// The CollationData aggregates over the arrays in the *Data*.cc files, and",
		"// the table that maps a language tag to one of them.",
		"//",
		"// Which groups are compiled in is a build option (SPRT_COLLATION); a group",
		"// that is switched off takes its locales out of the table with it, and",
		"// hasCollation() then answers false for them.",
		"",
		"///@ SP_EXCLUDE",
		"",
		"#pragma once",
		"",
		"namespace sprt::unicode::detail {",
		"",
	]
	emit_collation_instance(tables, "s_collRoot", root_present, "nullptr")

	# The group files open the namespace themselves, so they are included between
	# two halves of this one rather than inside it.
	tables.append("} // namespace sprt::unicode::detail")
	tables.append("")
	for key, title, filename, count, size in files:
		tables.append("// %s: %d locales, %d bytes of built tables." % (title, count, size))
		tables.append("#if SPRT_COLLATION_%s" % key.upper())
		tables.append("#include \"%s\"" % filename)
		tables.append("#endif")
		tables.append("")
	tables.append("namespace sprt::unicode::detail {")
	tables.append("")

	tables.append("// Language tags, sorted, for a binary search. The tag is ICU's spelling:")
	tables.append("// a language subtag, then an optional script or region, joined by '_'.")
	tables.append("static constexpr CollationLocale s_collationLocales[] = {")
	for locale, name, group in sorted(locale_entries):
		tables.append("#if SPRT_COLLATION_%s" % group.upper())
		tables.append("\t{\"%s\", %d, &%sTailoring}," % (locale, len(locale), name))
		tables.append("#endif")
	tables.append("};")
	tables.append("")
	tables.append("} // namespace sprt::unicode::detail")
	write(os.path.join(out_dir, "SPRuntimeCollationTables.cc"), tables)


# --- tailorings ----------------------------------------------------------------
#
# Which locales go into which build group. A locale that is not listed here and
# has a tailoring would be silently dropped, so the generator refuses to run when
# that happens - a new CLDR release adding a language must be a decision, not an
# accident.

SCRIPT_GROUPS = [
	("LatinNordic", "Latin, Nordic and Germanic",
		["da", "fo", "fi", "is", "kl", "no", "se", "smn", "sv", "et", "af", "fy", "dsb", "hsb",
			"lb", "nl", "de", "de_AT", "wae"]),
	("LatinSlavic", "Latin, Slavic and Baltic",
		["bs", "cs", "hr", "pl", "sk", "sl", "sr_Latn", "lt", "lv"]),
	("LatinRomance", "Latin, Romance and the rest of the EU",
		["br", "ca", "cy", "eo", "es", "fr", "fr_CA", "gl", "ga", "hu", "it", "mt", "pt", "ro",
			"sq"]),
	("LatinTurkic", "Latin, Turkic languages and Vietnamese",
		["az", "tk", "tr", "uz", "vi"]),
	("LatinOther", "Latin, Africa, the Pacific and South-East Asia",
		["blo", "ceb", "ee", "ff", "ff_Adlm", "fil", "ha", "haw", "id", "ig", "ku", "lkt", "ln",
			"ms", "nso", "om", "st", "sw", "tn", "to", "wo", "xh", "yo", "zu"]),
	("Cyrillic", "Cyrillic",
		["be", "bg", "bs_Cyrl", "kk", "ky", "mk", "mn", "ru", "sr", "sr_Cyrl", "tg", "uk"]),
	("Greek", "Greek, Armenian and Georgian", ["el", "hy", "ka"]),
	("Semitic", "Hebrew, Arabic and Persian",
		["ar", "ars", "fa", "fa_AF", "he", "kk_Arab", "ps", "ug", "ur", "yi"]),
	("Indic", "The Indic scripts",
		["as", "bn", "gu", "hi", "kn", "kok", "ml", "mr", "ne", "or", "pa", "sa", "si", "ta",
			"te"]),
	("SouthEastAsia", "South-East Asia and Tibetan", ["bo", "dz", "km", "lo", "my", "th"]),
	("Other", "Everything else", ["am", "chr", "en_US_POSIX"]),
	("Cjk", "Chinese, Japanese and Korean", ["ja", "ko", "zh", "yue"]),
]

# Locales whose default collation is a named variant rather than `standard`.
# Chinese has four and CLDR's default is pinyin; German and Catalan have named
# variants but their default order *is* the root, so they get no entry at all.
DEFAULT_VARIANTS = {"zh": "pinyin"}


def tailoring_unsafe_backward(root_ranges, tailoring):
	"""A tailoring's unsafe-backward set: the root's, plus its own ranges.

	CollationDataReader clones the root collator's set and adds the ranges from
	the tailoring, then marks lead surrogates again. Same here, at build time.
	"""
	extra = parse_serialized_set(tailoring["unsafeBwd"], "tailoring unsafeBwd")
	if not extra:
		return None  # nothing added: the root's set is used as it stands

	points = set()
	for start, end in root_ranges:
		points.update(range(start, end + 1))
	for start, end in extra:
		points.update(range(start, end + 1))
	for lead in range(0xD800, 0xDC00):
		first = 0x10000 + (lead - 0xD800) * 0x400
		if any(cp in points for cp in range(first, first + 0x400)):
			points.add(lead)

	ranges = []
	for cp in sorted(points):
		if ranges and cp == ranges[-1][1] + 1:
			ranges[-1][1] = cp
		else:
			ranges.append([cp, cp])
	return [tuple(r) for r in ranges]


def alias_reordering(tailoring, name):
	"""CollationSettings::aliasReordering, computed here rather than on load.

	Returns (rangesOffset, rangesLength, minHighNoReorder) for the reorder table
	that ships with the tailoring. ICU has a fallback that regenerates the ranges
	when the table is missing; every tailoring ICU ships has one, so that path is
	not ported - and this refuses to run rather than sort wrongly if that ever
	changes.
	"""
	codes = as_u32(tailoring["reorderCodes"])
	if not codes:
		return None
	table = tailoring["reorderTable"]
	if len(table) < 256:
		raise SystemExit("%s: reorder codes without a table; the regeneration path "
				"(CollationData::makeReorderRanges) is not ported" % name)

	# The trailing entries of reorderCodes are the (limit, offset) ranges: a range
	# has a non-zero upper half, a code does not.
	ranges_length = 0
	while ranges_length < len(codes) and (codes[len(codes) - ranges_length - 1] & 0xFFFF0000) != 0:
		ranges_length += 1
	ranges = codes[len(codes) - ranges_length:] if ranges_length else []

	if ranges:
		if (ranges[0] & 0xFFFF) != 0 or (ranges[-1] & 0xFFFF) == 0:
			raise SystemExit("%s: reorder ranges do not start at offset 0 or end at a non-zero one"
					% name)

	# Ranges before the first split lead byte are handled by the table itself.
	first = 0
	while first < ranges_length and (ranges[first] & 0xFF0000) == 0:
		first += 1
	if first == ranges_length:
		return (ranges, 0, 0, 0)
	return (ranges, first, ranges_length - first, ranges[-1] & 0xFFFF0000)


def gen_tailorings(icu4c, out_dir, root, uca_key):
	tailorings = read_tailorings(icu4c)
	root_ranges = root_unsafe_backward(root, icu4c)

	available = {}
	for (locale, variant), blob in tailorings.items():
		wanted = DEFAULT_VARIANTS.get(locale, "standard")
		if variant == wanted:
			available[locale] = blob

	grouped = {}
	assigned = set()
	for key, _title, locales in SCRIPT_GROUPS:
		grouped[key] = [loc for loc in locales if loc in available]
		assigned.update(locales)

	missing = sorted(set(available) - assigned - {"root"})
	if missing:
		raise SystemExit("these locales have a tailoring but no group in SCRIPT_GROUPS: %s\n"
				"Add them to a group (or to a new one) - a locale silently dropped here is a "
				"language that quietly sorts wrong." % ", ".join(missing))

	# Identical blobs are shared: hr/bs/sr_Latn are one table, and so are several
	# of the African ones.
	emitted = {}
	locale_entries = []
	files = []
	total = 0

	for key, title, _locales in SCRIPT_GROUPS:
		locales = grouped[key]
		if not locales:
			continue
		out = [LICENSE, "",
			"// Collation tailorings: %s." % title,
			"//",
			"// Each of these sits on top of the root table: its trie answers with",
			"// FallbackCE32 for every character the language does not move, and the",
			"// engine follows the base from there.",
			"",
			"///@ SP_EXCLUDE",
			"",
			"#pragma once",
			"",
			"namespace sprt::unicode::detail {",
			"",
		]
		instances = []
		group_bytes = 0
		for locale in locales:
			blob = available[locale]
			data = parse_tailoring(blob, (locale, "standard"), uca_key)
			name = "s_coll_" + locale.replace("-", "_")
			signature = bytes(blob)
			if signature in emitted:
				locale_entries.append((locale, emitted[signature], key))
				continue

			present = emit_collation_sections(out, name, data, locale)
			present["jamo"] = data["jamoCE32sStart"]
			unsafe = tailoring_unsafe_backward(root_ranges, data)
			if unsafe is not None:
				emit_unsafe_backward(out, name, unsafe)
				present["unsafeBwd"] = len(unsafe)
			else:
				present["unsafeBwd"] = 0

			reorder = alias_reordering(data, locale)
			if reorder is not None:
				ranges, first, length, min_high = reorder
				if ranges:
					emit_array(out, "uint32_t", name + "ReorderRanges", ranges, per_line=8)
				emit_array(out, "uint8_t", name + "ReorderTable",
						list(data["reorderTable"][:256]), fmt="%d", per_line=32)
			instances.append((name, present, reorder))
			emitted[signature] = name
			locale_entries.append((locale, name, key))
			group_bytes += len(blob)

		for name, present, _reorder in instances:
			emit_collation_instance(out, name, present, "&s_collRootData")
		for name, present, reorder in instances:
			out.append("static constexpr CollationTailoring %sTailoring = {" % name)
			out.append("\t&%sData," % name)
			out.append("\t%sOptions," % name)
			if reorder is not None:
				ranges, first, length, min_high = reorder
				out.append("\t%sReorderTable," % name)
				if length:
					out.append("\t%sReorderRanges + %d, %d, 0x%x," % (name, first, length, min_high))
				else:
					out.append("\tnullptr, 0, 0,")
			else:
				out.append("\tnullptr,")
				out.append("\tnullptr, 0, 0,")
			out.append("};")
			out.append("")

		out.append("} // namespace sprt::unicode::detail")
		filename = "SPRuntimeCollationData%s.cc" % key
		write(os.path.join(out_dir, filename), out)
		files.append((key, title, filename, len(locales), group_bytes))
		total += group_bytes

	return locale_entries, files, total


def write(path, lines):
	text = "\n".join(lines).rstrip("\n") + "\n"
	with open(path, "w", encoding="utf-8") as f:
		f.write(text)
	print("wrote %s (%d bytes)" % (os.path.basename(path), len(text)))


# --- reporting ----------------------------------------------------------------

def report(icu4c):
	for han in ("implicithan", "unihan"):
		root = read_root(icu4c, han)
		print("root (%s): UCA %s" % (han, uca_version_text(root["ucaVersion"])))
		total = 0
		for key in ("trie", "ces", "ce32s", "rootElements", "contexts", "unsafeBwd",
				"fastLatin", "scripts", "compressibleBytes"):
			size = len(root[key])
			total += size
			print("    %-20s %8d" % (key, size))
		print("    %-20s %8d (%d without rootElements)"
				% ("total", total, total - len(root["rootElements"])))

	tailorings = read_tailorings(icu4c)
	standard = {loc: blob for (loc, variant), blob in tailorings.items()
			if variant == "standard"}
	by_content = {}
	for loc, blob in standard.items():
		by_content.setdefault(blob, []).append(loc)
	print("tailorings: %d locales, %d variants, %d distinct standard blobs"
			% (len(standard), len(tailorings), len(by_content)))
	print("    standard total %d, deduped %d"
			% (sum(len(b) for b in standard.values()), sum(len(b) for b in by_content)))
	for blob, locs in sorted(by_content.items(), key=lambda kv: -len(kv[0]))[:10]:
		print("    %8d  %s" % (len(blob), ", ".join(sorted(locs))))

	root = read_root(icu4c, "unihan")
	unsafe = root_unsafe_backward(root, icu4c)
	print("root unsafe-backward: %d ranges" % len(unsafe))


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("icu4c", nargs="?",
			default=os.path.join(_ROOT, "runtime", "toolchains", "src", "icu4c"))
	parser.add_argument("--out", default=_HERE)
	parser.add_argument("--report", action="store_true",
			help="print the sizes and write nothing")
	parser.add_argument("--han", default="unihan", choices=("unihan", "implicithan"),
			help="how the root orders Han: unihan is what ICU itself ships, and "
				"what the tailorings were built against")
	args = parser.parse_args()

	if not os.path.isdir(args.icu4c):
		raise SystemExit("no icu4c checkout at " + args.icu4c)

	if args.report:
		report(args.icu4c)
		return

	gen_norm(args.icu4c, args.out)
	root, uca_key, root_present = gen_root(args.icu4c, args.out, args.han)
	locale_entries, files, total = gen_tailorings(args.icu4c, args.out, root, uca_key)
	gen_tables(args.out, root_present, locale_entries, files)
	print("tailorings: %d locales, %d distinct tables, %d bytes"
			% (len(locale_entries), len(files), total))


if __name__ == "__main__":
	main()
