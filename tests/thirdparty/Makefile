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

# Functional test suite for the third-party libraries shipped in the toolchain
# sysroot. The libraries are used DIRECTLY (their own public headers and ABI) -
# no stappler module is linked in, so a failure here points at the shipped
# library / its build options, not at engine glue code.
#
# Only `runtime` is linked, because that is what the libraries themselves were
# built against (the freestanding libc on Windows, glibc on Linux).
# See run.sh for the build+run driver.

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := thirdpartytest

LOCAL_PRIVATE_INCLUDE_PCH :=

LOCAL_MODULES_PATHS = tests/thirdparty/thirdparty-modules.mk

LOCAL_MODULES ?= \
	runtime_libc_wrapper \
	runtime \
	thirdparty_curl \
	thirdparty_openssl

LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))

LOCAL_SRCS_DIRS := lib
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

include $(STAPPLER_BUILD_ROOT)/universal.mk
