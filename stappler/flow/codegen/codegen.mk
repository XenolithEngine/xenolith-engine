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

# The generator: a built graph written out as a C++ unit that carries it as constants and performs
# each node through a function of its own.

MODULE_STAPPLER_FLOW_CODEGEN_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FLOW_CODEGEN_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_FLOW_CODEGEN_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/flow/codegen
MODULE_STAPPLER_FLOW_CODEGEN_SRCS_OBJS :=
MODULE_STAPPLER_FLOW_CODEGEN_INCLUDES_DIRS :=
MODULE_STAPPLER_FLOW_CODEGEN_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/flow/codegen
MODULE_STAPPLER_FLOW_CODEGEN_DEPENDS_ON := stappler_flow stappler_filesystem

MODULE_STAPPLER_FLOW_CODEGEN_SHARED_SPEC_SUMMARY := libstappler generator of C++ units from stappler_flow graphs

define MODULE_STAPPLER_FLOW_CODEGEN_SHARED_SPEC_DESCRIPTION
Module libstappler-flow-codegen writes a built graph out as a C++ unit
- the graph as constant tables with an identity checked on load
- one step function per node, calling operations by name
- GPU shaders of parallel blocks
- the generator tool's driver (runTool), around which a program is a dozen lines
endef

$(call define_module, stappler_flow_codegen, MODULE_STAPPLER_FLOW_CODEGEN)
