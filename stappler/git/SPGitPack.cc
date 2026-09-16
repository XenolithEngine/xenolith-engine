/**
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

#include "SPGitPack.h"

#include <zlib.h>

namespace STAPPLER_VERSIONIZED stappler::git {

// git packfile object types (3-bit tag in the object header)
enum PackObjType {
	OBJ_COMMIT = 1,
	OBJ_TREE = 2,
	OBJ_BLOB = 3,
	OBJ_TAG = 4,
	OBJ_OFS_DELTA = 6,
	OBJ_REF_DELTA = 7,
};

static ObjectType toObjectType(int t) {
	switch (t) {
	case OBJ_COMMIT: return ObjectType::Commit;
	case OBJ_TREE: return ObjectType::Tree;
	case OBJ_BLOB: return ObjectType::Blob;
	case OBJ_TAG: return ObjectType::Tag;
	default: return ObjectType::Invalid;
	}
}

// --- ObjectStore ---

const Object *ObjectStore::find(StringView hexKey) const {
	auto it = objects.find(hexKey);
	return it == objects.end() ? nullptr : &it->second;
}

const Object *ObjectStore::find(const Oid &oid) const { return find(StringView(oid.str())); }

// --- zlib ---

Status inflateStream(const uint8_t *data, size_t avail, Bytes &out, size_t &consumed) {
	z_stream zs = {};
	zs.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(data));
	zs.avail_in = uInt(avail);
	if (inflateInit(&zs) != Z_OK) {
		return Status::ErrorNotRecoverable;
	}

	uint8_t buf[16'384];
	int ret = Z_OK;
	do {
		zs.next_out = buf;
		zs.avail_out = sizeof(buf);
		ret = inflate(&zs, Z_NO_FLUSH);
		if (ret != Z_OK && ret != Z_STREAM_END) {
			inflateEnd(&zs);
			return Status::ErrorInvalidArguemnt;
		}
		size_t have = sizeof(buf) - zs.avail_out;
		out.insert(out.end(), buf, buf + have);
	} while (ret != Z_STREAM_END);

	consumed = zs.total_in;
	inflateEnd(&zs);
	return Status::Ok;
}

// --- delta ---

// Little-endian base-128 varint (delta source/target size).
static size_t readVarLE(const uint8_t *d, size_t n, size_t &i) {
	size_t v = 0;
	int shift = 0;
	while (i < n) {
		uint8_t c = d[i++];
		v |= size_t(c & 0x7f) << shift;
		if (!(c & 0x80)) {
			break;
		}
		shift += 7;
	}
	return v;
}

Status applyDelta(BytesView base, BytesView delta, Bytes &out) {
	const uint8_t *d = delta.data();
	size_t n = delta.size();
	size_t i = 0;

	size_t srcSize = readVarLE(d, n, i);
	if (srcSize != base.size()) {
		return Status::ErrorInvalidArguemnt;
	}
	size_t dstSize = readVarLE(d, n, i);

	out.clear();
	out.reserve(dstSize);

	const uint8_t *b = base.data();
	size_t bn = base.size();

	while (i < n) {
		uint8_t op = d[i++];
		if (op & 0x80) {
			// copy from base
			size_t cpOff = 0, cpSize = 0;
			if (op & 0x01) {
				cpOff |= size_t(d[i++]);
			}
			if (op & 0x02) {
				cpOff |= size_t(d[i++]) << 8;
			}
			if (op & 0x04) {
				cpOff |= size_t(d[i++]) << 16;
			}
			if (op & 0x08) {
				cpOff |= size_t(d[i++]) << 24;
			}
			if (op & 0x10) {
				cpSize |= size_t(d[i++]);
			}
			if (op & 0x20) {
				cpSize |= size_t(d[i++]) << 8;
			}
			if (op & 0x40) {
				cpSize |= size_t(d[i++]) << 16;
			}
			if (cpSize == 0) {
				cpSize = 0x1'0000;
			}
			if (cpOff + cpSize > bn) {
				return Status::ErrorInvalidArguemnt;
			}
			out.insert(out.end(), b + cpOff, b + cpOff + cpSize);
		} else if (op != 0) {
			// insert `op` literal bytes from the delta stream
			if (i + op > n) {
				return Status::ErrorInvalidArguemnt;
			}
			out.insert(out.end(), d + i, d + i + op);
			i += op;
		} else {
			return Status::ErrorInvalidArguemnt; // opcode 0 is reserved
		}
	}

	if (out.size() != dstSize) {
		return Status::ErrorInvalidArguemnt;
	}
	return Status::Ok;
}

// --- packfile ---

namespace {

struct RawObject {
	int packType = 0;
	size_t baseOffset = 0; // for OFS_DELTA
	Oid baseOid; // for REF_DELTA
	Bytes data; // inflated content (non-delta) or delta instructions

	bool resolved = false;
	ObjectType finalType = ObjectType::Invalid;
	Bytes finalData;
	Oid oid;
};

} // namespace

Status parsePack(BytesView pack, ObjectFormat fmt, ObjectStore &out) {
	const uint8_t *p = pack.data();
	size_t n = pack.size();
	size_t oidSize = getOidSize(fmt);

	// header: "PACK" + version(4) + count(4), trailer: oidSize checksum
	if (n < 12 + oidSize || p[0] != 'P' || p[1] != 'A' || p[2] != 'C' || p[3] != 'K') {
		return Status::ErrorInvalidArguemnt;
	}
	uint32_t version = (uint32_t(p[4]) << 24) | (uint32_t(p[5]) << 16) | (uint32_t(p[6]) << 8)
			| uint32_t(p[7]);
	uint32_t count = (uint32_t(p[8]) << 24) | (uint32_t(p[9]) << 16) | (uint32_t(p[10]) << 8)
			| uint32_t(p[11]);
	if (version != 2 && version != 3) {
		return Status::ErrorNotSupported;
	}

	size_t dataEnd = n - oidSize; // exclude trailing checksum
	size_t i = 12;

	Vector<RawObject> objs;
	objs.reserve(count);
	Map<size_t, size_t> offsetToIndex;

	for (uint32_t objIdx = 0; objIdx < count; ++objIdx) {
		if (i >= dataEnd) {
			return Status::ErrorInvalidArguemnt;
		}
		size_t objOffset = i;

		// object header varint: type (3 bits) + size (LE base-128)
		uint8_t c = p[i++];
		int type = (c >> 4) & 0x7;
		size_t size = c & 0x0f;
		int shift = 4;
		while (c & 0x80) {
			if (i >= dataEnd) {
				return Status::ErrorInvalidArguemnt;
			}
			c = p[i++];
			size |= size_t(c & 0x7f) << shift;
			shift += 7;
		}
		(void)size; // uncompressed size hint; we validate via inflate instead

		RawObject raw;
		raw.packType = type;

		if (type == OBJ_OFS_DELTA) {
			// negative offset varint (big-endian with +1 accumulation)
			if (i >= dataEnd) {
				return Status::ErrorInvalidArguemnt;
			}
			uint8_t oc = p[i++];
			size_t off = oc & 0x7f;
			while (oc & 0x80) {
				if (i >= dataEnd) {
					return Status::ErrorInvalidArguemnt;
				}
				oc = p[i++];
				off = ((off + 1) << 7) | (oc & 0x7f);
			}
			if (off > objOffset) {
				return Status::ErrorInvalidArguemnt;
			}
			raw.baseOffset = objOffset - off;
		} else if (type == OBJ_REF_DELTA) {
			if (i + oidSize > dataEnd) {
				return Status::ErrorInvalidArguemnt;
			}
			raw.baseOid.format = fmt;
			for (size_t k = 0; k < oidSize; ++k) { raw.baseOid.bytes[k] = p[i + k]; }
			i += oidSize;
		} else if (toObjectType(type) == ObjectType::Invalid) {
			return Status::ErrorInvalidArguemnt; // unknown object type
		}

		// inflate the object payload
		size_t consumed = 0;
		auto st = inflateStream(p + i, dataEnd - i, raw.data, consumed);
		if (st != Status::Ok) {
			return st;
		}
		i += consumed;

		if (type != OBJ_OFS_DELTA && type != OBJ_REF_DELTA) {
			// non-delta: materialize immediately
			raw.resolved = true;
			raw.finalType = toObjectType(type);
			raw.finalData = raw.data;
			raw.oid = hashObject(raw.finalType,
					BytesView(raw.finalData.data(), raw.finalData.size()), fmt);
			out.objects.emplace(raw.oid.str(), Object{raw.finalType, raw.finalData});
		}

		offsetToIndex.emplace(objOffset, objs.size());
		objs.emplace_back(sp::move(raw));
	}

	// resolve deltas until no further progress
	HashMap<String, size_t> oidToIndex;
	for (size_t k = 0; k < objs.size(); ++k) {
		if (objs[k].resolved) {
			oidToIndex.emplace(objs[k].oid.str(), k);
		}
	}

	size_t remaining = 0;
	for (auto &o : objs) {
		if (!o.resolved) {
			++remaining;
		}
	}

	while (remaining > 0) {
		bool progress = false;
		for (size_t k = 0; k < objs.size(); ++k) {
			RawObject &o = objs[k];
			if (o.resolved) {
				continue;
			}

			RawObject *base = nullptr;
			if (o.packType == OBJ_OFS_DELTA) {
				auto it = offsetToIndex.find(o.baseOffset);
				if (it != offsetToIndex.end()) {
					base = &objs[it->second];
				}
			} else if (o.packType == OBJ_REF_DELTA) {
				auto it = oidToIndex.find(StringView(o.baseOid.str()));
				if (it != oidToIndex.end()) {
					base = &objs[it->second];
				}
			}
			if (!base || !base->resolved) {
				continue;
			}

			auto st = applyDelta(BytesView(base->finalData.data(), base->finalData.size()),
					BytesView(o.data.data(), o.data.size()), o.finalData);
			if (st != Status::Ok) {
				return st;
			}
			o.finalType = base->finalType;
			o.oid = hashObject(o.finalType, BytesView(o.finalData.data(), o.finalData.size()), fmt);
			o.resolved = true;
			oidToIndex.emplace(o.oid.str(), k);
			out.objects.emplace(o.oid.str(), Object{o.finalType, o.finalData});
			progress = true;
			--remaining;
		}
		if (!progress) {
			return Status::ErrorNotFound; // unresolved delta base (thin pack / corruption)
		}
	}

	return Status::Ok;
}

} // namespace stappler::git
