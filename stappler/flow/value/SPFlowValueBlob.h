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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUEBLOB_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUEBLOB_H_

#include "SPFlowValue.h"
#include "SPVStoreArena.h"
#include "SPFlowValueVar.h"

namespace STAPPLER_VERSIONIZED stappler::flow::value {

// Deterministic geometric growth. Not a heuristic: the capacity has to be a pure function of the
// sequence of sizes a blob was given, or two stores that received the same operations would hold
// different bytes, and byte-identical stores is what the image comparisons rest on.
SP_PUBLIC uint32_t nextBlobCapacity(uint32_t current, uint32_t need);

// One element of a map's spine: 40 bytes, fixed, so a binary search needs no indirection.
// `pad` is named rather than implicit because it is zeroed and it is part of the image.
struct MapEntry {
	BlobHandle key;
	uint32_t pad;
	Var value;
};

static_assert(sizeof(MapEntry) == 40, "MapEntry is part of the image format");
static_assert(alignof(MapEntry) == 8, "MapEntry is part of the image format");

// Blob accessors. Every mutating call takes the address of the BlobHandle inside the owning record,
// not the handle itself: a resize is an allocator call, so the handle has to be read by value, the
// allocation performed, and the possibly-moved handle written back through a fresh Arena::write().
// No pointer outlives an allocator call, and doing it anywhere else would mean the caller had to. An Addr pointing into a
// blob - an array element, say - is invalidated by any resize of that blob, exactly as a host
// pointer is invalidated by any allocator call. The arena is a template parameter throughout, and
// every function below is instantiated once per arena kind at the bottom of SPFlowValue.scu.cpp: `A`
// is any of PlainArena / TrackedArena / ShadowArena, or ArenaRef where the kind has been erased,
// and they all offer the same members. A caller in graph/, ops/ or ext/ sees only these
// declarations, emits a plain external reference and links against the instantiation.
namespace blob {

template <typename A>
SP_PUBLIC Status stringAssign(A &, Addr handleAddr, StringView);
template <typename A>
SP_PUBLIC Status stringAppend(A &, Addr handleAddr, StringView);
template <typename A>
SP_PUBLIC Status stringResize(A &, Addr handleAddr, uint32_t size, uint8_t fill = 0);
template <typename A>
SP_PUBLIC uint32_t stringSize(const A &, Addr handleAddr);

// Read through a callback so the caller cannot store the pointer and outlive it.
template <typename A>
SP_PUBLIC void stringRead(const A &, Addr handleAddr, const Callback<void(StringView)> &);
template <typename A>
SP_PUBLIC mem_std::String stringGet(const A &, Addr handleAddr);

// The same storage as a string, and a different claim about it: a String is text, a Bytes is
// opaque. Nothing here reinterprets - the two differ only in the API type and in how the value
// projects into data::Value (CHARSTRING against BYTESTRING), which is exactly the distinction the
// caller wanted when they declared the field.
template <typename A>
SP_PUBLIC Status bytesAssign(A &, Addr handleAddr, BytesView);
template <typename A>
SP_PUBLIC Status bytesAppend(A &, Addr handleAddr, BytesView);
template <typename A>
SP_PUBLIC Status bytesResize(A &, Addr handleAddr, uint32_t size, uint8_t fill = 0);
template <typename A>
SP_PUBLIC uint32_t bytesSize(const A &, Addr handleAddr);
template <typename A>
SP_PUBLIC void bytesRead(const A &, Addr handleAddr, const Callback<void(BytesView)> &);
template <typename A>
SP_PUBLIC mem_std::Bytes bytesGet(const A &, Addr handleAddr);

// The handle itself, by value. Useful for inspecting capacity and for the typed span reader below;
// mutating anything through it is the accessors' job, not the caller's.
template <typename A>
SP_PUBLIC BlobHandle getHandle(const A &, Addr handleAddr);

template <typename A>
SP_PUBLIC uint32_t arrayCount(const A &, Addr handleAddr, ElementChain);
template <typename A>
SP_PUBLIC Status arrayResize(A &, Addr handleAddr, ElementChain, uint32_t count);
template <typename A>
SP_PUBLIC Status arrayGet(const A &, Addr handleAddr, ElementChain, uint32_t index, Var &out);
template <typename A>
SP_PUBLIC Status arraySet(A &, Addr handleAddr, ElementChain, uint32_t index, const Var &);
template <typename A>
SP_PUBLIC Status arrayPush(A &, Addr handleAddr, ElementChain, const Var &);

// The last element, taken off. Status::ErrorNotFound when there is none, which is the empty-stack
// answer rather than a failure. Exists next to arrayErase(count - 1) because the last element has
// no tail to move and no other index to shift, so it is worth the one path that says so.
template <typename A>
SP_PUBLIC Status arrayPop(A &, Addr handleAddr, ElementChain, Var &out);
template <typename A>
SP_PUBLIC Status arrayErase(A &, Addr handleAddr, ElementChain, uint32_t index);

// The address of an element's slot. For a container element that is the address of its nested
// BlobHandle, which is what the accessors above take - so nesting composes with no special case.
// Invalidated by any resize of the owning array.
template <typename A>
SP_PUBLIC Addr arrayElementAddr(const A &, Addr handleAddr, ElementChain, uint32_t index);

// Reading an array as a typed span. An array is one flat block of packed elements, so a reader can
// be handed the storage directly instead of a Var per element - that is the whole reason arrays
// are stored flat: walking 10 000 Vec3s should cost a pointer, not 10 000 24-byte values built and
// thrown away. The unit a span sees is the element's storage unit, not a struct wrapping it: a
// Vec3 is three floats because that is exactly how it is written.
//
//   Bool                       uint8_t     1 per element
//   Int, EntityRef, Enum       int64_t     1
//   Float                      double      1
//   Vec2 / Vec3 / Vec4, Color  float       2 / 3 / 4
//   Int32 / UInt32             int32_t / uint32_t  1
//   Float32                    float       1
//   String, Bytes, Array, Map  BlobHandle  1
//
// The match is by named unit rather than by size, because an Array<Int> and an Array<Float> are
// both eight bytes per element: `SpanView<int64_t>` over the latter would compile and silently
// reinterpret every value. A `T` with no unit above fails to compile at all.
enum class SpanUnit : uint8_t {
	None,
	U8,
	I64,
	F64,
	F32,
	Handle,
	I32,
	U32,
};

// The unit an element type is read as, and how many of them one element occupies.
SP_PUBLIC SpanUnit getSpanUnit(VarType element, uint32_t &outUnitsPerElement);

template <typename T>
struct SpanUnitOf;
template <>
struct SpanUnitOf<uint8_t> {
	static constexpr SpanUnit Value = SpanUnit::U8;
};
template <>
struct SpanUnitOf<int64_t> {
	static constexpr SpanUnit Value = SpanUnit::I64;
};
template <>
struct SpanUnitOf<double> {
	static constexpr SpanUnit Value = SpanUnit::F64;
};
template <>
struct SpanUnitOf<int32_t> {
	static constexpr SpanUnit Value = SpanUnit::I32;
};
template <>
struct SpanUnitOf<uint32_t> {
	static constexpr SpanUnit Value = SpanUnit::U32;
};
template <>
struct SpanUnitOf<float> {
	static constexpr SpanUnit Value = SpanUnit::F32;
};
template <>
struct SpanUnitOf<BlobHandle> {
	static constexpr SpanUnit Value = SpanUnit::Handle;
};

// Hands the array's storage to the callback as a contiguous span of `T`. Returns false without
// calling back when `T` is not the element type's unit; an empty array calls back with an empty
// span, the way stringRead does. The rule that no pointer outlives an allocator call applies with
// full force: the span points into the arena and
// dies at the next allocator call, so nothing inside the callback may add to, remove from or resize
// anything in this store - copy out first if you need to mutate. Alignment is free rather than
// assumed, every arena payload being 16-aligned unconditionally. `T` comes first and the arena kind
// is deduced, so a call site spells `arrayRead<float>(arena, ...)`: the element type is the one a
// caller has to state, and the store's kind is the one it already holds.
template <typename T, typename A>
bool arrayRead(const A &arena, Addr handleAddr, ElementChain chain,
		const Callback<void(SpanView<T>)> &cb) {
	uint32_t units = 0;
	if (getSpanUnit(chainHead(chain), units) != SpanUnitOf<T>::Value || units == 0) {
		return false;
	}

	auto h = getHandle(arena, handleAddr);
	auto stride = units * uint32_t(sizeof(T));
	auto count = h.size / stride;
	if (h.data == NullAddr || count == 0) {
		cb(SpanView<T>());
		return true;
	}
	if (auto src = arena.read(h.data, count * stride)) {
		cb(SpanView<T>(reinterpret_cast<const T *>(src), size_t(count) * units));
	} else {
		cb(SpanView<T>());
	}
	return true;
}

// The bytes of an array, whatever its element type - for hashing, checksumming or handing to a
// codec. The same lifetime rule as arrayRead.
template <typename A>
SP_PUBLIC bool arrayReadRaw(const A &, Addr handleAddr, ElementChain,
		const Callback<void(BytesView)> &);

template <typename A>
SP_PUBLIC uint32_t mapCount(const A &, Addr handleAddr);
template <typename A>
SP_PUBLIC bool mapFind(const A &, Addr handleAddr, StringView key, uint32_t &outIndex);
template <typename A>
SP_PUBLIC Status mapGet(const A &, Addr handleAddr, StringView key, Var &out);
template <typename A>
SP_PUBLIC Status mapSet(A &, Addr handleAddr, ElementChain, StringView key, const Var &);
// No element chain: what a value owns is described by the stored Var, not by the declaration.
template <typename A>
SP_PUBLIC Status mapErase(A &, Addr handleAddr, StringView key);
template <typename A>
SP_PUBLIC void mapKeyAt(const A &, Addr handleAddr, uint32_t index,
		const Callback<void(StringView)> &);
template <typename A>
SP_PUBLIC Status mapValueAt(const A &, Addr handleAddr, uint32_t index, Var &out);
template <typename A>
SP_PUBLIC Addr mapValueAddr(const A &, Addr handleAddr, uint32_t index);

// Lifecycle, shared. destroy walks the element chain, so a nested container frees everything below
// it. Takes the handle by value: the caller clears its own copy, which makes a second destroy a
// no-op rather than a double free.
template <typename A>
SP_PUBLIC void destroy(A &, BlobHandle, VarType, ElementChain);

// Deep copy. Re-resolves the source after every allocation in the destination - required even when
// the two arenas are the same one, because an allocation there moves the source's pointer too. Two
// parameters, because the two stores need not be the same kind: copying out of a debugger's arena
// into a release one is the same walk either way.
template <typename D, typename S>
SP_PUBLIC Status copy(D &dst, Addr dstHandleAddr, const S &src, Addr srcHandleAddr, VarType,
		ElementChain);

template <typename A>
SP_PUBLIC void encode(const A &, Addr handleAddr, VarType, ElementChain, mem_std::Value &out);
template <typename A>
SP_PUBLIC Status decode(A &, Addr handleAddr, VarType, ElementChain, const mem_std::Value &);

} // namespace blob

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUEBLOB_H_ */
