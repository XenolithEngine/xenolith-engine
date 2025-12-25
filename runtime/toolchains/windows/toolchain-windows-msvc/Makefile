# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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
	wget -O $(MAKE_SRC_TARBALL) $(MAKE_SRC_URL)

$(MAKE_SRC_DIR): $(MAKE_SRC_TARBALL)
	tar -xf $(MAKE_SRC_TARBALL)
	touch $(MAKE_SRC_DIR)


ZLIB_VER := zlib-1.3.1
ZLIB_DIR := $(ZLIB_VER)
ZLIB_TARBALL := $(addsuffix .tar.gz,$(ZLIB_VER))
ZLIB_URL := https://www.zlib.net/$(ZLIB_TARBALL)

$(ZLIB_TARBALL):
	wget -O $(ZLIB_TARBALL) $(ZLIB_URL)

$(ZLIB_DIR): $(ZLIB_TARBALL)
	tar -xf $(ZLIB_TARBALL)
	touch $(ZLIB_DIR)


LLVM_MAJOR := 21
LLVM_V := $(LLVM_MAJOR).1.7
LLVM_VER := llvm-project-$(LLVM_V).src
LLVM_DIR := $(LLVM_VER)
LLVM_TARBALL := $(addsuffix .tar.xz,$(LLVM_VER))
LLVM_URL := https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_V)/llvm-project-$(LLVM_V).src.tar.xz

$(LLVM_TARBALL):
	wget -O $(LLVM_TARBALL) $(LLVM_URL)

$(LLVM_DIR): $(LLVM_TARBALL)
	tar -xf $(LLVM_TARBALL)
	touch $(LLVM_DIR)


OUT_SYSROOT := ../$(SP_ARCH_CLANG)

include stage0.mk

download: $(MAKE_SRC_DIR) $(ZLIB_DIR) $(LLVM_DIR)

out: $(OUT_SYSROOT)

all: stage0 stage1 stage2

.PHONY: stage0 stage1 stage2 all 
