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

# Native contracts for the Tau WSI copy and acquisition paths, using the Mesa build's headers.
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_ROOT := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(LOCAL_ROOT)stappler-build
LOCAL_EXECUTABLE := tauwsitest
LOCAL_MODULES_PATHS :=
LOCAL_MODULES := runtime_libc_wrapper runtime
LOCAL_SRCS_OBJS := main.c
TAU_WSI_MESA := $(abspath $(LOCAL_ROOT)../../runtime/toolchains/src/mesa)
LOCAL_PRIVATE_DEPS := $(TAU_WSI_MESA)/src/vulkan/wsi/wsi_common_tau.c
TAU_WSI_BUILD ?= $(abspath $(LOCAL_ROOT)../../runtime/toolchains/target-tau/intermediate/x86_64-tau-vulkan/mesa-build)
LOCAL_INCLUDES_OBJS := $(TAU_WSI_MESA)/include $(TAU_WSI_MESA)/src $(TAU_WSI_MESA)/src/vulkan/wsi \
	$(TAU_WSI_MESA)/src/vulkan/runtime $(TAU_WSI_MESA)/src/vulkan/util \
	$(TAU_WSI_BUILD)/include $(TAU_WSI_BUILD)/src $(TAU_WSI_BUILD)/src/vulkan/wsi \
	$(TAU_WSI_BUILD)/src/vulkan/util $(TAU_WSI_BUILD)/src/vulkan/runtime \
	$(LOCAL_ROOT)../../runtime/toolchains/src/vulkan-headers/include
LOCAL_CFLAGS += -DVK_USE_PLATFORM_DISPLAY_KHR -DHAVE_PTHREAD -DHAVE_THRD_CREATE -DHAVE_TIMESPEC_GET \
	-DHAVE_STRUCT_TIMESPEC -DHAVE_ENDIAN_H \
	-ffunction-sections -fdata-sections
LOCAL_LDFLAGS += -Wl,--gc-sections
include $(STAPPLER_BUILD_ROOT)/universal.mk
