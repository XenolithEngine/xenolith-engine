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

# LLVM for the Embox target: the static libraries Mesa's llvmpipe JITs with
# (xenolith-os docs/EMBOX-LAVAPIPE.md, B2). Two stages, as the Linux device
# build in xenolith-os toolchain/llvm.mk:
#
#   native   llvm-tblgen, llvm-min-tblgen and llvm-config for the build host,
#            with the system compiler (the host package's clang cannot link a
#            native program)
#   cross    every LLVM library for AArch64, compiled against sprt like the
#            other deps (toolchain-libs.cmake, common/configure.mk), with the
#            native tablegen
#
# Both stages get the same component-shaping options and the same host triple,
# aarch64-unknown-none-elf: the native llvm-config is copied into usr/bin and is
# what Mesa's meson asks for the libraries, so its idea of `native` has to be
# AArch64 and its component list has to be the cross build's.
#
# The two build trees stay (intermediate/.../llvm-native, llvm-build): LLVM is a
# few thousand objects, and a rebuild after a patch to lib/Support is a few.

.DEFAULT_GOAL := all

LIBNAME = llvm

include ../common/configure.mk

LLVM_SRC ?= $(LIB_SRC_DIR)/llvm-project
LLVM_NATIVE := $(SP_INSTALL_PREFIX)/llvm-native
LLVM_BUILD := $(SP_INSTALL_PREFIX)/llvm-build
LLVM_TRIPLE := aarch64-unknown-none-elf

# What both stages agree on: the components (AArch64 only), the ABI knobs Mesa
# checks against (RTTI off, -Dcpp_rtti=false there), and nothing optional.
LLVM_COMMON := \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_TARGETS_TO_BUILD=AArch64 \
	-DLLVM_HOST_TRIPLE=$(LLVM_TRIPLE) \
	-DLLVM_DEFAULT_TARGET_TRIPLE=$(LLVM_TRIPLE) \
	-DLLVM_ENABLE_PROJECTS= \
	-DLLVM_ENABLE_RUNTIMES= \
	-DLLVM_ENABLE_RTTI=OFF \
	-DLLVM_ENABLE_EH=OFF \
	-DLLVM_ENABLE_ASSERTIONS=OFF \
	-DLLVM_ENABLE_BINDINGS=OFF \
	-DLLVM_ENABLE_ZLIB=OFF \
	-DLLVM_ENABLE_ZSTD=OFF \
	-DLLVM_ENABLE_LIBXML2=OFF \
	-DLLVM_ENABLE_TERMINFO=OFF \
	-DLLVM_ENABLE_LIBEDIT=OFF \
	-DLLVM_ENABLE_LIBPFM=OFF \
	-DLLVM_ENABLE_CURL=OFF \
	-DLLVM_ENABLE_HTTPLIB=OFF \
	-DLLVM_ENABLE_TELEMETRY=OFF \
	-DLLVM_INCLUDE_TESTS=OFF \
	-DLLVM_INCLUDE_EXAMPLES=OFF \
	-DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_INCLUDE_DOCS=OFF \
	-DLLVM_BUILD_LLVM_DYLIB=OFF \
	-DLLVM_LINK_LLVM_DYLIB=OFF \
	-DLLVM_APPEND_VC_REV=OFF

# Answers config-ix would otherwise get from probes that link, or get wrong: the
# sprt/Embox libc declares some of these and implements them as stubs (dlopen
# answers NULL), and LLVM should not build on them.
LLVM_EMBOX_PROBES := \
	-DHAVE_LIBDL=OFF \
	-DHAVE_DLOPEN=OFF \
	-DHAVE_LIBRT=OFF \
	-DHAVE_BACKTRACE=OFF \
	-DBacktrace_FOUND=OFF \
	-DHAVE_POSIX_SPAWN=OFF \
	-DHAVE_SIGALTSTACK=OFF \
	-DHAVE_GETAUXVAL=OFF \
	-DHAVE_MALLINFO=OFF \
	-DHAVE_MALLINFO2=OFF \
	-DHAVE_MALLCTL=OFF \
	-DHAVE_SBRK=OFF \
	-DHAVE_SYSEXITS_H=OFF \
	-DHAVE_PTHREAD_SETNAME_NP=OFF \
	-DHAVE_PTHREAD_GETNAME_NP=OFF \
	-DHAVE_PTHREAD_SET_NAME_NP=OFF \
	-DHAVE_PTHREAD_GET_NAME_NP=OFF \
	-DHAVE_CXX_ATOMICS_WITHOUT_LIB=ON \
	-DHAVE_CXX_ATOMICS64_WITHOUT_LIB=ON

LLVM_CROSS := \
	$(CONFIGURE_CMAKE) \
	$(LLVM_COMMON) \
	$(LLVM_EMBOX_PROBES) \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX)/usr \
	-DCMAKE_PROJECT_LLVM_INCLUDE=$(MAKE_ROOT)embox-llvm-project-include.cmake \
	-DLLVM_TARGET_ARCH=AArch64 \
	-DLLVM_NATIVE_TOOL_DIR=$(LLVM_NATIVE)/bin \
	-DLLVM_TABLEGEN=$(LLVM_NATIVE)/bin/llvm-tblgen \
	-DLLVM_INCLUDE_TOOLS=OFF \
	-DLLVM_BUILD_TOOLS=OFF \
	-DLLVM_INCLUDE_UTILS=OFF \
	-DLLVM_BUILD_UTILS=OFF \
	-DLLVM_ENABLE_PIC=OFF \
	-DLLVM_ENABLE_THREADS=ON \
	-DLLVM_ENABLE_BACKTRACES=OFF \
	-DLLVM_ENABLE_CRASH_OVERRIDES=OFF \
	-DLLVM_ENABLE_PLUGINS=OFF \
	-DLLVM_ENABLE_UNWIND_TABLES=OFF

$(LLVM_NATIVE)/bin/llvm-config:
	cmake -G Ninja -S $(LLVM_SRC)/llvm -B $(LLVM_NATIVE) \
		-DCMAKE_C_COMPILER=/usr/bin/cc -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
		$(LLVM_COMMON)
	cmake --build $(LLVM_NATIVE) --target llvm-min-tblgen llvm-tblgen llvm-config

# The libraries, their headers and CMake package, and nothing else: `all` would
# also link programs (llvm-tblgen for the target among them), and nothing here
# links a program against the libc++ port's out-of-line half.
LLVM_INSTALL_TARGETS := install-llvm-libraries install-llvm-headers install-cmake-exports

all: $(LLVM_NATIVE)/bin/llvm-config
	cmake -G Ninja -S $(LLVM_SRC)/llvm -B $(LLVM_BUILD) $(LLVM_CROSS)
	cmake --build $(LLVM_BUILD) --parallel --target llvm-libraries
	cmake --build $(LLVM_BUILD) --target $(LLVM_INSTALL_TARGETS)
	mkdir -p $(SP_INSTALL_PREFIX)/usr/bin
	cp -f $(LLVM_NATIVE)/bin/llvm-config $(SP_INSTALL_PREFIX)/usr/bin/llvm-config
	@libdir=$$($(SP_INSTALL_PREFIX)/usr/bin/llvm-config --libdir); \
	test "$$(realpath $$libdir)" = "$$(realpath $(SP_INSTALL_PREFIX)/usr/lib)" \
		|| { echo "llvm-config --libdir is $$libdir, not $(SP_INSTALL_PREFIX)/usr/lib" >&2; exit 1; }; \
	test -f $$libdir/libLLVMCore.a || { echo "no libLLVMCore.a in $$libdir" >&2; exit 1; }; \
	$(SP_INSTALL_PREFIX)/usr/bin/llvm-config --components | tr ' ' '\n' | grep -qx aarch64 \
		|| { echo "llvm-config has no aarch64 component" >&2; exit 1; }

.PHONY: all
