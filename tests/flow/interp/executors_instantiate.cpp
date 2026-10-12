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

// The machine instantiated for the executor sections' environment (executors_fixture.h). The kernel's
// own environment, and the environment-free classes, are stappler_flow's; the template bodies are its
// headers, included here beside the instantiation table (SPFlowInstantiate.h).

#include "SPCommon.h"

#include "SPFlowLocal.hpp"
#include "SPFlowFast.hpp"
#include "SPFlowContext.hpp"
#include "SPFlowMachine.hpp"
#include "SPFlowEngine.hpp"
#include "SPFlowInterp.h"
#include "SPFlowCompiled.h"

#include "executors_fixture.h"

#include "SPFlowInstantiate.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

#define TEST_EXECUTORS_INSTANTIATE(A) SP_FLOW_INSTANTIATE_IN(A, test::execfx::TestEnv)

SP_FLOW_FOR_EACH_ARENA(TEST_EXECUTORS_INSTANTIATE)

#undef TEST_EXECUTORS_INSTANTIATE

} // namespace stappler::flow
