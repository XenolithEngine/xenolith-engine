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

.DEFAULT_GOAL := all

LIBNAME = llvm-project

# libcxxabi/libunwind компилируются с заголовками STL из тела рантайма: libc++
# в deps-сборке больше не строится, а SDK-кланг (CLANG_DEFAULT_CXX_STDLIB=libc++)
# без -nostdinc++ нашёл бы c++/v1 sysroot'а раньше (тот же приём, что в
# common/harfbuzz.mk для WINDOWS/LINUX).
#
# Порядок инклудов обратный shim-дизайну (там include_libc/cxx первым): на
# hosted-linux vendored libcxx/include работает в glibc-режиме и обязан
# выигрывать — иначе shim навязывает sprt-определение __mbstate_t, которое
# конфликтует с glibc (typedef redefinition в cxa_demangle.cpp). Шиму остаётся
# только недостающее — <__config_site> (в дереве исходников его нет).
SP_USER_CXXFLAGS += -nostdinc++ \
	-isystem $(realpath $(dir $(CONFIGURE_MAKEFILE))/../../libcxx/include) \
	-isystem $(realpath $(dir $(CONFIGURE_MAKEFILE))/../../include_libc/cxx) \
	-isystem $(realpath $(dir $(CONFIGURE_MAKEFILE))/../../include)

include ../common/configure.mk

include libcxx-unwinder.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DLLVM_ENABLE_RUNTIMES="libcxxabi;libunwind" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLVM_ENABLE_PIC=On \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_HOST_TRIPLE="$(SP_TARGET)" \
	-DLLVM_DEFAULT_TARGET_TRIPLE="$(SP_TARGET)" \
	-DLIBCXXABI_ENABLE_EXCEPTIONS=OFF \
	-DLIBCXXABI_USE_LLVM_UNWINDER=On \
	-DLIBCXXABI_USE_COMPILER_RT=On \
	-DLIBCXXABI_ENABLE_STATIC_UNWINDER=$(LIBCXX_STATIC_UNWINDER) \
	-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_ENABLE_SHARED=Off \
	-DLIBUNWIND_USE_COMPILER_RT=On \
	-DLIBUNWIND_ENABLE_SHARED=Off \
	-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
	-DCMAKE_BUILD_TYPE=Release

ifeq ($(SP_ARCH),riscv64)
RISCV := 1
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" -S $(LIB_SRC_DIR)/$(LIBNAME)/runtimes $(CONFIGURE)
	cd $(LIBNAME); cmake  --build . --config Release --target install-cxxabi
	cd $(LIBNAME); cmake  --build . --config Release --target install
	$(call rule_rm,$(LIBNAME))

.PHONY: all
