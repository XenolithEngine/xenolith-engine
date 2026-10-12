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

# The flow GPU executor on WebGPU: the WGSL lowering of the flow kernel, proved block by block
# against the CPU - the conformance of every operation body, the GPU corpus of the interpreter's
# own tests through a real device, and the reducer library's tree folds.
#
# Native is wgpu-native over Vulkan (the runtime/toolchains make file pair brings the library);
# in a browser the same binary runs through runtime/wasm-js (`xwasm.sh flowwgpu`). No window, no
# presentation: the device is a headless compute loop.
#
#     flowwgpu                   every section
#
# Exit code is the number of failed checks (clamped to 254); 0 with a SKIP line when there is no
# WebGPU adapter.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := flowwgpu

LOCAL_PRIVATE_INCLUDE_PCH := SPCommon.h

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk

LOCAL_MODULES ?= \
	runtime \
	stappler_core \
	stappler_filesystem \
	stappler_data \
	stappler_brotli_lib \
	stappler_vstore \
	stappler_flow_value \
	stappler_flow \
	stappler_flow_ops \
	stappler_flow_executors \
	xenolith_backend_webgpu \
	xenolith_flow_webgpu

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

LOCAL_SRCS_DIRS := src
LOCAL_SRCS_OBJS := ../flow/check/flow_check.cpp ../flow/check/flow_names.cpp

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS := $(LOCAL_ROOT) $(LOCAL_ROOT)../flow

LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME := FlowWgpu
APPCONFIG_APP_PATH_COMMON := 0

include $(STAPPLER_BUILD_ROOT)/universal.mk
