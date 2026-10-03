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

# The standard library of graph operations. Kept out of the kernel so that the layer which defines
# what an operation is cannot come to depend on any particular one; the scene family reaches the
# scene through the run environment, so the whole library runs without one.

MODULE_STAPPLER_FLOW_OPS_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FLOW_OPS_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_FLOW_OPS_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/flow/ops
MODULE_STAPPLER_FLOW_OPS_SRCS_OBJS :=
MODULE_STAPPLER_FLOW_OPS_INCLUDES_DIRS :=
MODULE_STAPPLER_FLOW_OPS_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/flow/ops
MODULE_STAPPLER_FLOW_OPS_DEPENDS_ON := stappler_flow

MODULE_STAPPLER_FLOW_OPS_SHARED_SPEC_SUMMARY := libstappler standard library of graph operations

define MODULE_STAPPLER_FLOW_OPS_SHARED_SPEC_DESCRIPTION
Module libstappler-flow-ops implements the standard operations of stappler_flow graphs
- flow control, math, numeric vectors, strings, time
- parallel blocks and their collectors
- scene access through the run environment
- shader forms for GPU lowering
endef

$(call define_module, stappler_flow_ops, MODULE_STAPPLER_FLOW_OPS)
