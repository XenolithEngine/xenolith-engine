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

// Compile unit for stappler_flow: the .cc files below are include-only subunits and are never
// compiled on their own. Order matters - append, do not reorganize.

#include "SPCommon.h"

#include "SPFlow.cc"
#include "SPFlowOp.cc"
#include "SPFlowAsset.cc"
#include "SPFlowRuntime.cc"
#include "SPFlowScope.cc"
#include "SPFlowFunction.cc"
#include "SPFlowGpu.h"
#include "SPFlowParallel.cc"
#include "SPFlowFast.hpp"
#include "SPFlowMachine.hpp"
#include "SPFlowEngine.hpp"
#include "SPFlowInterp.cc"
#include "SPFlowCompiled.cc"
#include "SPFlowGpu.cc"
#include "SPFlowInstantiate.h"

// The run is a template over the arena kind, the graph's representation and the local store
// (SPFlowStatic.h), defined in the .hpp bodies above and therefore invisible to ops/, ext/ and the
// tests: they see the declarations, emit external references, and link against the instantiations
// below. The machine over the built graph and the arena store is InterpreterT<A>
// (SPFlowInterp.h); over a loaded unit and the arena store, CompiledRunT<A> (SPFlowCompiled.h) -
// one per arena kind rather than one per unit, since a unit is rows and the machine reads rows
// through CompiledGraph whichever unit they came from; over the fast store, CompiledFastRunT<A>
// (SPFlowFast.h) under both trace policies, because a shipped game runs a unit with no execution
// log. All of them stand behind the executor interface as InterpreterEngineT<A>,
// CompiledEngineT<A>, CompiledFastEngineT<A> and CompiledQuietEngineT<A>. No attribute goes on
// these lines - an attribute list cannot appear in explicit-instantiation position; SP_PUBLIC on
// the class templates and on the tracker tags carries the visibility.

namespace STAPPLER_VERSIONIZED stappler::flow {

// The kernel's own environment: what a unit generated for a host without a scene runs in, and what
// proves that nothing above reaches a scene it was not given. A host with an environment of its own
// instantiates the same lines for it in its own module, with SP_FLOW_INSTANTIATE_IN.
#define SP_FLOW_INSTANTIATE(A) \
	template class RunLocalViewT<A>; \
	SP_FLOW_INSTANTIATE_IN(A, NoEnv)

SP_FLOW_FOR_EACH_ARENA(SP_FLOW_INSTANTIATE)

} // namespace stappler::flow

#undef SP_FLOW_INSTANTIATE
