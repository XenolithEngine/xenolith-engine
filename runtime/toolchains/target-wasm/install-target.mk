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

# Copy the built wasm sysroot (runtimes + the compiler-rt resource dir + the
# app-facing target.mk) from intermediate/ into the installed targets/ tree.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_ROOT := $(dir $(realpath $(THIS_FILE)))
GIT_TAG ?= $(shell git describe --tags --abbrev=0 2>/dev/null)

T_INTERMEDIATE ?= $(abspath $(MAKE_ROOT))/intermediate/wasm32-unknown-unknown
T_TARGET ?= $(abspath $(MAKE_ROOT))/targets/wasm32-unknown-unknown

# NB: do NOT pre-create lib/clang/lib/wasi here — it is itself a rule target
# below, and if it already exists make treats that rule as up-to-date and skips
# the builtins copy.
$(T_TARGET):
	mkdir -p $(T_TARGET)/usr/lib $(T_TARGET)/share

# Static runtimes: libclang_rt.builtins-wasm32.a, libunwind.a, libc++abi.a, libc++.a
# libsprt.a ships too: it is the runtime (libc AND the libc++ port in one archive) that
# a freestanding wasm host binary links against - the wasm clang host is built exactly
# this way. Apps that drive the engine build system still rebuild the runtime from
# source; this archive is for consumers that only have a compiler and a sysroot.
$(T_TARGET)/usr/lib: $(T_INTERMEDIATE)/usr/lib | $(T_TARGET)
	@mkdir -p $@
	cp -af $(T_INTERMEDIATE)/usr/lib/*.a $@/ 2>/dev/null || true

# Installed libc++/libc++abi/libunwind headers.
# The intermediate usr/include already carries simde (installed by init-target's
# cmake build) alongside the libc++abi headers, so this copies both.
$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

# compiler-rt builtins in the resource dir the app target.mk references.
$(T_TARGET)/lib/clang/lib/wasi: $(T_INTERMEDIATE)/lib/clang/lib/wasi | $(T_TARGET)
	@mkdir -p $@
	cp -af $(T_INTERMEDIATE)/lib/clang/lib/wasi/*.a $@/ 2>/dev/null || true
	rm -f $(T_TARGET)/lib/clang/include
	cd $(T_TARGET)/lib/clang; ln -fs ../../../../hosts/$(HOST_ID)/lib/clang/$(SP_LLVM_VER)/include include 2>/dev/null || true

$(T_TARGET)/target.mk: $(T_INTERMEDIATE)/target.mk | $(T_TARGET)
	cp -af $< $@

$(T_TARGET)/share/licenses: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf ../licenses $(T_TARGET)/share 2>/dev/null || true

$(T_TARGET)/release: | $(T_TARGET)
	echo "$(GIT_TAG)" > $@

include ../common/utils/detect-platform.mk
include ../common/utils/llvm-version.mk

all: $(T_TARGET)/usr/lib $(T_TARGET)/usr/include \
	$(T_TARGET)/lib/clang/lib/wasi \
	$(T_TARGET)/target.mk $(T_TARGET)/share/licenses $(T_TARGET)/release

.PHONY: all
