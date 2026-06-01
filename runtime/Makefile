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

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

ifneq (,$(findstring apple,$(STAPPLER_TARGET)))
LOCAL_BUILD_STATIC := 0
LOCAL_BUILD_SHARED := 1
else
LOCAL_BUILD_STATIC := 1
LOCAL_BUILD_SHARED := 0
endif

LOCAL_LIBRARY := sprt

LOCAL_PRIVATE_INCLUDE_PCH :=

LOCAL_MODULES_PATHS =

LOCAL_MODULES ?= \
	runtime_libc_wrapper \
	runtime

LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))

LOCAL_SRCS_DIRS :=
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.c

LOCAL_OPTIMIZATION := -O3

# Used by runtime.mk
SPRT_BUILD_RUNTIME := 1

ifneq (,$(findstring apple,$(STAPPLER_TARGET)))
LOCAL_LDFLAGS := \
	-install_name @rpath/libsprt.dylib \
	-reexport_framework CoreFoundation \
	-reexport_framework Foundation \
	-reexport_framework SystemConfiguration \
	-reexport_framework Security \
	-reexport_framework UniformTypeIdentifiers \
	-reexport_framework AppKit \
	-reexport_framework Network \
	-reexport_framework IOKit \
	-reexport_framework QuartzCore \
	-reexport_framework Metal \
	-reexport-lSystem -reexport-licucore -reexport-lobjc -reexport-liconv -reexport-lc++abi
endif

include $(STAPPLER_BUILD_ROOT)/universal.mk
