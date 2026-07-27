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

# SPRT_SHARED=1 builds the runtime as a single shared object that owns the whole
# C/C++ runtime state (heap, stdio, TLS, atexit, exception tables). Consumers then
# link the generated import library and must be compiled with -DSPRT_SHARED_RUNTIME
# so SPRT_API/SPRT_GLOBAL resolve to the import side of the ABI.
SPRT_SHARED ?= 0

# Separate output tree for the shared build. On Windows both modes would otherwise write
# `sprt.lib`: the static archive in one case, sprt.dll's import library in the other -
# same path, silently incompatible contents. Keep them apart so switching SPRT_SHARED
# does not leave a consumer linking the wrong one.
ifeq ($(SPRT_SHARED),1)
LOCAL_OUTDIR ?= $(dir $(LOCAL_MAKEFILE))stappler-build-shared
else
LOCAL_OUTDIR ?= $(dir $(LOCAL_MAKEFILE))stappler-build
endif

ifeq ($(SPRT_SHARED),1)
LOCAL_BUILD_STATIC := 0
LOCAL_BUILD_SHARED := 1
else ifneq (,$(findstring apple,$(STAPPLER_TARGET)))
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
	runtime_window \
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

ifeq ($(SPRT_SHARED),1)
ifneq (,$(findstring windows,$(STAPPLER_TARGET)))
# SPRT_API/__declspec(dllexport) covers everything a consumer names in source. It cannot
# cover the symbols the *compiler* emits references to on its own, with no declaration
# in scope: static-destructor registration and the MSVC C++ ABI entry points. Those have
# to be named explicitly here.
#
# All of these are functions, so an unannotated reference in the consumer resolves
# through the import library's thunk; no dllimport is needed on the other side. Data
# symbols could not be handled this way - see libc_impl/app/windows/app_startup.cpp for
# the ones that must instead be defined per image.
# __tlregdtor is how clang registers a thread_local destructor. The list it feeds is
# thread-local state inside the runtime, so unlike the rest of the TLS apparatus (which
# is per-image - see libc_impl/app/windows/app_startup.cpp) it must be the runtime's.
SPRT_ABI_EXPORTS := \
	atexit \
	__tlregdtor \
	_CxxThrowException \
	__CxxFrameHandler3 \
	__std_terminate

LOCAL_LDFLAGS := $(addprefix -Xlinker /EXPORT:,$(SPRT_ABI_EXPORTS))
endif
endif

ifneq (,$(findstring apple,$(STAPPLER_TARGET)))
LOCAL_LDFLAGS := \
	-install_name @rpath/libsprt.dylib \
	-reexport_framework CoreFoundation \
	-reexport_framework Foundation \
	-reexport_framework SystemConfiguration \
	-reexport_framework Security \
	-reexport_framework UniformTypeIdentifiers \
	-reexport_framework Network \
	-reexport_framework IOKit \
	-reexport_framework QuartzCore \
	-reexport_framework Metal \
	-reexport-lSystem -reexport-licucore -reexport-lobjc -reexport-liconv -reexport-lc++abi
# AppKit is macOS-only; iOS uses UIKit instead.
ifneq (,$(findstring apple-ios,$(STAPPLER_TARGET)))
LOCAL_LDFLAGS += -reexport_framework UIKit
else
LOCAL_LDFLAGS += -reexport_framework AppKit
endif
endif

include $(STAPPLER_BUILD_ROOT)/universal.mk
