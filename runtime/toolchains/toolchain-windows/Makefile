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

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

SP_NJOBS := -j16
SP_ARCH := x86_64
SP_ARCH_LD := x86-64
SP_ARCH_CLANG ?= $(SP_ARCH)-pc-windows-msvc
SP_ARCH_LINUX := x86

# https://ftp.gnu.org/gnu/make/make-4.4.1.tar.lz
MAKE_SRC_VER := 4.4.1
MAKE_SRC_TARBALL := make-$(MAKE_SRC_VER).tar.lz
MAKE_SRC_DIR := make-$(MAKE_SRC_VER)
MAKE_SRC_URL := https://ftp.gnu.org/gnu/make/make-$(MAKE_SRC_VER).tar.lz

$(MAKE_SRC_TARBALL):
	powershell Invoke-WebRequest -Uri "$(MAKE_SRC_URL)" -OutFile "$(MAKE_SRC_TARBALL)"

$(MAKE_SRC_DIR): $(MAKE_SRC_TARBALL)
	powershell tar -xf $(MAKE_SRC_TARBALL)
	powershell touch $(MAKE_SRC_DIR)


LLVM_MAJOR := 21
LLVM_V := $(LLVM_MAJOR).1.8
LLVM_VER := llvm-project-$(LLVM_V).src
LLVM_DIR := $(LLVM_VER)
LLVM_TARBALL := $(addsuffix .tar.xz,$(LLVM_VER))
LLVM_URL := https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_V)/llvm-project-$(LLVM_V).src.tar.xz

$(LLVM_TARBALL):
	powershell Invoke-WebRequest -Uri "$(LLVM_URL)" -OutFile "$(LLVM_TARBALL)"

$(LLVM_DIR): $(LLVM_TARBALL)
	powershell tar -xf $(LLVM_TARBALL)
	powershell touch $(LLVM_DIR)

OUT_SYSROOT := ../$(SP_ARCH_CLANG)

include stage0.mk

$(OUT_SYSROOT): $(STAGE0_SYSROOT)
	rm -rf $(OUT_SYSROOT)
	mkdir -p $(OUT_SYSROOT)
	cp -rf $(STAGE0_SYSROOT)/lib $(OUT_SYSROOT)
	cp -rf $(STAGE0_SYSROOT)/include $(OUT_SYSROOT)
	mkdir $(OUT_SYSROOT)/bin
	cp -f $(STAGE0_SYSROOT)/bin/clang.exe $(OUT_SYSROOT)/bin
	cp -f $(STAGE0_SYSROOT)/bin/lld.exe $(OUT_SYSROOT)/bin
	cp -f $(STAGE0_SYSROOT)/bin/llvm-ar.exe $(OUT_SYSROOT)/bin
	cp -f $(STAGE0_SYSROOT)/bin/LLVM-C.dll $(OUT_SYSROOT)/bin
	cp -f $(STAGE0_SYSROOT)/bin/LTO.dll $(OUT_SYSROOT)/bin
	cp -f $(STAGE0_SYSROOT)/bin/libclang.dll $(OUT_SYSROOT)/bin

	@echo 'TOOLCHAIN_SYSROOT := $$(abspath $$(dir $$(lastword $$(MAKEFILE_LIST))))' > $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_TARGET := $(SP_ARCH_CLANG)' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_LIBDIR := $$(TOOLCHAIN_SYSROOT)/lib' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_INCLUDEDIR := $$(TOOLCHAIN_SYSROOT)/include' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_CC := $$(TOOLCHAIN_SYSROOT)/bin/clang.exe' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_CXX := $$(TOOLCHAIN_SYSROOT)/bin/clang.exe' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_AR := $$(TOOLCHAIN_SYSROOT)/bin/llvm-ar.exe' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_GENERAL_CFLAGS := -nostdinc -isystem $$(TOOLCHAIN_SYSROOT)/lib/clang/21' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_GENERAL_CXXFLAGS := -nostdinc -isystem $$(TOOLCHAIN_SYSROOT)/lib/clang/21' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_LIB_LDFLAGS := -L$$(TOOLCHAIN_LIBDIR) -llibc++' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_EXEC_LDFLAGS := -L$$(TOOLCHAIN_LIBDIR) -llibc++' >> $(OUT_SYSROOT)/toolchain.mk
	@echo '$$(info Available toolchain: $$(TOOLCHAIN_TARGET): $$(TOOLCHAIN_SYSROOT))' >> $(OUT_SYSROOT)/toolchain.mk

download: $(MAKE_SRC_DIR) $(LLVM_DIR)

out: $(OUT_SYSROOT)

all: stage0

.PHONY: stage0 all 
