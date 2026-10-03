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

// Compile unit for stappler_flow_value: the .cc files below are include-only subunits and are never
// compiled on their own. Order matters - append, do not reorganize.

#include "SPCommon.h"

#include "SPFlowValue.h"
#include "SPFlowValueDiag.h"

#include "SPFlowValueCounters.cc"
#include "SPFlowValueVar.cc"
#include "SPFlowValueDecl.cc"
#include "SPFlowValueBlob.cc"
#include "SPFlowValueSchema.cc"

// The instantiations. Everything above that is a template over the arena kind is defined in a
// subunit and therefore invisible outside this compile unit; the layers above see only the
// declarations, emit plain external references, and link against what is emitted here. No
// attribute goes on any of these lines: an attribute list cannot appear in explicit-instantiation
// position and SP_PUBLIC expands to one, so visibility is carried by SP_PUBLIC on the declarations.
// They come after every definition, because blob:: is defined before ComponentType calls it.

namespace STAPPLER_VERSIONIZED stappler::flow::value {

// The kinds, plus the erased one. ShadowArena is an alias of TrackedArena in a release build, so
// naming it there would instantiate the same specialization twice.
#if DEBUG
#define SP_FLOW_VALUE_FOR_EACH_ARENA(M) M(PlainArena) M(TrackedArena) M(ShadowArena) M(ArenaRef)
#else
#define SP_FLOW_VALUE_FOR_EACH_ARENA(M) M(PlainArena) M(TrackedArena) M(ArenaRef)
#endif

// The cross product, for the calls that read one store and write another: carrying a record out of
// a debugger's arena into a release one is an ordinary copy.
#define SP_FLOW_VALUE_PAIRS_WITH(M, D) M(D, PlainArena) M(D, TrackedArena) M(D, ShadowArena) M(D, ArenaRef)

#if DEBUG
#define SP_FLOW_VALUE_FOR_EACH_ARENA_PAIR(M) \
	SP_FLOW_VALUE_PAIRS_WITH(M, PlainArena) \
	SP_FLOW_VALUE_PAIRS_WITH(M, TrackedArena) \
	SP_FLOW_VALUE_PAIRS_WITH(M, ShadowArena) \
	SP_FLOW_VALUE_PAIRS_WITH(M, ArenaRef)
#else
#define SP_FLOW_VALUE_PAIRS_RELEASE(M, D) M(D, PlainArena) M(D, TrackedArena) M(D, ArenaRef)
#define SP_FLOW_VALUE_FOR_EACH_ARENA_PAIR(M) \
	SP_FLOW_VALUE_PAIRS_RELEASE(M, PlainArena) \
	SP_FLOW_VALUE_PAIRS_RELEASE(M, TrackedArena) \
	SP_FLOW_VALUE_PAIRS_RELEASE(M, ArenaRef)
#endif

// blob::, in full: the tests reach almost all of it directly and an extension may reach any of it.
#define SP_FLOW_VALUE_INSTANTIATE_BLOB(A) \
	template Status blob::stringAssign<A>(A &, Addr, StringView); \
	template Status blob::stringAppend<A>(A &, Addr, StringView); \
	template Status blob::stringResize<A>(A &, Addr, uint32_t, uint8_t); \
	template uint32_t blob::stringSize<A>(const A &, Addr); \
	template void blob::stringRead<A>(const A &, Addr, const Callback<void(StringView)> &); \
	template mem_std::String blob::stringGet<A>(const A &, Addr); \
	template Status blob::bytesAssign<A>(A &, Addr, BytesView); \
	template Status blob::bytesAppend<A>(A &, Addr, BytesView); \
	template Status blob::bytesResize<A>(A &, Addr, uint32_t, uint8_t); \
	template uint32_t blob::bytesSize<A>(const A &, Addr); \
	template void blob::bytesRead<A>(const A &, Addr, const Callback<void(BytesView)> &); \
	template mem_std::Bytes blob::bytesGet<A>(const A &, Addr); \
	template BlobHandle blob::getHandle<A>(const A &, Addr); \
	template uint32_t blob::arrayCount<A>(const A &, Addr, ElementChain); \
	template Status blob::arrayResize<A>(A &, Addr, ElementChain, uint32_t); \
	template Status blob::arrayGet<A>(const A &, Addr, ElementChain, uint32_t, Var &); \
	template Status blob::arraySet<A>(A &, Addr, ElementChain, uint32_t, const Var &); \
	template Status blob::arrayPush<A>(A &, Addr, ElementChain, const Var &); \
	template Status blob::arrayPop<A>(A &, Addr, ElementChain, Var &); \
	template Status blob::arrayErase<A>(A &, Addr, ElementChain, uint32_t); \
	template Addr blob::arrayElementAddr<A>(const A &, Addr, ElementChain, uint32_t); \
	template bool blob::arrayReadRaw<A>(const A &, Addr, ElementChain, \
			const Callback<void(BytesView)> &); \
	template uint32_t blob::mapCount<A>(const A &, Addr); \
	template bool blob::mapFind<A>(const A &, Addr, StringView, uint32_t &); \
	template Status blob::mapGet<A>(const A &, Addr, StringView, Var &); \
	template Status blob::mapSet<A>(A &, Addr, ElementChain, StringView, const Var &); \
	template Status blob::mapErase<A>(A &, Addr, StringView); \
	template void blob::mapKeyAt<A>(const A &, Addr, uint32_t, const Callback<void(StringView)> &); \
	template Status blob::mapValueAt<A>(const A &, Addr, uint32_t, Var &); \
	template Addr blob::mapValueAddr<A>(const A &, Addr, uint32_t); \
	template void blob::destroy<A>(A &, BlobHandle, VarType, ElementChain); \
	template void blob::encode<A>(const A &, Addr, VarType, ElementChain, mem_std::Value &); \
	template Status blob::decode<A>(A &, Addr, VarType, ElementChain, const mem_std::Value &);
SP_FLOW_VALUE_FOR_EACH_ARENA(SP_FLOW_VALUE_INSTANTIATE_BLOB)

// ComponentType's per-instance methods. Member templates of a non-template class: the type holds no
// arena, so the kind belongs to the call.
#define SP_FLOW_VALUE_INSTANTIATE_SCHEMA(A) \
	template Status ComponentType::getField<A>(const A &, Addr, const FieldDesc &, Var &) const; \
	template Status ComponentType::setField<A>(A &, Addr, const FieldDesc &, const Var &) const; \
	template Status ComponentType::getField<A>(const A &, Addr, StringView, Var &) const; \
	template Status ComponentType::setField<A>(A &, Addr, StringView, const Var &) const; \
	template Status ComponentType::setFieldFromValue<A>(A &, Addr, const FieldDesc &, \
			const mem_std::Value &, CastPolicy) const; \
	template Addr ComponentType::createInstance<A>(A &) const; \
	template Status ComponentType::initInstance<A>(A &, Addr, DiagSink *) const; \
	template void ComponentType::destroyInstance<A>(A &, Addr) const; \
	template void ComponentType::freeInstance<A>(A &, Addr) const; \
	template void ComponentType::encodeInstance<A>(const A &, Addr, mem_std::Value &) const; \
	template Status ComponentType::decodeInstance<A>(A &, Addr, const mem_std::Value &, \
			CastPolicy) const;
SP_FLOW_VALUE_FOR_EACH_ARENA(SP_FLOW_VALUE_INSTANTIATE_SCHEMA)

#define SP_FLOW_VALUE_INSTANTIATE_CROSS(D, S) \
	template Status blob::copy<D, S>(D &, Addr, const S &, Addr, VarType, ElementChain); \
	template Status ComponentType::copyInstance<D, S>(D &, Addr, const S &, Addr) const; \
	template Status ComponentType::migrateInstance<D, S>(D &, Addr, const ComponentType &, \
			const S &, Addr, mem_std::Value *) const;
SP_FLOW_VALUE_FOR_EACH_ARENA_PAIR(SP_FLOW_VALUE_INSTANTIATE_CROSS)

} // namespace stappler::flow::value
