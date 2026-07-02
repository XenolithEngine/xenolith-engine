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

# Standalone test suite that checks the freestanding libc implementation
# (runtime/libc_impl) for behavioural identity with the host libc.
#
# The same sources are compiled for the host (Linux/glibc) and for
# x86_64-pc-windows-msvc (the freestanding libc_impl). Every test prints
# deterministic output so the two runs can be diffed for identity.
# See compare.sh for the build+run+diff driver.

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := libctest

LOCAL_PRIVATE_INCLUDE_PCH :=

LOCAL_MODULES_PATHS =

LOCAL_MODULES ?= \
	runtime_libc_wrapper \
	runtime \
	runtime_window
	# ^ runtime_window: TEMPORARY — added to compile-test the +open macOS window
	#   backend (Phase 2 closed-framework ObjC headers). Remove after bring-up.

LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))

LOCAL_SRCS_DIRS := c
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

include $(STAPPLER_BUILD_ROOT)/universal.mk
