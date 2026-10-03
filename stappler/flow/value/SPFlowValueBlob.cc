/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Strings, arrays and maps in the arena. A blob is one flat block: above MaxInChunkPayload the
// arena gives out an oversize run and this layer never notices, so there is no rope and no second
// representation to keep in step. The cost that buys: an
// oversize realloc always relocates and a pool never returns an individual block, so repeated
// oversize churn grows the host pool. Three invariants govern every function here. No
// pointer from read()/write() is held across a call that can allocate, and a resize is one, hence
// the shape repeated below - read the handle by value, allocate, write the handle back through a
// fresh write(); a stale pointer here does not crash, it silently returns another block's bytes,
// which is why the torture test compares against a shadow model after every op. Every mutated
// byte comes from Arena::write() over a range covering the whole mutation, so a memmove inside a
// spine takes one write() spanning source and destination together. BLOB-Z: [size, capacity) is
// zero at all times, the arena only zeroing the granule padding beyond the size it was asked for
// while this layer asks for `capacity`.

#include "SPFlowValueBlob.h"

#include <sprt/c/__sprt_string.h>
#include <sprt/runtime/utils/base64.h>

namespace STAPPLER_VERSIONIZED stappler::flow::value {

uint32_t nextBlobCapacity(uint32_t current, uint32_t need) {
	uint32_t want = need;
	uint32_t grown = current + current / 2;
	if (grown > want) {
		want = grown;
	}
	if (want < MinPayload) {
		want = MinPayload;
	}
	// Granule-aligned with a floor of MinPayload means capacity == Arena::sizeOf(data) exactly, so
	// there is no invisible slack the barrier does not cover.
	return alignUp(want, Granule);
}

namespace blob {

template <typename A>
static BlobHandle readHandle(const A &arena, Addr handleAddr) {
	BlobHandle h;
	if (auto src = arena.read(handleAddr, uint32_t(sizeof(BlobHandle)))) {
		__sprt_memcpy(&h, src, sizeof(BlobHandle));
	}
	return h;
}

template <typename A>
static void writeHandle(A &arena, Addr handleAddr, const BlobHandle &h) {
	if (auto dst = arena.write(handleAddr, uint32_t(sizeof(BlobHandle)))) {
		__sprt_memcpy(dst, &h, sizeof(BlobHandle));
	}
}

// Grows the block so that `need` bytes fit, preserving the first `size` and zeroing the rest.
// Leaves `size` alone: what the bytes mean is the caller's business.
template <typename A>
static Status reserve(A &arena, Addr handleAddr, uint32_t need) {
	auto h = readHandle(arena, handleAddr); // by value - the pointer dies at the next line
	if (need <= h.capacity) {
		return Status::Ok;
	}

	auto cap = nextBlobCapacity(h.capacity, need);
	auto moved = h.data == NullAddr ? arena.alloc(cap) : arena.realloc(h.data, cap);
	if (moved == NullAddr) {
		return Status::ErrorOutOfHostMemory; // h untouched, the store is intact
	}

	h.data = moved;
	auto oldCapacity = h.capacity;
	h.capacity = cap;

	// BLOB-Z: the arena hands back dirty bytes, so the new tail is zeroed here and nowhere else.
	if (cap > oldCapacity) {
		if (auto dst = arena.write(h.data + oldCapacity, cap - oldCapacity)) {
			__sprt_memset(dst, 0, cap - oldCapacity);
		}
	}
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

// Shrinks the logical size, zeroing what was dropped. Capacity is kept: the next growth reuses it,
// and a blob grown then truncated is a genuinely different state from one that was never grown.
// Only the dropped bytes are zeroed, not everything up to the capacity - BLOB-Z already holds past
// `size`, since reserve() zeroes each growth and this zeroes each shrink, and rewriting the tail
// would take the barrier over a whole page and drag it into the next version for no effect on the
// content.
template <typename A>
static void truncateTo(A &arena, Addr handleAddr, uint32_t size) {
	auto h = readHandle(arena, handleAddr);
	if (size >= h.size) {
		return;
	}
	if (h.data != NullAddr) {
		if (auto dst = arena.write(h.data + size, h.size - size)) {
			__sprt_memset(dst, 0, h.size - size);
		}
	}
	h.size = size;
	writeHandle(arena, handleAddr, h);
}

template <typename A>
uint32_t stringSize(const A &arena, Addr handleAddr) {
	return readHandle(arena, handleAddr).size;
}

// The raw forms. Both claims - text and opaque - share them, and neither goes through a StringView
// on the way in: StringViewBase(ptr, len) stops at the first NUL, so routing bytes through one
// would silently truncate exactly the payloads a Bytes field exists to hold.
template <typename A>
static Status blobAssign(A &arena, Addr handleAddr, const uint8_t *data, uint32_t size) {
	auto st = reserve(arena, handleAddr, size);
	if (st != Status::Ok) {
		return st;
	}
	// Truncate first so the tail beyond the new size is zeroed, then write the new bytes.
	truncateTo(arena, handleAddr, 0);

	auto h = readHandle(arena, handleAddr);
	if (size > 0) {
		if (auto dst = arena.write(h.data, size)) {
			__sprt_memcpy(dst, data, size);
		}
	}
	h.size = size;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

template <typename A>
static Status blobAppend(A &arena, Addr handleAddr, const uint8_t *data, uint32_t size) {
	if (size == 0) {
		return Status::Ok;
	}
	auto before = readHandle(arena, handleAddr);
	auto st = reserve(arena, handleAddr, before.size + size);
	if (st != Status::Ok) {
		return st;
	}

	auto h = readHandle(arena, handleAddr); // re-resolved: reserve may have moved the block
	if (auto dst = arena.write(h.data + h.size, size)) {
		__sprt_memcpy(dst, data, size);
	}
	h.size += size;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

// Hands out the stored extent with no interpretation. Returns false when there is nothing there.
template <typename A>
static bool blobRead(const A &arena, Addr handleAddr, const uint8_t *&outData, uint32_t &outSize) {
	auto h = readHandle(arena, handleAddr);
	if (h.data == NullAddr || h.size == 0) {
		return false;
	}
	if (auto src = arena.read(h.data, h.size)) {
		outData = src;
		outSize = h.size;
		return true;
	}
	return false;
}

template <typename A>
Status stringAssign(A &arena, Addr handleAddr, StringView str) {
	return blobAssign(arena, handleAddr, reinterpret_cast<const uint8_t *>(str.data()),
			uint32_t(str.size()));
}

template <typename A>
Status stringAppend(A &arena, Addr handleAddr, StringView str) {
	return blobAppend(arena, handleAddr, reinterpret_cast<const uint8_t *>(str.data()),
			uint32_t(str.size()));
}

template <typename A>
Status stringResize(A &arena, Addr handleAddr, uint32_t size, uint8_t fill) {
	auto before = readHandle(arena, handleAddr);
	if (size < before.size) {
		truncateTo(arena, handleAddr, size);
		return Status::Ok;
	}
	if (size == before.size) {
		return Status::Ok;
	}

	auto st = reserve(arena, handleAddr, size);
	if (st != Status::Ok) {
		return st;
	}
	auto h = readHandle(arena, handleAddr);
	if (fill != 0) {
		if (auto dst = arena.write(h.data + h.size, size - h.size)) {
			__sprt_memset(dst, fill, size - h.size);
		}
	}
	h.size = size;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

// Note the asymmetry with bytesRead, and it is the point of having both: a StringView cannot carry
// an embedded NUL, so text stops there. A Bytes field is what holds a payload that can.
template <typename A>
void stringRead(const A &arena, Addr handleAddr, const Callback<void(StringView)> &cb) {
	const uint8_t *data = nullptr;
	uint32_t size = 0;
	if (!blobRead(arena, handleAddr, data, size)) {
		cb(StringView());
		return;
	}
	cb(StringView(reinterpret_cast<const char *>(data), size));
}

template <typename A>
mem_std::String stringGet(const A &arena, Addr handleAddr) {
	mem_std::String out;
	stringRead(arena, handleAddr, [&](StringView str) { out.assign(str.data(), str.size()); });
	return out;
}

// The bytes forms, forwarded to the string path rather than duplicated. A blob is bytes either way:
// what a String and a Bytes disagree about is the claim - text or opaque - and that difference
// lives entirely in the API type and in the projection. Two copies of the growth, the truncation
// and the BLOB-Z zeroing would be two places for the byte discipline to drift.

template <typename A>
BlobHandle getHandle(const A &arena, Addr handleAddr) {
	return readHandle(arena, handleAddr);
}

template <typename A>
uint32_t bytesSize(const A &arena, Addr handleAddr) {
	return readHandle(arena, handleAddr).size;
}

template <typename A>
Status bytesAssign(A &arena, Addr handleAddr, BytesView data) {
	return blobAssign(arena, handleAddr, data.data(), uint32_t(data.size()));
}

template <typename A>
Status bytesAppend(A &arena, Addr handleAddr, BytesView data) {
	return blobAppend(arena, handleAddr, data.data(), uint32_t(data.size()));
}

template <typename A>
Status bytesResize(A &arena, Addr handleAddr, uint32_t size, uint8_t fill) {
	return stringResize(arena, handleAddr, size, fill);
}

template <typename A>
void bytesRead(const A &arena, Addr handleAddr, const Callback<void(BytesView)> &cb) {
	const uint8_t *data = nullptr;
	uint32_t size = 0;
	if (!blobRead(arena, handleAddr, data, size)) {
		cb(BytesView());
		return;
	}
	cb(BytesView(data, size));
}

template <typename A>
mem_std::Bytes bytesGet(const A &arena, Addr handleAddr) {
	mem_std::Bytes out;
	bytesRead(arena, handleAddr,
			[&](BytesView data) { out.assign(data.data(), data.data() + data.size()); });
	return out;
}

SpanUnit getSpanUnit(VarType element, uint32_t &outUnitsPerElement) {
	outUnitsPerElement = 1;
	if (isContainerType(element)) {
		return SpanUnit::Handle;
	}
	switch (element) {
	case VarType::Bool: return SpanUnit::U8;
	case VarType::Int:
	case VarType::EntityRef:
	case VarType::Enum: return SpanUnit::I64;
	case VarType::Float: return SpanUnit::F64;
	case VarType::Int32: return SpanUnit::I32;
	case VarType::UInt32: return SpanUnit::U32;
	case VarType::Float32: return SpanUnit::F32;
	case VarType::Vec2: outUnitsPerElement = 2; return SpanUnit::F32;
	case VarType::Vec3: outUnitsPerElement = 3; return SpanUnit::F32;
	case VarType::Vec4:
	case VarType::Color: outUnitsPerElement = 4; return SpanUnit::F32;
	default: break;
	}
	outUnitsPerElement = 0;
	return SpanUnit::None;
}

// A container element occupies a BlobHandle; everything else its own layer-defined size.
static uint32_t elementStride(ElementChain chain) {
	auto head = chainHead(chain);
	if (head == VarType::Nil) {
		return 0;
	}
	if (isContainerType(head)) {
		return uint32_t(sizeof(BlobHandle));
	}
	switch (head) {
	case VarType::Bool: return 1;
	case VarType::Int:
	case VarType::Float:
	case VarType::EntityRef:
	case VarType::Enum: return 8;
	case VarType::Vec2: return 8;
	case VarType::Vec3: return 12;
	case VarType::Vec4:
	case VarType::Color: return 16;
	case VarType::Int32:
	case VarType::UInt32:
	case VarType::Float32: return 4;
	default: return 0;
	}
}

template <typename A>
uint32_t arrayCount(const A &arena, Addr handleAddr, ElementChain chain) {
	auto stride = elementStride(chain);
	if (stride == 0) {
		return 0;
	}
	return readHandle(arena, handleAddr).size / stride;
}

template <typename A>
bool arrayReadRaw(const A &arena, Addr handleAddr, ElementChain chain,
		const Callback<void(BytesView)> &cb) {
	auto stride = elementStride(chain);
	if (stride == 0) {
		return false;
	}
	auto h = readHandle(arena, handleAddr);
	// Rounded down to whole elements: the slack past the last one is BLOB-Z's zeros, not content.
	auto size = (h.size / stride) * stride;
	if (h.data == NullAddr || size == 0) {
		cb(BytesView());
		return true;
	}
	if (auto src = arena.read(h.data, size)) {
		cb(BytesView(src, size));
	} else {
		cb(BytesView());
	}
	return true;
}

template <typename A>
Addr arrayElementAddr(const A &arena, Addr handleAddr, ElementChain chain, uint32_t index) {
	auto stride = elementStride(chain);
	auto h = readHandle(arena, handleAddr);
	if (stride == 0 || h.data == NullAddr || (index + 1) * stride > h.size) {
		return NullAddr;
	}
	return h.data + index * stride;
}

template <typename A>
Status arrayResize(A &arena, Addr handleAddr, ElementChain chain, uint32_t count) {
	auto stride = elementStride(chain);
	if (stride == 0) {
		return Status::ErrorInvalidArguemnt;
	}
	auto head = chainHead(chain);
	auto tail = chainTail(chain);
	auto before = arrayCount(arena, handleAddr, chain);

	// Shrinking a container array frees what falls off the end first: the handles are about to be
	// zeroed, and nothing else owns those blocks.
	if (count < before && isContainerType(head)) {
		for (uint32_t i = count; i < before; ++i) {
			auto slot = arrayElementAddr(arena, handleAddr, chain, i);
			if (slot == NullAddr) {
				continue;
			}
			auto nested = readHandle(arena, slot);
			destroy(arena, nested, head, tail);
		}
	}

	if (count <= before) {
		truncateTo(arena, handleAddr, count * stride);
		return Status::Ok;
	}

	auto st = reserve(arena, handleAddr, count * stride);
	if (st != Status::Ok) {
		return st;
	}
	// The grown region is already zero by BLOB-Z, which is the right initial value for every
	// element type: a zero BlobHandle is an empty nested container.
	auto h = readHandle(arena, handleAddr);
	h.size = count * stride;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

// One element out of bytes that are already in hand, and one element into them. Factored out of
// arrayGet/arraySet so that the push and pop below can reach an element having read the handle
// once: the switch is the cheap half of either accessor, and the handle read is the half worth
// saving.
static Status loadElement(const uint8_t *src, ElementChain chain, VarType head, uint32_t stride,
		Var &out);
static Status storeElement(uint8_t *dst, VarType head, uint32_t stride, const Var &value);

template <typename A>
Status arrayGet(const A &arena, Addr handleAddr, ElementChain chain, uint32_t index, Var &out) {
	SP_FLOW_VALUE_COUNT(arrayGet);
	auto slot = arrayElementAddr(arena, handleAddr, chain, index);
	if (slot == NullAddr) {
		return Status::ErrorNotFound;
	}
	auto head = chainHead(chain);
	auto stride = elementStride(chain);
	auto src = arena.read(slot, stride);
	if (!src) {
		return Status::ErrorInvalidArguemnt;
	}
	return loadElement(src, chain, head, stride, out);
}

static Status loadElement(const uint8_t *src, ElementChain chain, VarType head, uint32_t stride,
		Var &out) {
	if (isContainerType(head)) {
		BlobHandle nested;
		__sprt_memcpy(&nested, src, sizeof(nested));
		out = makeBlob(head, chainTail(chain), nested);
		return Status::Ok;
	}

	switch (head) {
	case VarType::Bool: out = makeBool(src[0] != 0); return Status::Ok;
	case VarType::Int: {
		int64_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeInt(v);
		return Status::Ok;
	}
	case VarType::Float: {
		double v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeFloat(v);
		return Status::Ok;
	}
	case VarType::Int32: {
		int32_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeInt32(v);
		return Status::Ok;
	}
	case VarType::UInt32: {
		uint32_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeUInt32(v);
		return Status::Ok;
	}
	case VarType::Float32: {
		float v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeFloat32(v);
		return Status::Ok;
	}
	case VarType::EntityRef: {
		uint64_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeEntityRef(EntityId::unpack(v));
		return Status::Ok;
	}
	case VarType::Enum: {
		int64_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeEnum(v);
		return Status::Ok;
	}
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: {
		float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		__sprt_memcpy(v, src, stride);
		switch (head) {
		case VarType::Vec2: out = makeVec2(v[0], v[1]); break;
		case VarType::Vec3: out = makeVec3(v[0], v[1], v[2]); break;
		case VarType::Vec4: out = makeVec4(v[0], v[1], v[2], v[3]); break;
		default: out = makeColor(v[0], v[1], v[2], v[3]); break;
		}
		return Status::Ok;
	}
	default: break;
	}
	return Status::ErrorInvalidArguemnt;
}

template <typename A>
Status arraySet(A &arena, Addr handleAddr, ElementChain chain, uint32_t index, const Var &value) {
	SP_FLOW_VALUE_COUNT(arraySet);
	auto head = chainHead(chain);
	// A container element owns its block; assigning a handle over it would orphan what was there.
	// Reach it with arrayElementAddr() and use the blob accessors instead.
	if (isContainerType(head)) {
		return Status::ErrorInvalidArguemnt;
	}
	if (value.type != head) {
		return Status::ErrorInvalidArguemnt;
	}

	auto slot = arrayElementAddr(arena, handleAddr, chain, index);
	if (slot == NullAddr) {
		return Status::ErrorNotFound;
	}
	auto stride = elementStride(chain);
	auto dst = arena.write(slot, stride);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	return storeElement(dst, head, stride, value);
}

static Status storeElement(uint8_t *dst, VarType head, uint32_t stride, const Var &value) {
	switch (head) {
	case VarType::Bool: dst[0] = value.i != 0 ? 1 : 0; return Status::Ok;
	case VarType::Int: __sprt_memcpy(dst, &value.i, sizeof(int64_t)); return Status::Ok;
	case VarType::Float: __sprt_memcpy(dst, &value.f, sizeof(double)); return Status::Ok;
	case VarType::EntityRef: __sprt_memcpy(dst, &value.ent.id, sizeof(uint64_t)); return Status::Ok;
	case VarType::Enum: __sprt_memcpy(dst, &value.e.value, sizeof(int64_t)); return Status::Ok;
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: __sprt_memcpy(dst, value.v, stride); return Status::Ok;
	case VarType::Int32: {
		auto v = int32_t(value.i);
		__sprt_memcpy(dst, &v, sizeof(v));
		return Status::Ok;
	}
	case VarType::UInt32: {
		auto v = uint32_t(value.i);
		__sprt_memcpy(dst, &v, sizeof(v));
		return Status::Ok;
	}
	case VarType::Float32: __sprt_memcpy(dst, &value.v[0], sizeof(float)); return Status::Ok;
	default: break;
	}
	return Status::ErrorInvalidArguemnt;
}

// Append, having read the handle once. The general spelling - count, resize, set - reads the same
// twelve bytes five times over, because each step is written to stand alone; a push is the
// interpreter's commonest store mutation, every unit of work entering the ready queue being one, so
// it gets the version that does not. A container element is left to the general path deliberately:
// it owns a block, arraySet refuses to assign over one, and reproducing that refusal here would be
// a second implementation of it.
template <typename A>
Status arrayPush(A &arena, Addr handleAddr, ElementChain chain, const Var &value) {
	auto head = chainHead(chain);
	auto stride = elementStride(chain);
	if (stride == 0 || isContainerType(head) || value.type != head) {
		auto count = arrayCount(arena, handleAddr, chain);
		auto st = arrayResize(arena, handleAddr, chain, count + 1);
		if (st != Status::Ok) {
			return st;
		}
		return arraySet(arena, handleAddr, chain, count, value);
	}

	SP_FLOW_VALUE_COUNT(arraySet);
	auto h = readHandle(arena, handleAddr);
	// Rounded down to whole elements, exactly as arrayCount does: a size that is not a multiple of
	// the stride is a corrupt blob, and the append must land where the general path would have put
	// it.
	auto off = (h.size / stride) * stride;
	if (off + stride > h.capacity) {
		auto st = reserve(arena, handleAddr, off + stride);
		if (st != Status::Ok) {
			return st;
		}
		h = readHandle(arena, handleAddr); // re-resolved: reserve may have moved the block
	}

	auto dst = arena.write(h.data + off, stride);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	auto st = storeElement(dst, head, stride, value);
	if (st != Status::Ok) {
		return st;
	}
	h.size = off + stride;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

// Take the last element off, having read the handle once. The mirror of arrayPush, and the same
// argument. The element is fetched through write() rather than read() because the bytes are about
// to be zeroed anyway, so one barrier call covers both and the value is taken out before it goes.
template <typename A>
Status arrayPop(A &arena, Addr handleAddr, ElementChain chain, Var &out) {
	SP_FLOW_VALUE_COUNT(arrayGet);
	auto stride = elementStride(chain);
	if (stride == 0) {
		return Status::ErrorInvalidArguemnt;
	}
	auto h = readHandle(arena, handleAddr);
	auto count = h.size / stride;
	if (h.data == NullAddr || count == 0) {
		return Status::ErrorNotFound;
	}

	auto head = chainHead(chain);
	auto off = (count - 1) * stride;
	if (isContainerType(head)) {
		// The element owns a block, and dropping the handle without freeing it would leak it.
		auto nested = readHandle(arena, h.data + off);
		destroy(arena, nested, head, chainTail(chain));
		h = readHandle(arena, handleAddr); // destroy() allocates, so the block may have moved
	}

	auto raw = arena.write(h.data + off, stride);
	if (!raw) {
		return Status::ErrorInvalidArguemnt;
	}
	auto st = loadElement(raw, chain, head, stride, out);
	if (st != Status::Ok) {
		return st;
	}
	// BLOB-Z: what is past the size is zeros, and this is where the dropped element becomes past.
	__sprt_memset(raw, 0, stride);
	h.size = off;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

template <typename A>
Status arrayErase(A &arena, Addr handleAddr, ElementChain chain, uint32_t index) {
	auto stride = elementStride(chain);
	auto count = arrayCount(arena, handleAddr, chain);
	if (stride == 0 || index >= count) {
		return Status::ErrorNotFound;
	}

	auto head = chainHead(chain);
	if (isContainerType(head)) {
		auto slot = arrayElementAddr(arena, handleAddr, chain, index);
		auto nested = readHandle(arena, slot);
		destroy(arena, nested, head, chainTail(chain));
	}

	auto h = readHandle(arena, handleAddr);
	auto tailBytes = (count - index - 1) * stride;
	if (tailBytes > 0) {
		// One write() spanning source and destination together: marking only the destination would
		// leave the journal without the bytes it needs to put the source back.
		if (auto dst = arena.write(h.data + index * stride, tailBytes + stride)) {
			__sprt_memmove(dst, dst + stride, tailBytes);
		}
	}
	truncateTo(arena, handleAddr, (count - 1) * stride);
	return Status::Ok;
}

template <typename A>
uint32_t mapCount(const A &arena, Addr handleAddr) {
	return readHandle(arena, handleAddr).size / uint32_t(sizeof(MapEntry));
}

template <typename A>
static Addr mapEntryAddr(const A &arena, Addr handleAddr, uint32_t index) {
	auto h = readHandle(arena, handleAddr);
	if (h.data == NullAddr || (index + 1) * sizeof(MapEntry) > h.size) {
		return NullAddr;
	}
	return h.data + index * uint32_t(sizeof(MapEntry));
}

template <typename A>
static MapEntry readEntry(const A &arena, Addr handleAddr, uint32_t index) {
	MapEntry e{};
	auto addr = mapEntryAddr(arena, handleAddr, index);
	if (addr != NullAddr) {
		if (auto src = arena.read(addr, uint32_t(sizeof(MapEntry)))) {
			__sprt_memcpy(&e, src, sizeof(MapEntry));
		}
	}
	return e;
}

// Unsigned lexicographic, shorter-first on a prefix: locale-free and endian-free, which is what
// makes the spine's order a function of the keys alone.
template <typename A>
static int compareKey(const A &arena, const BlobHandle &key, StringView probe) {
	StringView stored;
	if (key.data != NullAddr && key.size > 0) {
		if (auto src = arena.read(key.data, key.size)) {
			stored = StringView(reinterpret_cast<const char *>(src), key.size);
		}
	}
	auto n = stored.size() < probe.size() ? stored.size() : probe.size();
	if (n > 0) {
		auto r = __builtin_memcmp(stored.data(), probe.data(), n);
		if (r != 0) {
			return r < 0 ? -1 : 1;
		}
	}
	if (stored.size() == probe.size()) {
		return 0;
	}
	return stored.size() < probe.size() ? -1 : 1;
}

// Returns the index where `key` is, or where it would be inserted. No allocation happens inside, so
// the reads are safe for its duration.
template <typename A>
static uint32_t mapLowerBound(const A &arena, Addr handleAddr, StringView key, bool &found) {
	found = false;
	uint32_t lo = 0;
	uint32_t hi = mapCount(arena, handleAddr);
	while (lo < hi) {
		auto mid = lo + (hi - lo) / 2;
		auto entry = readEntry(arena, handleAddr, mid);
		auto r = compareKey(arena, entry.key, key);
		if (r == 0) {
			found = true;
			return mid;
		}
		if (r < 0) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

template <typename A>
bool mapFind(const A &arena, Addr handleAddr, StringView key, uint32_t &outIndex) {
	bool found = false;
	auto at = mapLowerBound(arena, handleAddr, key, found);
	if (found) {
		outIndex = at;
	}
	return found;
}

template <typename A>
Status mapGet(const A &arena, Addr handleAddr, StringView key, Var &out) {
	uint32_t index = 0;
	if (!mapFind(arena, handleAddr, key, index)) {
		return Status::ErrorNotFound;
	}
	out = readEntry(arena, handleAddr, index).value;
	return Status::Ok;
}

template <typename A>
void mapKeyAt(const A &arena, Addr handleAddr, uint32_t index,
		const Callback<void(StringView)> &cb) {
	auto entry = readEntry(arena, handleAddr, index);
	if (entry.key.data == NullAddr || entry.key.size == 0) {
		cb(StringView());
		return;
	}
	if (auto src = arena.read(entry.key.data, entry.key.size)) {
		cb(StringView(reinterpret_cast<const char *>(src), entry.key.size));
	} else {
		cb(StringView());
	}
}

template <typename A>
Status mapValueAt(const A &arena, Addr handleAddr, uint32_t index, Var &out) {
	if (mapEntryAddr(arena, handleAddr, index) == NullAddr) {
		return Status::ErrorNotFound;
	}
	out = readEntry(arena, handleAddr, index).value;
	return Status::Ok;
}

template <typename A>
Addr mapValueAddr(const A &arena, Addr handleAddr, uint32_t index) {
	auto addr = mapEntryAddr(arena, handleAddr, index);
	// The value's own BlobHandle sits inside its Var, in the container variant's first 12 bytes.
	return addr == NullAddr ? NullAddr
							: addr + uint32_t(offsetof(MapEntry, value) + offsetof(Var, c.blob));
}

template <typename A>
Status mapSet(A &arena, Addr handleAddr, ElementChain chain, StringView key, const Var &value) {
	// A declared map constrains its values; a heterogeneous one (an empty chain) takes any shape
	// and relies on each Var describing itself. Checked here rather than at the schema level
	// because a nested map's declaration lives in its owner's chain and nowhere else.
	auto declared = chainHead(chain);
	if (declared != VarType::Nil && value.type != declared) {
		return Status::ErrorInvalidArguemnt;
	}

	bool found = false;
	auto at = mapLowerBound(arena, handleAddr, key, found);

	if (found) {
		auto entry = readEntry(arena, handleAddr, at);
		// Replacing a container value frees what it owned; nothing else points at that block.
		if (isContainerType(entry.value.type)) {
			destroy(arena, entry.value.c.blob, entry.value.type, entry.value.c.elem);
		}
		entry.value = value;
		auto addr = mapEntryAddr(arena, handleAddr, at);
		if (auto dst = arena.write(addr, uint32_t(sizeof(MapEntry)))) {
			__sprt_memcpy(dst, &entry, sizeof(MapEntry));
		}
		return Status::Ok;
	}

	// The key gets its own block. Allocated first, so a failure here leaves the spine untouched.
	BlobHandle keyHandle;
	if (!key.empty()) {
		auto cap = nextBlobCapacity(0, uint32_t(key.size()));
		keyHandle.data = arena.alloc(cap);
		if (keyHandle.data == NullAddr) {
			return Status::ErrorOutOfHostMemory;
		}
		keyHandle.capacity = cap;
		keyHandle.size = uint32_t(key.size());
		if (auto dst = arena.write(keyHandle.data, cap)) {
			__sprt_memset(dst, 0, cap); // BLOB-Z for the slack past the key
			__sprt_memcpy(dst, key.data(), key.size());
		}
	}

	auto count = mapCount(arena, handleAddr);
	auto st = reserve(arena, handleAddr, (count + 1) * uint32_t(sizeof(MapEntry)));
	if (st != Status::Ok) {
		arena.free(keyHandle.data);
		return st;
	}

	auto h = readHandle(arena, handleAddr); // re-resolved after the growth
	auto entrySize = uint32_t(sizeof(MapEntry));
	auto tailCount = count - at;
	if (tailCount > 0) {
		if (auto dst = arena.write(h.data + at * entrySize, (tailCount + 1) * entrySize)) {
			__sprt_memmove(dst + entrySize, dst, tailCount * entrySize);
		}
	}

	MapEntry entry{};
	entry.key = keyHandle;
	entry.pad = 0;
	entry.value = value;
	if (auto dst = arena.write(h.data + at * entrySize, entrySize)) {
		__sprt_memcpy(dst, &entry, sizeof(MapEntry));
	}

	h.size = (count + 1) * entrySize;
	writeHandle(arena, handleAddr, h);
	return Status::Ok;
}

template <typename A>
Status mapErase(A &arena, Addr handleAddr, StringView key) {
	uint32_t at = 0;
	if (!mapFind(arena, handleAddr, key, at)) {
		return Status::ErrorNotFound;
	}

	auto entry = readEntry(arena, handleAddr, at);
	arena.free(entry.key.data);
	if (isContainerType(entry.value.type)) {
		destroy(arena, entry.value.c.blob, entry.value.type, entry.value.c.elem);
	}

	auto count = mapCount(arena, handleAddr);
	auto entrySize = uint32_t(sizeof(MapEntry));
	auto h = readHandle(arena, handleAddr);
	auto tailCount = count - at - 1;
	if (tailCount > 0) {
		if (auto dst = arena.write(h.data + at * entrySize, (tailCount + 1) * entrySize)) {
			__sprt_memmove(dst, dst + entrySize, tailCount * entrySize);
		}
	}
	truncateTo(arena, handleAddr, (count - 1) * entrySize);
	return Status::Ok;
}

template <typename A>
void destroy(A &arena, BlobHandle h, VarType type, ElementChain chain) {
	if (h.data == NullAddr) {
		return;
	}

	// Free the children before the block that describes them: reading a handle out of a block we
	// have already released would be reading freed memory.
	if (type == VarType::Array) {
		auto head = chainHead(chain);
		if (isContainerType(head)) {
			auto stride = uint32_t(sizeof(BlobHandle));
			auto count = h.size / stride;
			for (uint32_t i = 0; i < count; ++i) {
				BlobHandle nested;
				if (auto src = arena.read(h.data + i * stride, stride)) {
					__sprt_memcpy(&nested, src, sizeof(nested));
					destroy(arena, nested, head, chainTail(chain));
				}
			}
		}
	} else if (type == VarType::Map) {
		auto entrySize = uint32_t(sizeof(MapEntry));
		auto count = h.size / entrySize;
		for (uint32_t i = 0; i < count; ++i) {
			MapEntry e{};
			if (auto src = arena.read(h.data + i * entrySize, entrySize)) {
				__sprt_memcpy(&e, src, sizeof(MapEntry));
			}
			arena.free(e.key.data);
			if (isContainerType(e.value.type)) {
				destroy(arena, e.value.c.blob, e.value.type, e.value.c.elem);
			}
		}
	}

	arena.free(h.data);
}

template <typename D, typename S>
Status copy(D &dst, Addr dstHandleAddr, const S &src, Addr srcHandleAddr, VarType type,
		ElementChain chain) {
	// Whatever the destination held is replaced, not merged.
	auto existing = readHandle(dst, dstHandleAddr);
	if (existing.data != NullAddr) {
		destroy(dst, existing, type, chain);
		writeHandle(dst, dstHandleAddr, BlobHandle{});
	}

	if (type == VarType::String || type == VarType::Bytes) {
		// Copied out first: the allocation inside the assign would move the source if the two
		// handles live in the same arena. Through the byte form for both claims - a String's
		// content is bytes too, and routing it through a StringView would stop at a NUL.
		mem_std::Bytes payload;
		bytesRead(src, srcHandleAddr,
				[&](BytesView d) { payload.assign(d.data(), d.data() + d.size()); });
		return bytesAssign(dst, dstHandleAddr, BytesView(payload.data(), payload.size()));
	}

	if (type == VarType::Array) {
		auto head = chainHead(chain);
		auto tail = chainTail(chain);
		auto count = arrayCount(src, srcHandleAddr, chain);
		auto st = arrayResize(dst, dstHandleAddr, chain, count);
		if (st != Status::Ok) {
			return st;
		}
		for (uint32_t i = 0; i < count; ++i) {
			if (isContainerType(head)) {
				// Both addresses are re-resolved every iteration: the nested copy allocates, and
				// that invalidates the element addresses on both sides when the arenas are one.
				auto srcSlot = arrayElementAddr(src, srcHandleAddr, chain, i);
				auto dstSlot = arrayElementAddr(dst, dstHandleAddr, chain, i);
				if (srcSlot == NullAddr || dstSlot == NullAddr) {
					return Status::ErrorInvalidArguemnt;
				}
				st = copy(dst, dstSlot, src, srcSlot, head, tail);
			} else {
				Var value;
				st = arrayGet(src, srcHandleAddr, chain, i, value);
				if (st == Status::Ok) {
					st = arraySet(dst, dstHandleAddr, chain, i, value);
				}
			}
			if (st != Status::Ok) {
				return st;
			}
		}
		return Status::Ok;
	}

	if (type == VarType::Map) {
		auto count = mapCount(src, srcHandleAddr);
		for (uint32_t i = 0; i < count; ++i) {
			mem_std::String key;
			mapKeyAt(src, srcHandleAddr, i, [&](StringView s) { key.assign(s.data(), s.size()); });
			Var value;
			if (mapValueAt(src, srcHandleAddr, i, value) != Status::Ok) {
				return Status::ErrorInvalidArguemnt;
			}

			if (isContainerType(value.type)) {
				// Insert an empty container first, then deep-copy into it: the value's handle must
				// be the destination's own, never the source's.
				auto placeholder = makeBlob(value.type, value.c.elem, BlobHandle{});
				auto st = mapSet(dst, dstHandleAddr, chain, StringView(key.data(), key.size()),
						placeholder);
				if (st != Status::Ok) {
					return st;
				}
				uint32_t at = 0;
				if (!mapFind(dst, dstHandleAddr, StringView(key.data(), key.size()), at)) {
					return Status::ErrorInvalidArguemnt;
				}
				auto dstSlot = mapValueAddr(dst, dstHandleAddr, at);
				auto srcSlot = mapValueAddr(src, srcHandleAddr, i);
				st = copy(dst, dstSlot, src, srcSlot, value.type, value.c.elem);
				if (st != Status::Ok) {
					return st;
				}
			} else {
				auto st = mapSet(dst, dstHandleAddr, chain, StringView(key.data(), key.size()),
						value);
				if (st != Status::Ok) {
					return st;
				}
			}
		}
		return Status::Ok;
	}

	return Status::ErrorInvalidArguemnt;
}

template <typename A>
void encode(const A &arena, Addr handleAddr, VarType type, ElementChain chain,
		mem_std::Value &out) {
	if (type == VarType::String) {
		stringRead(arena, handleAddr, [&](StringView s) { out.setString(s); });
		return;
	}

	if (type == VarType::Bytes) {
		bytesRead(arena, handleAddr, [&](BytesView data) { out.setBytes(data); });
		return;
	}

	if (type == VarType::Array) {
		out = mem_std::Value(mem_std::Value::Type::ARRAY);
		auto head = chainHead(chain);
		auto count = arrayCount(arena, handleAddr, chain);
		for (uint32_t i = 0; i < count; ++i) {
			mem_std::Value entry;
			if (isContainerType(head)) {
				auto slot = arrayElementAddr(arena, handleAddr, chain, i);
				encode(arena, slot, head, chainTail(chain), entry);
			} else {
				Var value;
				if (arrayGet(arena, handleAddr, chain, i, value) == Status::Ok) {
					encodeVar(value, entry);
				}
			}
			out.addValue(sprt::move(entry));
		}
		return;
	}

	if (type == VarType::Map) {
		out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
		auto count = mapCount(arena, handleAddr);
		for (uint32_t i = 0; i < count; ++i) {
			mem_std::String key;
			mapKeyAt(arena, handleAddr, i, [&](StringView s) { key.assign(s.data(), s.size()); });
			Var value;
			if (mapValueAt(arena, handleAddr, i, value) != Status::Ok) {
				continue;
			}
			mem_std::Value entry;
			if (isContainerType(value.type)) {
				encode(arena, mapValueAddr(arena, handleAddr, i), value.type, value.c.elem, entry);
			} else {
				encodeVar(value, entry);
			}
			out.setValue(sprt::move(entry), StringView(key.data(), key.size()));
		}
		return;
	}
}

template <typename A>
Status decode(A &arena, Addr handleAddr, VarType type, ElementChain chain,
		const mem_std::Value &value) {
	if (type == VarType::String) {
		if (!value.isString()) {
			return Status::ErrorInvalidArguemnt;
		}
		return stringAssign(arena, handleAddr, value.getString());
	}

	if (type == VarType::Bytes) {
		if (value.isBytes()) {
			return bytesAssign(arena, handleAddr, value.getBytes());
		}
		// The engine's JSON writer emits a BYTESTRING as "BASE64:<base64url>" and its JSON reader
		// does not turn that back into bytes, so a scene that went out as JSON comes back with a
		// CHARSTRING where a Bytes field expects bytes. Repaired here, at the one seam that knows
		// what the field is. Only that exact form is accepted: a bare string is not silently taken
		// as its own bytes, which would make a typo in an asset load as data instead of failing.
		StringView text(value.getString());
		if (text.starts_with("BASE64:")) {
			auto payload = text.sub(7);
			Status st = Status::ErrorInvalidArguemnt;
			sprt::base64::decode(payload.data(), payload.size(),
					[&](const uint8_t *data, size_t size) {
				st = bytesAssign(arena, handleAddr, BytesView(data, size));
			});
			return st;
		}
		return Status::ErrorInvalidArguemnt;
	}

	if (type == VarType::Array) {
		if (!value.isArray()) {
			return Status::ErrorInvalidArguemnt;
		}
		auto head = chainHead(chain);
		auto count = uint32_t(value.size());
		auto st = arrayResize(arena, handleAddr, chain, count);
		if (st != Status::Ok) {
			return st;
		}
		for (uint32_t i = 0; i < count; ++i) {
			auto &entry = value.getValue(i);
			if (isContainerType(head)) {
				auto slot = arrayElementAddr(arena, handleAddr, chain, i);
				st = decode(arena, slot, head, chainTail(chain), entry);
			} else {
				Var decoded;
				st = decodeVar(entry, head, decoded);
				if (st == Status::Ok) {
					st = arraySet(arena, handleAddr, chain, i, decoded);
				}
			}
			if (st != Status::Ok) {
				return st;
			}
		}
		return Status::Ok;
	}

	if (type == VarType::Map) {
		if (!value.isDictionary()) {
			return Status::ErrorInvalidArguemnt;
		}
		auto head = chainHead(chain);
		for (auto &it : value.asDict()) {
			StringView key(it.first);
			if (isContainerType(head)) {
				auto placeholder = makeBlob(head, chainTail(chain), BlobHandle{});
				auto st = mapSet(arena, handleAddr, chain, key, placeholder);
				if (st != Status::Ok) {
					return st;
				}
				uint32_t at = 0;
				if (!mapFind(arena, handleAddr, key, at)) {
					return Status::ErrorInvalidArguemnt;
				}
				st = decode(arena, mapValueAddr(arena, handleAddr, at), head, chainTail(chain),
						it.second);
				if (st != Status::Ok) {
					return st;
				}
			} else {
				// A heterogeneous map takes the value's own shape; a declared one takes the
				// element type the schema gave it.
				auto want = head == VarType::Nil ? VarType::Nil : head;
				if (want == VarType::Nil) {
					if (it.second.isBool()) {
						want = VarType::Bool;
					} else if (it.second.isInteger()) {
						want = VarType::Int;
					} else if (it.second.isDouble()) {
						want = VarType::Float;
					} else {
						return Status::ErrorInvalidArguemnt;
					}
				}
				Var decoded;
				auto st = decodeVar(it.second, want, decoded);
				if (st != Status::Ok) {
					return st;
				}
				st = mapSet(arena, handleAddr, chain, key, decoded);
				if (st != Status::Ok) {
					return st;
				}
			}
		}
		return Status::Ok;
	}

	return Status::ErrorInvalidArguemnt;
}

} // namespace blob

} // namespace stappler::flow::value
