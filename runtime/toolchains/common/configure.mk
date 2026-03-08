# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

DEBUG ?= 0

ifeq ($(findstring Windows,$(OS)),Windows)
SHELL = powershell.exe
MKDIR = powershell New-Item -ItemType Directory -Force -Path
RM := powershell Remove-Item -Recurse -Force -ErrorAction Ignore -Path 

rule_mkdir = powershell New-Item -ItemType Directory -Force -Path "$(1)" | Out-Null
rule_rm = powershell 'if (Test-Path "$(1)") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(1)" }'
rule_cp = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force
rule_cpr = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force -Recurse
rule_mv = powershell Move-Item -Path "$(1)" -Destination "$(2)" -Force

mklibname = $(addprefix lib,$(addsuffix .lib, $(1)))
else
MKDIR = mkdir -p
RM := rm -rf

rule_mkdir = mkdir -p $(1)
rule_rm = rm -rf $(1)
rule_cp = cp -f $(1) $(2)
rule_cpr = cp -rf $(1) $(2)
rule_mv = mv -f $(1) $(2)

mklibname = $(addprefix lib,$(addsuffix .a, $(1)))
endif

export PKG_CONFIG_PATH=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig

SP_CFLAGS := $(SP_OPT) $(SP_USER_CFLAGS) -resource-dir $(SP_INSTALL_PREFIX)/lib/clang --target=$(SP_TARGET)
SP_CXXFLAGS := $(SP_OPT) $(SP_USER_CXXFLAGS) -resource-dir $(SP_INSTALL_PREFIX)/lib/clang --target=$(SP_TARGET)
SP_CPPFLAGS := -I$(SP_INSTALL_PREFIX)/usr/include $(SP_USER_CPPFLAGS) -resource-dir $(SP_INSTALL_PREFIX)/lib/clang --target=$(SP_TARGET)
SP_LDFLAGS := -L$(SP_INSTALL_PREFIX)/usr/lib $(SP_USER_LDFLAGS) -resource-dir $(SP_INSTALL_PREFIX)/lib/clang --target=$(SP_TARGET)

ifdef SP_TOOLCHAIN_PREFIX
SP_CFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) \
	-idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc \
	$(SP_CFLAGS)
SP_CXXFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) \
	-idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc \
	$(SP_CXXFLAGS)
SP_CPPFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) \
	-idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc \
	$(SP_CPPFLAGS)
SP_LDFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) $(SP_LDFLAGS)
endif

ifdef WINDOWS
SP_CFLAGS +=  -D_CRT_SECURE_NO_WARNINGS -Wno-deprecated-declarations -Xclang --dependent-lib=libcmt
SP_CXXFLAGS +=  -D_CRT_SECURE_NO_WARNINGS -Wno-deprecated-declarations -Xclang --dependent-lib=libcmt
endif

SP_LIBS_ALL := 
SP_LIBS_PLATFORM := 
ifdef ANDROID
SP_LIBS_ALL := -lz -lm -landroid $(SP_LIBS_ALL)
SP_LIBS_PLATFORM := -landroid $(SP_LIBS_PLATFORM)
endif

#
# Autoconf helper
#

export PKG_CONFIG_PATH=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig

CONFIGURE_AUTOCONF := \
	CC=$(SP_CC) \
	CPP="$(SP_CC) -E" \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	CFLAGS="$(SP_CFLAGS)" \
	CXXFLAGS="$(SP_CXXFLAGS)" \
	CPPFLAGS="$(SP_CPPFLAGS)" \
	LDFLAGS="$(SP_LDFLAGS)" \
	PKG_CONFIG_PATH="$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig" \
	LIBS="$(SP_LIBS_ALL)" \
	--includedir=$(SP_INSTALL_PREFIX)/usr/include \
	--libdir=$(SP_INSTALL_PREFIX)/usr/lib \
	--bindir=$(MAKE_ROOT)$(LIBNAME)/prefix/bin \
	--sbindir=$(MAKE_ROOT)$(LIBNAME)/prefix/sbin \
	--datarootdir=$(MAKE_ROOT)$(LIBNAME)/prefix/share \
	--prefix=$(SP_INSTALL_PREFIX) \
	--enable-shared=no \
	--enable-static=yes

ifdef ANDROID
CONFIGURE_AUTOCONF += --host=$(SP_TARGET)
endif

ifdef LINUX
ifeq ($(ARCH),e2k)
CONFIGURE_AUTOCONF += --host=e2k-linux
else
endif

ifeq ($(ARCH),x86_64)
CONFIGURE_AUTOCONF += --host=x86_64-linux-gnu
endif

ifeq ($(ARCH),aarch64)
CONFIGURE_AUTOCONF += --host=aarch64-linux-gnu
endif

ifeq ($(ARCH),riscv64)
CONFIGURE_AUTOCONF += --host=riscv64-linux-gnu
endif
endif

ifdef WINDOWS
CONFIGURE_AUTOCONF += RC=$(SP_RC)
endif


#
# CMake helper
#

CONFIGURE_CMAKE_C_FLAGS_INIT := $(SP_OPT) $(SP_USER_CFLAGS)
CONFIGURE_CMAKE_CXX_FLAGS_INIT := $(SP_OPT) $(SP_USER_CXXFLAGS)
CONFIGURE_EXE_LINKER_FLAGS_INIT := $(SP_LIBS_PLATFORM) $(SP_USER_LDFLAGS)
CONFIGURE_SHARED_LINKER_FLAGS_INIT := $(SP_LIBS_PLATFORM) $(SP_USER_LDFLAGS)

ifndef WINDOWS
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

CONFIGURE_CMAKE :=

ifdef SP_TOOLCHAIN_FILE
CONFIGURE_CMAKE += -DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE))
endif

CONFIGURE_CMAKE += \
	-DSP_C_FLAGS="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DSP_CXX_FLAGS="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT)" \
	-DSP_EXE_LINKER_FLAGS="$(CONFIGURE_EXE_LINKER_FLAGS_INIT)" \
	-DSP_SHARED_LINKER_FLAGS="$(CONFIGURE_SHARED_LINKER_FLAGS_INIT)" \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_INSTALL_BINDIR=$(MAKE_ROOT)$(LIBNAME)/bin \
	-DCMAKE_INSTALL_DATAROOTDIR=$(MAKE_ROOT)$(LIBNAME)/share \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_SYSTEM_PROCESSOR=$(SP_ARCH) \
	-DCMAKE_OSX_ARCHITECTURES=$(SP_ARCH) \
#	-DCMAKE_VERBOSE_MAKEFILE=On


ifdef ANDROID
CONFIGURE_CMAKE += \
	-DCMAKE_SYSTEM_NAME=Android \
	-DCMAKE_ANDROID_ARCH=$(ANDROID_ARCH) \
	-DCMAKE_ANDROID_ARCH_ABI=$(ARCH) \
	-DCMAKE_ANDROID_NDK=$(NDK) \
	-DCMAKE_ANDROID_API=$(ANDROID_PLATFORM_LEVEL)
endif

ifdef LINUX

endif

ifdef WINDOWS
CONFIGURE_CMAKE += -DCMAKE_RC_COMPILER=$(SP_RC) -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
endif

ifeq ($(DEBUG),1)
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Debug
else
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Release
endif
