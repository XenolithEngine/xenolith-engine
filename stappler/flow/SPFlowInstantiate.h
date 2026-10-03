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

#ifndef STAPPLER_FLOW_SPFLOWINSTANTIATE_H_
#define STAPPLER_FLOW_SPFLOWINSTANTIATE_H_

#include "SPFlowInterp.h"
#include "SPFlowCompiled.h"

// The machine over every store, for one run environment. The template bodies are in the .hpp
// files; a module that brings an environment of its own includes those and this, and expands
// SP_FLOW_FOR_EACH_ARENA over a macro of one argument that calls SP_FLOW_INSTANTIATE_IN with its
// environment, inside `namespace stappler::flow`:
//
//     #define MY_INSTANTIATE(A) SP_FLOW_INSTANTIATE_IN(A, my::SceneEnv)
//     SP_FLOW_FOR_EACH_ARENA(MY_INSTANTIATE)

#if DEBUG
#define SP_FLOW_FOR_EACH_ARENA(M) \
	M(value::PlainArena) \
	M(value::TrackedArena) \
	M(value::ShadowArena)
#else
// ShadowArena is TrackedArena in a release build, so naming it would instantiate the same
// specialization twice.
#define SP_FLOW_FOR_EACH_ARENA(M) \
	M(value::PlainArena) \
	M(value::TrackedArena)
#endif

#define SP_FLOW_INSTANTIATE_IN(A, E) \
	template class LocalStoreT<A, RuntimeGraph, E>; \
	template class ContextT<RuntimeGraph, LocalStoreT<A, RuntimeGraph, E>>; \
	template class MachineT<RuntimeGraph, LocalStoreT<A, RuntimeGraph, E>, TraceLog>; \
	template WatchResolution resolveWatch<RuntimeGraph, LocalStoreT<A, RuntimeGraph, E>>( \
			const Watch &, uint32_t, const RuntimeGraph *, const LocalStoreT<A, RuntimeGraph, E> *, \
			const E::Scene<A> *, WatchTarget &); \
	template class LocalStoreT<A, CompiledGraph, E>; \
	template class ContextT<CompiledGraph, LocalStoreT<A, CompiledGraph, E>>; \
	template class MachineT<CompiledGraph, LocalStoreT<A, CompiledGraph, E>, TraceLog>; \
	template WatchResolution resolveWatch<CompiledGraph, LocalStoreT<A, CompiledGraph, E>>( \
			const Watch &, uint32_t, const CompiledGraph *, const LocalStoreT<A, CompiledGraph, E> *, \
			const E::Scene<A> *, WatchTarget &); \
	template class FastLocalT<A, CompiledGraph, E>; \
	template class ContextT<CompiledGraph, CompiledFastLocalT<A, E>>; \
	template class MachineT<CompiledGraph, CompiledFastLocalT<A, E>, TraceLog>; \
	template class MachineT<CompiledGraph, CompiledFastLocalT<A, E>, TraceNone>; \
	template WatchResolution resolveWatch<CompiledGraph, CompiledFastLocalT<A, E>>(const Watch &, \
			uint32_t, const CompiledGraph *, const CompiledFastLocalT<A, E> *, const E::Scene<A> *, \
			WatchTarget &); \
	template class RunEngineT<A, E>; \
	template class MachineEngineT<InterpreterT<A, E>>; \
	template class MachineEngineT<CompiledRunT<A, E, TraceLog>>; \
	template class MachineEngineT<CompiledFastRunT<A, E, TraceLog>>; \
	template class MachineEngineT<CompiledFastRunT<A, E, TraceNone>>;

#endif /* STAPPLER_FLOW_SPFLOWINSTANTIATE_H_ */
