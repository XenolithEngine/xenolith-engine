# Copyright (c) 2026 Xenolith Team <admin@xenloth.studio>
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

SP_NJOBS ?= -j16
SP_ARCH_TARGET ?= x86_64
SP_ARCH_TARGET_LINUX ?= $(SP_ARCH_TARGET)
SP_ARCH_TARGET_CLANG ?= $(SP_ARCH_TARGET)-unknown-linux-gnu

ifeq ($(SP_ARCH_TARGET_LINUX),x86_64)
SP_LDSO_NAME := ld-linux-x86-64.so.2

else ifeq ($(SP_ARCH_TARGET_LINUX),aarch64)
SP_ARCH_TARGET_LINUX := arm64
SP_LDSO_NAME := ld-linux-aarch64.so.1

else ifeq ($(SP_ARCH_TARGET_LINUX),riscv64)
SP_ARCH_TARGET_LINUX := riscv
SP_LDSO_NAME := ld-linux-riscv64-lp64d.so.1
GLIBC_SRC_VER ?= 2.35

else
$(error Unknown target arch: $(SP_ARCH_TARGET))
endif

SP_ARCH_HOST := $(shell uname -m)
SP_ARCH_HOST_LINUX ?= $(SP_ARCH_HOST)
SP_ARCH_HOST_CLANG := $(SP_ARCH_HOST)-unknown-linux-gnu

ifeq ($(SP_ARCH_HOST_LINUX),aarch64)
SP_ARCH_HOST_LINUX := arm64
endif


include ../host-linux-glibc/glibc.mk

BINUTILS_SRC_VER := 2.46.0
BINUTILS_SRC_TARBALL := binutils-$(BINUTILS_SRC_VER).tar.xz
BINUTILS_SRC_DIR := src/binutils
BINUTILS_SRC_URL := https://ftp.gnu.org/gnu/binutils/binutils-$(BINUTILS_SRC_VER).tar.gz

$(eval $(call download_tarbell_target,$(BINUTILS_SRC_DIR),$(BINUTILS_SRC_TARBALL),$(BINUTILS_SRC_URL)))

LINUX_TARGET_HEADERS_DIR := src/linux-headers-target-$(SP_ARCH_TARGET)

$(LINUX_TARGET_HEADERS_DIR): $(LINUX_KERNEL_DIR)
	mkdir -p $@
	cd $(LINUX_KERNEL_DIR); make headers_install ARCH=$(SP_ARCH_TARGET_LINUX) INSTALL_HDR_PATH=../$(notdir $(LINUX_TARGET_HEADERS_DIR))
	cd $(LINUX_KERNEL_DIR); make clean
	touch -m -r src/$(LINUX_KERNEL_TARBALL) $(LINUX_KERNEL_DIR)

include host.mk
include target.mk

OUT_SYSROOT ?= $(SP_ARCH_TARGET_CLANG)

OUT_LIBS := $(addprefix $(OUT_SYSROOT)/lib/,\
	crt1.o \
	crti.o \
	crtn.o \
	gcrt1.o \
	$(SP_LDSO_NAME) \
	libanl.so.1 \
	libBrokenLocale.so.1 \
	libc.so.6 \
	libc_nonshared.a \
	libc.so \
	libdl.so.2 \
	libm.so.6 \
	libmemusage.so \
	libpthread.so.0 \
	libresolv.so.2 \
	librt.so.1 \
	libthread_db.so.1 \
	libutil.so.1 \
	Mcrt1.o \
	Scrt1.o \
	libunwind.a \
	)

$(OUT_SYSROOT)/lib/%: $(TARGET_SYSROOT)/lib/% $(TARGET_GLIBC)
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	cp -f $< $@
	chmod +x $@


OUT_RTS := $(addprefix $(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)/,\
	clang_rt.crtbegin.o \
	clang_rt.crtend.o \
	libclang_rt.profile.a \
	liborc_rt.a \
	libclang_rt.builtins.a \
)

$(OUT_SYSROOT)/lib/clang/21/include: $(TARGET_SYSROOT)/lib/clang
	@mkdir -p $@
	cp -rf $(TARGET_SYSROOT)/lib/clang/21/include/* $@

$(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)/clang_rt.crtbegin.o: \
		$(TARGET_SYSROOT)/lib/linux/clang_rt.crtbegin-$(SP_ARCH_TARGET).o $(OUT_SYSROOT)/lib/clang/21/include
	@mkdir -p $(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)/clang_rt.crtend.o: \
		$(TARGET_SYSROOT)/lib/linux/clang_rt.crtend-$(SP_ARCH_TARGET).o $(OUT_SYSROOT)/lib/clang/21/include
	@mkdir -p $(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)/libclang_rt.profile.a: \
		$(TARGET_SYSROOT)/lib/linux/libclang_rt.profile-$(SP_ARCH_TARGET).a $(OUT_SYSROOT)/lib/clang/21/include
	@mkdir -p $(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)/liborc_rt.a: \
		$(TARGET_SYSROOT)/lib/linux/liborc_rt-$(SP_ARCH_TARGET).a $(OUT_SYSROOT)/lib/clang/21/include
	@mkdir -p $(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)/libclang_rt.builtins.a: \
		$(TARGET_SYSROOT)/lib/linux/libclang_rt.builtins-$(SP_ARCH_TARGET).a $(OUT_SYSROOT)/lib/clang/21/include
	@mkdir -p $(OUT_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_TARGET_CLANG)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/include: $(TARGET_GLIBC)
	@rm -rf $(OUT_SYSROOT)/include
	@mkdir -p $(OUT_SYSROOT)/include
	cp -rf $(TARGET_SYSROOT)/include/* $(OUT_SYSROOT)/include

$(OUT_SYSROOT)/toolchain.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@

$(OUT_SYSROOT): $(OUT_LIBS) $(OUT_SYSROOT)/include $(OUT_SYSROOT)/toolchain.cmake \
		$(OUT_RTS) $(lastword $(MAKEFILE_LIST))
	cd $(OUT_SYSROOT)/lib; ln -fs libanl.so.1 libanl.so
	cd $(OUT_SYSROOT)/lib; ln -fs libBrokenLocale.so.1 libBrokenLocale.so
	cd $(OUT_SYSROOT)/lib; ln -fs libdl.so.2 libdl.so
	cd $(OUT_SYSROOT)/lib; ln -fs libm.so.6 libm.so
	cd $(OUT_SYSROOT)/lib; ln -fs libpthread.so.0 libpthread.so
	cd $(OUT_SYSROOT)/lib; ln -fs libresolv.so.2 libresolv.so
	cd $(OUT_SYSROOT)/lib; ln -fs librt.so.1 librt.so
	cd $(OUT_SYSROOT)/lib; ln -fs libthread_db.so.1 libthread_db.so
	cd $(OUT_SYSROOT)/lib; ln -fs libutil.so.1 libutil.so

ifdef OUT_HOST
	rm -f $(OUT_SYSROOT)/host
	cd $(OUT_SYSROOT); ln -fs $(OUT_HOST) host
endif

download: $(LINUX_TARGET_HEADERS_DIR)

out: $(OUT_SYSROOT)

all: host target

.PHONY: host target all download
