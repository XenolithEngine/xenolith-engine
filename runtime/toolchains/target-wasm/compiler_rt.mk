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

# Cross-build the LLVM runtimes for wasm32: compiler-rt (builtins), libunwind and
# libc++abi (NOT libc++ — the runtime ships its own STL). Building them together
# (the runtimes superbuild, like target-linux) rather than compiler-rt alone is
# deliberate — a standalone builtins build produces a truncated compiler-rt that
# lacks the unwinder-dependent pieces. Everything compiles against the sprt
# headers via the generated toolchain.cmake.

.DEFAULT_GOAL := all

LIBNAME := llvm-project
BUILD_DIR := runtimes-build-wasm

include ../common/utils/detect-platform.mk
include ../common/utils/init-shell.mk

# libc++abi builds against the sprt STL: sprt owns the public std:: types
# (type_info as a real std class, exception, bad_cast/bad_typeid/bad_alloc), and
# the sprt STL carries hand-written replacements for the libc++ build-internal
# headers libc++abi pulls (<__config>, <__config_site>, <__assert>,
# <__assertion_handler>, <stdexcept>, <version>, <__memory/aligned_alloc.h>,
# <__thread/support.h>). The C++ runtimes are compiled hosted so std:: exposes the
# libc. Set WASM_ENABLE_LIBCXXABI=0 to build only compiler-rt + libunwind.
WASM_ENABLE_LIBCXXABI ?= 1
ifeq ($(WASM_ENABLE_LIBCXXABI),1)
WASM_RUNTIMES := compiler-rt;libunwind;libcxxabi
else
WASM_RUNTIMES := compiler-rt;libunwind
endif

CONFIGURE := \
	-G "Ninja" \
	-S $(LIB_SRC_DIR)/$(LIBNAME)/runtimes \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE)) \
	-DCMAKE_PROJECT_INCLUDE=$(abspath $(MAKE_ROOT))/wasm-project-include.cmake \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_INSTALL_LIBDIR=$(SP_INSTALL_PREFIX)/usr/lib \
	-DCMAKE_INSTALL_INCLUDEDIR=$(SP_INSTALL_PREFIX)/usr/include \
	-DLLVM_ENABLE_RUNTIMES="$(WASM_RUNTIMES)" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLVM_ENABLE_PIC=Off \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_TARGET) \
	-DLLVM_HOST_TRIPLE=$(SP_TARGET) \
	-DCOMPILER_RT_DEFAULT_TARGET_ONLY=On \
	-DCOMPILER_RT_BAREMETAL_BUILD=On \
	-DCOMPILER_RT_BUILD_BUILTINS=On \
	-DCOMPILER_RT_BUILD_SANITIZERS=Off \
	-DCOMPILER_RT_BUILD_XRAY=Off \
	-DCOMPILER_RT_BUILD_MEMPROF=Off \
	-DCOMPILER_RT_BUILD_CTX_PROFILE=Off \
	-DCOMPILER_RT_BUILD_PROFILE=Off \
	-DCOMPILER_RT_BUILD_LIBFUZZER=Off \
	-DCOMPILER_RT_BUILD_ORC=Off \
	-DCOMPILER_RT_BUILD_GWP_ASAN=Off \
	-DCOMPILER_RT_OS_DIR=wasi \
	-DLIBUNWIND_ENABLE_SHARED=Off \
	-DLIBUNWIND_ENABLE_THREADS=Off \
	-DLIBUNWIND_IS_BAREMETAL=On \
	-DLIBUNWIND_USE_COMPILER_RT=On \
	-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_ENABLE_SHARED=Off \
	-DLIBCXXABI_ENABLE_THREADS=Off \
	-DLIBCXXABI_ENABLE_EXCEPTIONS=On \
	-DLIBCXXABI_USE_LLVM_UNWINDER=On \
	-DLIBCXXABI_USE_COMPILER_RT=On \
	-DLIBCXXABI_BAREMETAL=On \
	-DLIBCXXABI_ENABLE_STATIC=On \
	-DLIBCXXABI_ENABLE_NEW_DELETE_DEFINITIONS=Off \
	-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_LIBCXX_INCLUDES="$(SP_RUNTIME_ROOT)/include_libc/cxx;$(SP_RUNTIME_ROOT)/libcxx/include"

all:
	$(call rule_rm,$(BUILD_DIR))
	$(call rule_mkdir,$(BUILD_DIR))
	cd $(BUILD_DIR); cmake $(CONFIGURE)
	cd $(BUILD_DIR); cmake --build . --config Release --target install
	$(call rule_rm,$(BUILD_DIR))
	# Move the builtins into the resource dir the app target.mk points -resource-dir at.
	$(call rule_mkdir,$(SP_INSTALL_PREFIX)/lib/clang/lib)
	$(call rule_rm,$(SP_INSTALL_PREFIX)/lib/clang/lib/wasi)
	$(call rule_mv,$(SP_INSTALL_PREFIX)/lib/wasi,$(SP_INSTALL_PREFIX)/lib/clang/lib)

.PHONY: all
