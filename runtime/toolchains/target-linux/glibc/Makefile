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

SP_SRC_DIR := $(abspath $(dir $(LOCAL_MAKEFILE))../../src)

include ../../host-linux-glibc/glibc.mk

BINUTILS_SRC_VER := 2.46.0
BINUTILS_SRC_TARBALL := binutils-$(BINUTILS_SRC_VER).tar.xz
BINUTILS_SRC_DIR := src/binutils
BINUTILS_SRC_URL := https://ftp.gnu.org/gnu/binutils/binutils-$(BINUTILS_SRC_VER).tar.gz

$(eval $(call download_tarbell_target,$(BINUTILS_SRC_DIR),$(BINUTILS_SRC_TARBALL),$(BINUTILS_SRC_URL)))

LINUX_HOST_HEADERS_DIR := src/linux-headers-host-$(SP_ARCH_TARGET)

$(LINUX_HOST_HEADERS_DIR): $(LINUX_KERNEL_DIR)
	mkdir -p $@
	cd $(LINUX_KERNEL_DIR); make headers_install ARCH=$(SP_ARCH_HOST_LINUX) INSTALL_HDR_PATH=../$(notdir $(LINUX_HOST_HEADERS_DIR))
	cd $(LINUX_KERNEL_DIR); make clean
	touch -m -r src/$(LINUX_KERNEL_TARBALL) $(LINUX_KERNEL_DIR)

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
	)

OUT_USR_LIBS := $(addprefix $(OUT_SYSROOT)/usr/lib/,\
	libunwind.a \
	)

$(OUT_SYSROOT)/lib/%: $(TARGET_SYSROOT)/lib/% $(TARGET_GLIBC)
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	cp -f $< $@
	chmod +x $@

$(OUT_SYSROOT)/usr/lib/%: $(TARGET_SYSROOT)/lib/% $(TARGET_GLIBC)
	@mkdir -p $(OUT_SYSROOT)/usr/lib
	@rm -f $@
	cp -f $< $@
	chmod +x $@

ifeq ($(SP_ARCH_TARGET),riscv64)

OUT_RTS_FILES := \
	clang_rt.crtbegin.o \
	clang_rt.crtend.o \
	libclang_rt.profile.a \
	libclang_rt.builtins.a

else

OUT_RTS_FILES := \
	clang_rt.crtbegin.o \
	clang_rt.crtend.o \
	libclang_rt.profile.a \
	libclang_rt.builtins.a \
	liborc_rt.a \

endif

OUT_RTS := $(addprefix $(OUT_SYSROOT)/lib/clang/lib/$(SP_ARCH_TARGET_CLANG)/,$(OUT_RTS_FILES))

$(OUT_SYSROOT)/lib/clang/lib/$(SP_ARCH_TARGET_CLANG)/clang_rt.crtbegin.o: \
		$(TARGET_SYSROOT)/lib/linux/clang_rt.crtbegin-$(SP_ARCH_TARGET).o
	@mkdir -p $(dir $@)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/lib/$(SP_ARCH_TARGET_CLANG)/clang_rt.crtend.o: \
		$(TARGET_SYSROOT)/lib/linux/clang_rt.crtend-$(SP_ARCH_TARGET).o
	@mkdir -p $(dir $@)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/lib/$(SP_ARCH_TARGET_CLANG)/libclang_rt.profile.a: \
		$(TARGET_SYSROOT)/lib/linux/libclang_rt.profile-$(SP_ARCH_TARGET).a
	@mkdir -p $(dir $@)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/lib/$(SP_ARCH_TARGET_CLANG)/liborc_rt.a: \
		$(TARGET_SYSROOT)/lib/linux/liborc_rt-$(SP_ARCH_TARGET).a
	@mkdir -p $(dir $@)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/lib/$(SP_ARCH_TARGET_CLANG)/libclang_rt.builtins.a: \
		$(TARGET_SYSROOT)/lib/linux/libclang_rt.builtins-$(SP_ARCH_TARGET).a
	@mkdir -p $(dir $@)
	@rm -f $@
	cp -f $< $@

$(OUT_SYSROOT)/lib/clang/lib/linux: $(TARGET_SYSROOT)/lib/linux $(TARGET_LIBCRT)
	@mkdir -p $(dir $@)
	@rm -rf $@
	cp -rf $< $@

$(OUT_SYSROOT)/include: $(TARGET_GLIBC)
	@rm -rf $(OUT_SYSROOT)/include
	@mkdir -p $(OUT_SYSROOT)/include
	cp -rf $(TARGET_SYSROOT)/include/* $(OUT_SYSROOT)/include

$(OUT_SYSROOT)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Linux' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH_TARGET)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/include_libc' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang -lc++ -lc++abi' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@

$(OUT_SYSROOT)/target.ini: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo '[host_machine]' > $@
	@echo "system = 'linux'" >> $@
	@echo "cpu_family = '$(SP_ARCH_TARGET)'" >> $@
	@echo "cpu = '$(SP_ARCH_TARGET)'" >> $@
	@echo "endian = 'little'" >> $@
	@echo '[binaries]' >> $@
	@echo "c = '@DIRNAME@/host/bin/cc'" >> $@
	@echo "cpp = '@DIRNAME@/host/bin/c++'" >> $@
	@echo "ar = '@DIRNAME@/host/bin/llvm-ar'" >> $@
	@echo "strip = '@DIRNAME@/host/bin/llvm-strip'" >> $@
	@echo "pkg-config = 'pkg-config'" >> $@
	@echo '[properties]' >> $@
	@echo "sys_root = '@DIRNAME@'" >> $@
	@echo "pkg_config_libdir = '@DIRNAME@/usr/lib/pkgconfig'" >> $@
	@echo '[built-in options]' >> $@
	@echo "prefix = '@DIRNAME@'" >> $@
	@echo "libdir = '@DIRNAME@/lib'" >> $@
	@echo "c_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@
	@echo "c_link_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@
	@echo "cpp_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@
	@echo "cpp_link_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@

$(OUT_SYSROOT)/toolchain.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR "$(SP_ARCH_TARGET)")' >> $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS}  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS}  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS}  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH Off)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(PKG_CONFIG_PATH "$${CMAKE_CURRENT_LIST_DIR}/usr/lib/pkgconfig")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_PREFIX_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(CMAKE_INSTALL_PREFIX "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_INSTALL_LIBDIR "$${CMAKE_CURRENT_LIST_DIR}/usr/lib")' >> $@
	@echo 'set(CMAKE_INSTALL_INCLUDEDIR "$${CMAKE_CURRENT_LIST_DIR}/usr/include")' >> $@

$(OUT_SYSROOT)/sysroot: $(OUT_LIBS) $(OUT_USR_LIBS) $(OUT_SYSROOT)/lib/clang/lib/linux $(OUT_SYSROOT)/include \
		$(OUT_SYSROOT)/toolchain.cmake $(OUT_SYSROOT)/target.mk $(OUT_SYSROOT)/target.ini
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
	cd $(OUT_SYSROOT)/lib/clang; ln -fs ../../host/lib/clang/21/include include
endif
	touch $@

download: $(LINUX_TARGET_HEADERS_DIR)

out: download host target $(OUT_SYSROOT)/sysroot

all: download host target

.PHONY: host target all download
