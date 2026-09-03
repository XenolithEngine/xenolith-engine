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
# Cross-build the LLVM runtimes for Embox EL0: compiler-rt (builtins), libunwind
# and libc++abi. NOT libc++ -- the STL on this target is the engine's own port
# (runtime/libcxx), and libc++abi is built against ITS headers so that
# std::type_info and the exception types are one set across the whole image.
#
# Built as the runtimes superbuild rather than compiler-rt alone: a standalone
# builtins build produces a truncated compiler-rt missing the pieces that depend
# on the unwinder. Same reasoning as target-wasm.
#
# LIBUNWIND_IS_BAREMETAL is ON here and that is a temporary answer. The
# non-baremetal ELF path finds .eh_frame through dl_iterate_phdr and
# PT_GNU_EH_FRAME -- which our link does emit -- but it also needs a
# dl_iterate_phdr and a <link.h>, and the libc has neither yet. So for now the
# unwinder scans between __eh_frame_start/__eh_frame_end, which app-aarch64.lds
# provides. Contour L4 supplies dl_iterate_phdr for a static image and flips this
# to Off; nothing else in this file changes.
#
# COMPILER_RT_INSTALL_PATH is set EXPLICITLY rather than left to LLVM's
# derivation. Without it the builtins land in lib/<os_dir>/ here while the same
# configuration puts them in lib/clang/lib/<os_dir>/ for target-wasm, and every
# consumer path (runtime.mk, configure.mk, install-target.mk) would have to guess
# which. Stating it makes the location a fact instead of a derivation.

.DEFAULT_GOAL := all

LIBNAME := llvm-project
BUILD_DIR := runtimes-build-embox-user

include ../common/utils/detect-platform.mk
include ../common/utils/init-shell.mk

EMBOX_USER_ENABLE_LIBCXXABI ?= 1
ifeq ($(EMBOX_USER_ENABLE_LIBCXXABI),1)
EMBOX_USER_RUNTIMES := compiler-rt;libunwind;libcxxabi
else
EMBOX_USER_RUNTIMES := compiler-rt;libunwind
endif

CONFIGURE := \
	-G "Ninja" \
	-S $(LIB_SRC_DIR)/$(LIBNAME)/runtimes \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE)) \
	-DCMAKE_PROJECT_INCLUDE=$(abspath $(MAKE_ROOT))/embox-user-project-include.cmake \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_INSTALL_LIBDIR=$(SP_INSTALL_PREFIX)/usr/lib \
	-DCMAKE_INSTALL_INCLUDEDIR=$(SP_INSTALL_PREFIX)/usr/include \
	-DLLVM_ENABLE_RUNTIMES="$(EMBOX_USER_RUNTIMES)" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLVM_ENABLE_PIC=Off \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
	-DLLVM_HOST_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
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
	-DCOMPILER_RT_OS_DIR=embox_user \
	-DCOMPILER_RT_INSTALL_PATH=$(SP_INSTALL_PREFIX)/lib/clang \
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
	cd $(BUILD_DIR); cmake --build .
	cd $(BUILD_DIR); cmake --install .
	$(call rule_rm,$(BUILD_DIR))

.PHONY: all
