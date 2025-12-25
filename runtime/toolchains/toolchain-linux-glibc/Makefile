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
SP_ARCH_CLANG ?= $(SP_ARCH)-unknown-linux-gnu
SP_ARCH_LINUX := x86

# Используем последний доступный GCC
# https://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.gz
GCC_SRC_VER := 15.2.0
GCC_SRC_TARBALL := gcc-$(GCC_SRC_VER).tar.xz
GCC_SRC_DIR := gcc-$(GCC_SRC_VER)
GCC_SRC_URL := https://ftp.gnu.org/gnu/gcc/gcc-$(GCC_SRC_VER)/gcc-$(GCC_SRC_VER).tar.gz

$(GCC_SRC_TARBALL):
	wget -O $(GCC_SRC_TARBALL) $(GCC_SRC_URL)

$(GCC_SRC_DIR): $(GCC_SRC_TARBALL)
	tar -xf $(GCC_SRC_TARBALL)
	
	touch $(GCC_SRC_DIR)

# Заголовки и glibc должны соотвествовать друг другу и быть минимально доступной версии
# Используем LTS ядро 5.10

LINUX_KERNEL_FAMILY := v5.x
LINUX_KERNEL_VER := 5.10.247
LINUX_KERNEL_TARBALL := linux-$(LINUX_KERNEL_VER).tar.xz
LINUX_KERNEL_DIR := linux-$(LINUX_KERNEL_VER)
LINUX_KERNEL_URL := https://cdn.kernel.org/pub/linux/kernel/$(LINUX_KERNEL_FAMILY)/linux-$(LINUX_KERNEL_VER).tar.xz

$(LINUX_KERNEL_TARBALL):
	wget -O $(LINUX_KERNEL_TARBALL) $(LINUX_KERNEL_URL)

$(LINUX_KERNEL_DIR): $(LINUX_KERNEL_TARBALL)
	tar -xf $(LINUX_KERNEL_TARBALL)
	touch $(LINUX_KERNEL_DIR)

LINUX_HEADERS_DIR := linux-headers-$(SP_ARCH_LINUX)

$(LINUX_HEADERS_DIR): $(LINUX_KERNEL_DIR)
	mkdir -p $(LINUX_HEADERS_DIR)
	cd $(LINUX_KERNEL_DIR); make headers_install ARCH=i386 INSTALL_HDR_PATH=../$(LINUX_HEADERS_DIR)

# Используем 2.26 для первоначального запуска, поскольку более поздние требуют libstdc++
GLIBC_MIN_SRC_VER := 2.26
GLIBC_MIN_SRC_TARBALL := glibc-$(GLIBC_MIN_SRC_VER).tar.xz
GLIBC_MIN_SRC_DIR := glibc-$(GLIBC_MIN_SRC_VER)
GLIBC_MIN_SRC_URL := https://ftp.gnu.org/gnu/glibc/glibc-$(GLIBC_MIN_SRC_VER).tar.xz

$(GLIBC_MIN_SRC_TARBALL):
	wget -O $(GLIBC_MIN_SRC_TARBALL) $(GLIBC_MIN_SRC_URL)

$(GLIBC_MIN_SRC_DIR): $(GLIBC_MIN_SRC_TARBALL)
	tar -xf $(GLIBC_MIN_SRC_TARBALL)
	touch $(GLIBC_MIN_SRC_DIR)


# Используем 2.33 как ближайшую к целевой версии ядра
GLIBC_SRC_VER := 2.33
GLIBC_SRC_TARBALL := glibc-$(GLIBC_SRC_VER).tar.xz
GLIBC_SRC_DIR := glibc-$(GLIBC_SRC_VER)
GLIBC_SRC_URL := https://ftp.gnu.org/gnu/glibc/glibc-$(GLIBC_SRC_VER).tar.xz

$(GLIBC_SRC_TARBALL):
	wget -O $(GLIBC_SRC_TARBALL) $(GLIBC_SRC_URL)

$(GLIBC_SRC_DIR): $(GLIBC_SRC_TARBALL)
	tar -xf $(GLIBC_SRC_TARBALL)
	touch $(GLIBC_SRC_DIR)


# https://ftp.gnu.org/gnu/make/make-4.3.tar.lz
# Нам нужен make-4.3 чтобы собирать старые версии glibc
# Более новые версии будут уходить в бесконечный цикл
MAKE_SRC_VER := 4.3
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
LLVM_V := $(LLVM_MAJOR).1.8
LLVM_VER := llvm-project-$(LLVM_V).src
LLVM_DIR := $(LLVM_VER)
LLVM_TARBALL := $(addsuffix .tar.xz,$(LLVM_VER))
LLVM_URL := https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_V)/llvm-project-$(LLVM_V).src.tar.xz

$(LLVM_TARBALL):
	wget -O $(LLVM_TARBALL) $(LLVM_URL)

$(LLVM_DIR): $(LLVM_TARBALL)
	tar -xf $(LLVM_TARBALL)
	touch $(LLVM_DIR)


include stage0.mk
include stage1.mk
include stage2.mk

OUT_SYSROOT ?= ../$(SP_ARCH_CLANG)

OUT_TOOLS_TO_REMOVE := \
	amdgpu-arch \
	catchsegv \
	clang-format \
	clang-installapi \
	clang-linker-wrapper \
	clang-nvlink-wrapper \
	clang-offload-bundler \
	clang-offload-packager \
	clang-refactor \
	clang-repl \
	clang-scan-deps \
	clang-sycl-linker \
	diagtool \
	gencat \
	getconf \
	getent \
	git-clang-format \
	hmaptool \
	iconv \
	ldd \
	llvm-cov \
	llvm-cxxfilt \
	llvm-dwp \
	llvm-mca \
	llvm-ml \
	llvm-nm \
	llvm-objcopy \
	llvm-objdump \
	llvm-pdbutil \
	llvm-profdata \
	llvm-profgen \
	llvm-readobj \
	llvm-size \
	llvm-strings \
	llvm-strip \
	llvm-symbolizer \
	locale \
	localedef \
	makedb \
	mtrace \
	nvptx-arch \
	offload-arch \
	pcprofiledump \
	pldd \
	sotruss \
	sprof \
	xtrace \


OUT_LIBS_TO_REMOVE := \
	libm.a \
	libmvec.a \
	libm-*.a \
	libclang.a \
	libclang.so \
	libclang.so.* \
	libpthread.a \


$(OUT_SYSROOT): $(STAGE2_CLANG_CC) $(STAGEOUT_GLIBC)
	rm -rf $(OUT_SYSROOT)
	cp -rf $(STAGEOUT_SYSROOT) $(OUT_SYSROOT)
	cd $(OUT_SYSROOT)/bin; rm -f $(OUT_TOOLS_TO_REMOVE)
	cd $(OUT_SYSROOT)/lib; rm -f $(OUT_LIBS_TO_REMOVE)
	cd $(OUT_SYSROOT)/lib; rm -rf audit gconv pkgconfig
	cd $(OUT_SYSROOT)/share; rm -rf i18n locale man
	rm -rf $(OUT_SYSROOT)/var $(OUT_SYSROOT)/etc $(OUT_SYSROOT)/libexec
	cd $(OUT_SYSROOT)/bin; ln -s -f clang cc
	cd $(OUT_SYSROOT)/bin; ln -s -f clang c++
	cd $(OUT_SYSROOT)/bin; ln -s -f llvm-ar ar
	
	@echo 'TOOLCHAIN_SYSROOT := $$(abspath $$(dir $$(lastword $$(MAKEFILE_LIST))))' > $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_TARGET := $(SP_ARCH_CLANG)' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_LIBDIR := $$(TOOLCHAIN_SYSROOT)/lib' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_INCLUDEDIR := $$(TOOLCHAIN_SYSROOT)/include' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_INCLUDEDIR_LIBC := $$(TOOLCHAIN_SYSROOT)/include_libc' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_CC := $$(TOOLCHAIN_SYSROOT)/bin/clang' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_CXX := $$(TOOLCHAIN_SYSROOT)/bin/clang++' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_AR := $$(TOOLCHAIN_SYSROOT)/bin/llvm-ar' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_GLSLANG := $$(TOOLCHAIN_SYSROOT)/bin/glslang' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_SPIRV_LINK := $$(TOOLCHAIN_SYSROOT)/bin/spirv-link' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_GENERAL_CXXFLAGS := -stdlib=libc++' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_LIB_LDFLAGS := -lc++ -lc++abi' >> $(OUT_SYSROOT)/toolchain.mk
	@echo 'TOOLCHAIN_EXEC_LDFLAGS := -lc++ -lc++abi' >> $(OUT_SYSROOT)/toolchain.mk
	@echo '$$(info Available toolchain: $$(TOOLCHAIN_TARGET): $$(TOOLCHAIN_SYSROOT))' >> $(OUT_SYSROOT)/toolchain.mk

download: $(GLIBC_MIN_SRC_DIR) $(GCC_SRC_DIR) $(MAKE_SRC_DIR) $(ZLIB_DIR) $(LLVM_DIR) $(LINUX_KERNEL_DIR) $(LINUX_HEADERS_DIR) $(GLIBC_SRC_DIR)

out: $(OUT_SYSROOT)

all: stage0 stage1 stage2

.PHONY: stage0 stage1 stage2 all 
