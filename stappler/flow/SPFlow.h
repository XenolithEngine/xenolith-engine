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

#ifndef STAPPLER_FLOW_SPFLOW_H_
#define STAPPLER_FLOW_SPFLOW_H_

#include "SPFlowDiag.h"
#include "SPFlowValueSchema.h"
#include "SPFlowValueHost.h"

// The graph of operations, and the interpreter that walks it. A graph is code, not state - no byte of it lives in an arena, so a rollback restores what the systems did
// to the scene without touching what the author did to the graph, and a hot swap replaces the whole
// runtime form at once. ComponentType descriptors are host-side with stable addresses for the same
// reason: an OpDesc holds one directly.
namespace STAPPLER_VERSIONIZED stappler::flow {

// Names used on nearly every line below; everything else keeps its `vstore::` spelling, so it stays
// obvious which layer a type belongs to.
using value::Addr;
using value::ElementChain;
using value::NullAddr;
using value::TypeId;
using value::Var;
using value::VarType;
using value::makeTypeId;

// An operation is identified by the hash of its name, exactly like a component type: the asset
// stores the name (readable in a diff, stable between runs and platforms) and the build resolves
// it. sprt::hash64 and nothing else - see SPFlowValueVar.h on why every other hash in reach picks its
// width from the pointer size.
using OpId = TypeId;

// Limits the interpreter depends on, checked here: MaxDataPins is 32 so the interpreter can hold
// "which inputs of this node have a value" in one machine word rather than a structure in the
// arena. Validation refuses a wider signature before any graph can use one.
static constexpr uint32_t MaxNodesPerGraph = 65'535;
static constexpr uint32_t MaxDataPins = 32; // per direction
static constexpr uint32_t MaxExecOut = 32; // an exec input is always 0 or 1
static constexpr uint32_t MaxEdgesPerGraph = 1u << 18;

// No pin. A part of a signature that a given group does not have says so with this rather than with
// a count, because a count cannot tell "absent" from "the pin at index zero".
static constexpr uint32_t NullPin = 0xffff'ffffu;

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOW_H_ */
