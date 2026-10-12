# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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

# The WebGPU device of the flow GPU executor: `flow::GpuDevice` over the webgpu backend, WGSL
# shaders and no window. It links the wgpu-native library the backend brings, and on wasm the
# browser imports come with the backend as well.

MODULE_XENOLITH_FLOW_WEBGPU_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_FLOW_WEBGPU_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_FLOW_WEBGPU_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_FLOW_WEBGPU_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/flow/webgpu
MODULE_XENOLITH_FLOW_WEBGPU_SRCS_OBJS :=
MODULE_XENOLITH_FLOW_WEBGPU_INCLUDES_DIRS :=
MODULE_XENOLITH_FLOW_WEBGPU_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/flow/webgpu
MODULE_XENOLITH_FLOW_WEBGPU_DEPENDS_ON := xenolith_backend_webgpu stappler_flow_executors

#spec

MODULE_XENOLITH_FLOW_WEBGPU_SHARED_SPEC_SUMMARY := Flow GPU executor on WebGPU

define MODULE_XENOLITH_FLOW_WEBGPU_SHARED_SPEC_DESCRIPTION
Module libxenolith-flow-webgpu implements the flow GPU executor's device interface
(flow::GpuDevice) over the WebGPU backend, with the WGSL shaders of the flow lowering.
endef

# module name resolution
$(call define_module, xenolith_flow_webgpu, MODULE_XENOLITH_FLOW_WEBGPU)
