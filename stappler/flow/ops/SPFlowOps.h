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

#ifndef STAPPLER_FLOW_OPS_SPFLOWOPS_H_
#define STAPPLER_FLOW_OPS_SPFLOWOPS_H_

#include "SPFlowInterp.h"

// The standard library of nodes, a separate module on purpose: the graph layer knows what an
// operation is and nothing about any particular one. Nothing here produces an effect - the
// deferred-effect buffer is the run's, and a node that could call out of the process without it
// would make "rewind without side effects" a lie. The `scene.*` family is a different matter: it
// reads and writes components through the door OpContext opens, because a graph that cannot address
// state outside its own run can only compute.
namespace STAPPLER_VERSIONIZED stappler::flow::ops {

// Everything below. Idempotent: an operation that is already registered is left alone, so a caller
// who does not know whether someone else did this may just call it.
SP_PUBLIC Status registerCoreOps(flow::OpRegistry &);

// The families, for a host that wants only some of them.
SP_PUBLIC Status registerFlowOps(flow::OpRegistry &); // event, sequence, branch
SP_PUBLIC Status registerMathOps(flow::OpRegistry &); // math, logic, compare, convert, value
SP_PUBLIC Status registerStringOps(flow::OpRegistry &); // string, debug
SP_PUBLIC Status registerTimeOps(flow::OpRegistry &); // wait.timer, await.component
SP_PUBLIC Status registerSceneOps(flow::OpRegistry &); // scene.global, has, get/set, find, add/remove
SP_PUBLIC Status registerNumericOps(flow::OpRegistry &); // every scalar type, vectors, conversions, enums
SP_PUBLIC Status registerParallelOps(flow::OpRegistry &); // the parallel block and its collectors

// The scene components the library needs - `wait.Timer` and nothing else so far. Registered against
// the scene's registry, not the operation registry's derived types: a timer outlives the run that
// armed it, which is the whole reason waiting is expressed this way. Idempotent.
SP_PUBLIC Status registerCoreComponents(value::TypeRegistry &);

} // namespace stappler::flow::ops

#endif /* STAPPLER_FLOW_OPS_SPFLOWOPS_H_ */
