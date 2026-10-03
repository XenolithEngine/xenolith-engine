# Copyright (c) 2026 Stappler Team <admin@stappler.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# The graph kernel: operations and their registry, the graph document, the build, the machine over
# both stores, the interpreter and the loaded unit. A host brings its scene as a run environment
# (SPFlowEnv.h) and its words for the numeric diagnostics. The sources of the submodules below this
# directory are theirs, hence the compile unit named rather than the directory searched.

MODULE_STAPPLER_FLOW_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FLOW_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_FLOW_SRCS_DIRS :=
MODULE_STAPPLER_FLOW_SRCS_OBJS := $(STAPPLER_MODULE_DIR)/flow/SPFlow.scu.cpp
MODULE_STAPPLER_FLOW_INCLUDES_DIRS :=
MODULE_STAPPLER_FLOW_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/flow
MODULE_STAPPLER_FLOW_DEPENDS_ON := stappler_core stappler_data stappler_flow_value

MODULE_STAPPLER_FLOW_SHARED_SPEC_SUMMARY := libstappler graph kernel: operations, graph build, interpreter and compiled runs

define MODULE_STAPPLER_FLOW_SHARED_SPEC_DESCRIPTION
Module libstappler-flow implements an execution graph over typed values
- operations and their registry; the graph document and its build
- a resumable machine over the arena and a fast one over host memory
- the interpreter and the run of a generated unit
- run environments a host plugs its own scene into
- GPU lowering of parallel blocks
endef

$(call define_module, stappler_flow, MODULE_STAPPLER_FLOW)
