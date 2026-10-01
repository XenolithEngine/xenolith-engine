# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := vstoretest

LOCAL_PRIVATE_INCLUDE_PCH := SPCommon.h

# SP_VSTORE_DIRTY_RANGE=1 builds the suite under the range encoding of the dirty map
LOCAL_CXXFLAGS += $(if $(filter 1,$(SP_VSTORE_DIRTY_RANGE)),-DSP_VSTORE_DIRTY_RANGE=1)

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk

LOCAL_MODULES ?= \
	runtime \
	stappler_core \
	stappler_filesystem \
	stappler_data \
	stappler_brotli_lib \
	stappler_vstore \
	stappler_zstd_lib \
	stappler_lzma_lib

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

LOCAL_SRCS_DIRS := arena journal check
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME := VStoreTest
APPCONFIG_BUNDLE_NAME := org.stappler.VStoreTest
APPCONFIG_APP_PATH_COMMON := 3

include $(STAPPLER_BUILD_ROOT)/universal.mk
