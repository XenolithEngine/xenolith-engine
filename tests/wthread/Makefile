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

# Wasm thread / allocator stress: concurrent malloc/free traffic that forces
# repeated linear-memory growth (the single memory.grow lock shared between
# libc sbrk and mimalloc's wasi primitive layer is what this exercises), plus
# a direct multi-thread sbrk() overlap check. Run headless via
# runtime/wasm-js/run-node.mjs or in the browser via runtime/wasm-js/xwasm.sh thread.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := wthreadtest
LOCAL_MODULES_PATHS =
LOCAL_MODULES ?= \
	runtime_libc_wrapper \
	runtime
LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))
LOCAL_MAIN := main.cpp
include $(STAPPLER_BUILD_ROOT)/universal.mk
