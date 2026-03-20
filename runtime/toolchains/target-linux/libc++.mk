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

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLVM_ENABLE_PIC=On \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_HOST_TRIPLE="$(SP_TARGET)" \
	-DLLVM_DEFAULT_TARGET_TRIPLE="$(SP_TARGET)" \
	-DLIBCXX_HAS_ATOMIC_LIB=Off \
	-DLIBCXX_ENABLE_SHARED=Off \
	-DLIBCXX_USE_COMPILER_RT=On \
	-DLIBCXX_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_USE_LLVM_UNWINDER=On \
	-DLIBCXXABI_USE_COMPILER_RT=On \
	-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
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
	cd $(LIBNAME); cmake  --build . --config Release --target install-cxx
	cd $(LIBNAME); cmake  --build . --config Release --target install-cxxabi
	cd $(LIBNAME); cmake  --build . --config Release --target install
	$(call rule_rm,$(LIBNAME))

.PHONY: all
