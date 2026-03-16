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

CONFIGURE_MAKEFILE := $(lastword $(MAKEFILE_LIST))

include $(dir $(CONFIGURE_MAKEFILE))utils/init-shell.mk
include $(dir $(CONFIGURE_MAKEFILE))utils/names.mk

ifdef DARWIN
SP_MACOS_SDK ?= $(abspath $(LIB_SRC_DIR))/MacOSX.sdk
SP_IOS_SDK ?= $(abspath $(LIB_SRC_DIR))/iPhoneOS.sdk
SP_IOSSIM_SDK ?= $(abspath $(LIB_SRC_DIR))/iPhoneSimulator.sdk
endif

export PKG_CONFIG_PATH=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig

#
# Эти флаги будут использоваться при сборке без CMake.
#
SP_CFLAGS := $(SP_OPT) $(SP_USER_CFLAGS) --target=$(SP_TARGET) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS := $(SP_OPT) $(SP_USER_CXXFLAGS) --target=$(SP_TARGET) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CPPFLAGS := --target=$(SP_TARGET) -isystem $(SP_INSTALL_PREFIX)/usr/include $(SP_USER_CPPFLAGS)
SP_LDFLAGS := --target=$(SP_TARGET) -L$(SP_INSTALL_PREFIX)/usr/lib $(SP_USER_LDFLAGS)

# Если используется SP_TOOLCHAIN_FILE, значит, мы используем разделёные HOST и TARGET файлы, и
# -resource-dir нужно явно определить внутри TARGET
ifdef SP_TOOLCHAIN_FILE
SP_CFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
SP_CXXFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
SP_CPPFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
SP_LDFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
endif

ifdef SP_TOOLCHAIN_PREFIX
SP_CFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_CFLAGS)
SP_CXXFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_CXXFLAGS)
SP_CPPFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_CPPFLAGS)
SP_LDFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) $(SP_LDFLAGS)
endif # SP_TOOLCHAIN_PREFIX

ifdef WINDOWS
SP_CFLAGS +=  -D_CRT_SECURE_NO_WARNINGS -Wno-deprecated-declarations -Xclang --dependent-lib=libcmt
SP_CXXFLAGS +=  -D_CRT_SECURE_NO_WARNINGS -Wno-deprecated-declarations -Xclang --dependent-lib=libcmt
endif # WINDOWS

ifdef DARWIN

ifeq ($(SP_SYSNAME),Darwin)
SP_CFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -isysroot $(SP_MACOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -isysroot $(SP_MACOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_MACOS_SDK)/usr/lib -F$(SP_MACOS_SDK)/System/Library/Frameworks
endif # SP_SYSNAME Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
SP_CFLAGS += -mmacosx-version-min=$(SP_IOS_VER) -isysroot $(SP_IOSSIM_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mmacosx-version-min=$(SP_IOS_VER) -isysroot $(SP_IOSSIM_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mmacosx-version-min=$(SP_IOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_IOSSIM_SDK)/usr/lib -F$(SP_IOSSIM_SDK)/System/Library/Frameworks
else # SP_IOSSIM
SP_CFLAGS += -mmacosx-version-min=$(SP_IOS_VER) -isysroot $(SP_IOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mmacosx-version-min=$(SP_IOS_VER) -isysroot $(SP_IOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mmacosx-version-min=$(SP_IOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_IOS_SDK)/usr/lib -F$(SP_IOS_SDK)/System/Library/Frameworks
endif # SP_IOSSIM
endif # iOS

endif # DARWIN


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
	--includedir=$(SP_INSTALL_PREFIX)/usr/include \
	--libdir=$(SP_INSTALL_PREFIX)/usr/lib \
	--bindir=$(MAKE_ROOT)$(LIBNAME)/prefix/bin \
	--sbindir=$(MAKE_ROOT)$(LIBNAME)/prefix/sbin \
	--datarootdir=$(MAKE_ROOT)$(LIBNAME)/prefix/share \
	--prefix=$(SP_INSTALL_PREFIX) \
	--enable-shared=no \
	--enable-static=yes

CONFIGURE_AUTOCONF += --host=$(CONFIGURE_HOST_$(SP_SYSNAME)_$(SP_ARCH))

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

ifdef LINUX
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

ifdef ANDROID
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

CONFIGURE_CMAKE :=

ifdef SP_TOOLCHAIN_FILE

CONFIGURE_CMAKE += -DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE))

else # SP_TOOLCHAIN_FILE

ifdef ANDROID
CONFIGURE_CMAKE += \
	-DCMAKE_SYSTEM_NAME=Android \
	-DCMAKE_ANDROID_ARCH=$(ANDROID_ARCH) \
	-DCMAKE_ANDROID_ARCH_ABI=$(ANDROID_ARCH_ABI) \
	-DCMAKE_ANDROID_NDK=$(NDK) \
	-DCMAKE_ANDROID_API=$(ANDROID_PLATFORM_LEVEL) \
	-DANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	-DANDROID_NDK=$(NDK)
endif # ANDROID

CONFIGURE_CMAKE += \
	-DCMAKE_ASM_FLAGS_INIT="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DCMAKE_C_FLAGS_INIT="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DCMAKE_CXX_FLAGS_INIT="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT)" \
	-DCMAKE_EXE_LINKER_FLAGS_INIT="$(CONFIGURE_EXE_LINKER_FLAGS_INIT)" \
	-DCMAKE_SHARED_LINKER_FLAGS_INIT="$(CONFIGURE_SHARED_LINKER_FLAGS_INIT)" \
	-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=Off \
	-DCMAKE_FIND_ROOT_PATH="$(SP_INSTALL_PREFIX);$(SP_INSTALL_PREFIX)/usr" \
	-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
	-DCMAKE_PREFIX_PATH="${SP_INSTALL_PREFIX};${SP_INSTALL_PREFIX}/usr" \
	-DCMAKE_INSTALL_PREFIX="${SP_INSTALL_PREFIX}" \
	-DCMAKE_INSTALL_LIBDIR="${SP_INSTALL_PREFIX}/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="${SP_INSTALL_PREFIX}/usr/include" \
	-DPKG_CONFIG_PATH="$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig"
endif # SP_TOOLCHAIN_FILE


ifdef DARWIN

CONFIGURE_CMAKE_C_FLAGS_INIT += -isystem $(SP_INSTALL_PREFIX)/usr/include
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -isystem $(SP_INSTALL_PREFIX)/usr/include

CONFIGURE_CMAKE += \
	-DCMAKE_LIBTOOL="$(SP_INSTALL_PREFIX)/host/bin/llvm-libtool-darwin" \
	-DCMAKE_LIPO="$(SP_INSTALL_PREFIX)/host/bin/llvm-lipo"  \
	-DCMAKE_C_COMPILER=$(SP_CC) \
	-DCMAKE_CXX_COMPILER=$(SP_CXX) \

CONFIGURE_EXE_LINKER_FLAGS_INIT := -L$(SP_INSTALL_PREFIX)/usr/lib
CONFIGURE_SHARED_LINKER_FLAGS_INIT := -L$(SP_INSTALL_PREFIX)/usr/lib

ifeq ($(SP_SYSNAME),Darwin)
CONFIGURE_CMAKE += -DCMAKE_OSX_SYSROOT=$(SP_MACOS_SDK) \
	-DCMAKE_FRAMEWORK_PATH="$(SP_MACOS_SDK)/System/Library/Frameworks"
CONFIGURE_EXE_LINKER_FLAGS_INIT += \
	-L$(SP_MACOS_SDK)/usr/lib \
	-F$(SP_MACOS_SDK)/System/Library/Frameworks
CONFIGURE_SHARED_LINKER_FLAGS_INIT += \
	-L$(SP_MACOS_SDK)/usr/lib \
	-F$(SP_MACOS_SDK)/System/Library/Frameworks
endif # Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
CONFIGURE_CMAKE += -DCMAKE_OSX_SYSROOT=$(SP_IOSSIM_SDK) \
	-DCMAKE_FRAMEWORK_PATH="$(SP_IOSSIM_SDK)/System/Library/Frameworks"
CONFIGURE_EXE_LINKER_FLAGS_INIT += \
	-L$(SP_IOSSIM_SDK)/usr/lib \
	-F$(SP_IOSSIM_SDK)/System/Library/Frameworks
CONFIGURE_SHARED_LINKER_FLAGS_INIT += \
	-L$(SP_IOSSIM_SDK)/usr/lib \
	-F$(SP_IOSSIM_SDK)/System/Library/Frameworks
else # SP_IOSSIM
CONFIGURE_CMAKE += -DCMAKE_OSX_SYSROOT=$(SP_IOS_SDK) \
	-DCMAKE_FRAMEWORK_PATH="$(SP_IOS_SDK)/System/Library/Frameworks"
CONFIGURE_EXE_LINKER_FLAGS_INIT += \
	-L$(SP_IOS_SDK)/usr/lib \
	-F$(SP_IOS_SDK)/System/Library/Frameworks
CONFIGURE_SHARED_LINKER_FLAGS_INIT += \
	-L$(SP_IOS_SDK)/usr/lib \
	-F$(SP_IOS_SDK)/System/Library/Frameworks
endif # SP_IOSSIM
endif # iOS

endif # DARWIN

CONFIGURE_CMAKE += \
	-DSP_C_FLAGS="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DSP_CXX_FLAGS="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT)" \
	-DSP_EXE_LINKER_FLAGS="$(CONFIGURE_EXE_LINKER_FLAGS_INIT)" \
	-DSP_SHARED_LINKER_FLAGS="$(CONFIGURE_SHARED_LINKER_FLAGS_INIT)" \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_INSTALL_BINDIR=$(MAKE_ROOT)$(LIBNAME)/bin \
	-DCMAKE_INSTALL_DATAROOTDIR=$(MAKE_ROOT)$(LIBNAME)/share \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_SYSTEM_PROCESSOR=$(CONFIGURE_PROC_$(SP_ARCH)) \
	-DCMAKE_POSITION_INDEPENDENT_CODE=On \
	-DCMAKE_VERBOSE_MAKEFILE=On

ifdef WINDOWS
CONFIGURE_CMAKE += -DCMAKE_RC_COMPILER=$(SP_RC) -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
endif

ifeq ($(DEBUG),1)
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Debug
else
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Release
endif
