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

#ifndef STAPPLER_FLOW_SPFLOWINTERP_H_
#define STAPPLER_FLOW_SPFLOWINTERP_H_

#include "SPFlowEngine.h"
#include "SPFlowMachine.h"

// The interpreter is a name and not a class: `InterpreterT<A>` is `MachineT` (SPFlowMachine.h)
// instantiated for the graph the build produces and the store that keeps a run's records in the
// arena of kind `A`. A generated unit instantiates the same machine over the tables it carries,
// which is what makes the two engines one engine rather than two that agree
//. The run vocabulary (RunOutcome, RunReport, RunConfigT, the
// breakpoints, resolveWatch) is in SPFlowMachine.h and the door an operation is handed (OpContext)
// in SPFlowContext.h; what this header adds is the two names.
namespace STAPPLER_VERSIONIZED stappler::flow {

// The door over the built graph and the arena store, one per arena kind.
template <typename A, typename Env = NoEnv>
using OpContextT = ContextT<RuntimeGraph, LocalStoreT<A, RuntimeGraph, Env>>;

// The interpreter, one per arena kind: the machine over the built graph, the arena store and the
// execution log. XSGraph.scu.cpp instantiates it for PlainArena, TrackedArena and ShadowArena, and
// those three are the whole of what exists - `ArenaRef` is deliberately not among them.
template <typename A, typename Env = NoEnv>
using InterpreterT = MachineT<RuntimeGraph, LocalStoreT<A, RuntimeGraph, Env>, TraceLog>;

// The interpreter behind the executor interface (SPFlowEngine.h): what a host holds when it may
// be handed a unit instead. "interp" is what such a run's state names.
template <typename A, typename Env>
struct RunEngineName<InterpreterT<A, Env>> {
	static constexpr StringView Value = StringView("interp");
};

template <typename A, typename Env = NoEnv>
using InterpreterEngineT = MachineEngineT<InterpreterT<A, Env>>;

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWINTERP_H_ */
