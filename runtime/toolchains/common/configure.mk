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
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
SP_MACOS_SDK ?= $(shell xcrun --show-sdk-path)
SP_IOS_SDK ?= $(shell xcrun --sdk iphoneos --show-sdk-path)
SP_IOSSIM_SDK ?= $(shell xcrun --sdk iphonesimulator --show-sdk-path)
else
SP_MACOS_SDK ?= $(abspath $(LIB_SRC_DIR))/MacOSX.sdk
SP_IOS_SDK ?= $(abspath $(LIB_SRC_DIR))/iPhoneOS.sdk
SP_IOSSIM_SDK ?= $(abspath $(LIB_SRC_DIR))/iPhoneSimulator.sdk
endif
ifneq (,$(findstring +open,$(SP_INSTALL_PREFIX)))
SP_MACOS_SDK := $(SP_INSTALL_PREFIX)
endif
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
SP_WINDOWS_INCLIUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include \
	-isystem $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows \
	-isystem $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows/casemap

SP_CFLAGS += -Xclang --dependent-lib=sprt -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
SP_CXXFLAGS += -Xclang --dependent-lib=sprt -nostdlib  $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
endif # WINDOWS


ifdef WASM
# Freestanding WebAssembly. Like the Windows target, the sprt runtime IS the
# libc — at toolchain-build time only its headers are needed (the archive links
# per-app), so point the include search at the in-tree runtime headers and force
# the wasm platform (__SPRT_WASM) + the wasm feature set the app build uses. Kept
# in sync with the app target.mk (make/os/wasm.mk + target-wasm/init-target.mk).
SP_WASM_FEATURES := -matomics -mbulk-memory -mmutable-globals -msign-ext -mnontrapping-fptoint
SP_WASM_C_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include
SP_WASM_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include

# clang does not auto-add its builtin/resource include dir for the wasm32 freestanding
# target, so its builtin headers (<stdatomic.h>, <stdarg.h>, intrinsics) do not resolve.
# Add the host clang resource include (the -resource-dir's include/, symlinked to the
# host clang's) at LOWEST priority (-idirafter) so it fills only the clang-builtin gaps
# without shadowing the sprt libc headers.
SP_WASM_RESOURCE_INC := -idirafter $(SP_INSTALL_PREFIX)/lib/clang/include

# C++ deps (harfbuzz, ...) parse the sprt STL headers, which require C++20 (concepts,
# etc.). cmake's CXX_STANDARD does not emit a -std flag here because the toolchain
# skips compiler detection, so pin it in the flags (matches the app's GLOBAL_STDXX).
# C++ deps are compiled HOSTED (no -ffreestanding, so __STDC_HOSTED__==1), matching
# the LLVM C++ runtimes build: the sprt STL then exposes the C library under std::
# and its __STDC_HOSTED__-gated bridges activate (e.g. <cmath> -> std::isfinite,
# std::isnan), which third-party C++ (harfbuzz) relies on. C deps stay freestanding.
SP_CFLAGS += -nostdinc -ffreestanding -nostdlib $(SP_WASM_FEATURES) $(SP_WASM_C_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
SP_CXXFLAGS += -nostdinc -nostdinc++ -nostdlib -std=gnu++2a $(SP_WASM_FEATURES) $(SP_WASM_CXX_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
SP_CPPFLAGS += -nostdinc $(SP_WASM_FEATURES) $(SP_WASM_C_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
SP_LDFLAGS += -nostdlib $(SP_WASM_FEATURES)
endif # WASM


ifdef DARWIN

# Apple's modern libc ABI in <sys/cdefs.h> (no legacy $UNIX2003 / $INODE64
# suffixes) needs no -DXNU_PLATFORM_<platform> here: the +open sysroot bakes the
# macro into <sys/cdefs.h> at assembly time (open-sysroot.mk, mirroring how
# Apple resolves those branches with unifdef when generating the SDK), and the
# real SDKs ship the header already resolved.

ifeq ($(SP_SYSNAME),Darwin)
SP_SDK_DIR := $(SP_MACOS_SDK)
SP_CFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -isysroot $(SP_MACOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -isysroot $(SP_MACOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_MACOS_SDK)/usr/lib -F$(SP_MACOS_SDK)/System/Library/Frameworks
endif # SP_SYSNAME Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
SP_SDK_DIR := $(SP_IOSSIM_SDK)
SP_CFLAGS += -mios-simulator-version-min=$(SP_IOS_VER) -isysroot $(SP_IOSSIM_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mios-simulator-version-min=$(SP_IOS_VER) -isysroot $(SP_IOSSIM_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mios-simulator-version-min=$(SP_IOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_IOSSIM_SDK)/usr/lib -F$(SP_IOSSIM_SDK)/System/Library/Frameworks
else # SP_IOSSIM
SP_SDK_DIR := $(SP_IOS_SDK)
SP_CFLAGS += -mios-version-min=$(SP_IOS_VER) -isysroot $(SP_IOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mios-version-min=$(SP_IOS_VER) -isysroot $(SP_IOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mios-version-min=$(SP_IOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_IOS_SDK)/usr/lib -F$(SP_IOS_SDK)/System/Library/Frameworks
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

ifdef WASM
CONFIGURE_CMAKE_C_FLAGS_INIT += -nostdinc -ffreestanding $(SP_WASM_FEATURES) $(SP_WASM_C_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -nostdinc -nostdinc++ -std=gnu++2a $(SP_WASM_FEATURES) $(SP_WASM_CXX_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
CONFIGURE_EXE_LINKER_FLAGS_INIT += -nostdlib -Wl,--no-entry -Wl,--export-if-defined=main \
	-L$(SP_INSTALL_PREFIX)/usr/lib -lsprt \
	$(SP_INSTALL_PREFIX)/lib/clang/lib/wasi/libclang_rt.builtins-wasm32.a
endif # WASM

ifdef LINUX
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

ifdef ANDROID
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

CONFIGURE_CMAKE :=

ifdef SP_TOOLCHAIN_FILE

CONFIGURE_CMAKE += \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE)) \
	-DCMAKE_PREFIX_PATH="${SP_INSTALL_PREFIX};${SP_INSTALL_PREFIX}/usr" \
	-DCMAKE_INSTALL_PREFIX="${SP_INSTALL_PREFIX}" \
	-DCMAKE_INSTALL_LIBDIR="${SP_INSTALL_PREFIX}/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="${SP_INSTALL_PREFIX}/usr/include" \
	-DPKG_CONFIG_PATH="$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig"

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


# +open header layout (Linux-target parity): the SDK-like headers — apple-oss libc,
# the baked overlay, and our libc++ (include_libc/c++/v1, built by libcxx.mk) — live
# in <sysroot>/include_libc; usr/include holds ONLY the third-party deps' own headers.
# Deps builds therefore search include_libc via -isystem, AFTER usr/include (a dep's
# own installed headers win first). C++ ordering: our c++/v1 must come FIRST (before
# usr/include and include_libc), or libc++'s <climits> -> `#include_next <limits.h>`
# would hit the C <limits.h> before libc++'s own and error. Detected by the "+open"
# marker in the install prefix (same idiom as make/os/darwin.mk). Stock SDK targets
# are unaffected. cmake deps get the include_libc -isystem from toolchain.cmake
# (emitted by init-target.mk), so only the c++/v1 prepend is mirrored there.
ifneq (,$(findstring +open,$(SP_INSTALL_PREFIX)))
SP_CXXFLAGS := -isystem $(SP_INSTALL_PREFIX)/include_libc/c++/v1 $(SP_CXXFLAGS)
CONFIGURE_CMAKE_CXX_FLAGS_INIT := -isystem $(SP_INSTALL_PREFIX)/include_libc/c++/v1 $(CONFIGURE_CMAKE_CXX_FLAGS_INIT)
SP_CFLAGS += -isystem $(SP_INSTALL_PREFIX)/include_libc
SP_CXXFLAGS += -isystem $(SP_INSTALL_PREFIX)/include_libc
SP_CPPFLAGS += -isystem $(SP_INSTALL_PREFIX)/include_libc
endif


ifdef WINDOWS
CONFIGURE_CMAKE_C_FLAGS_INIT += -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
CONFIGURE_CMAKE_RC_FLAGS_INIT += -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
CONFIGURE_EXE_LINKER_FLAGS_INIT += -nostdlib -L$(SP_INSTALL_PREFIX)/lib
CONFIGURE_SHARED_LINKER_FLAGS_INIT += -nostdlib -L$(SP_INSTALL_PREFIX)/lib
endif

CONFIGURE_CMAKE += \
	-DSP_C_FLAGS="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DSP_CXX_FLAGS="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT)" \
	-DSP_EXE_LINKER_FLAGS="$(CONFIGURE_EXE_LINKER_FLAGS_INIT)" \
	-DSP_SHARED_LINKER_FLAGS="$(CONFIGURE_SHARED_LINKER_FLAGS_INIT)" \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_INSTALL_BINDIR=$(MAKE_ROOT)$(LIBNAME)/bin \
	-DCMAKE_INSTALL_DATAROOTDIR=$(SP_INSTALL_PREFIX)/usr/share \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_SYSTEM_PROCESSOR=$(CONFIGURE_PROC_$(SP_ARCH)) \
	-DCMAKE_POSITION_INDEPENDENT_CODE=On \
	-DCMAKE_VERBOSE_MAKEFILE=On

ifdef WINDOWS
CONFIGURE_CMAKE += \
	-DCMAKE_RC_COMPILER=$(SP_RC) \
	-DCMAKE_RC_FLAGS_INIT="$(CONFIGURE_CMAKE_RC_FLAGS_INIT)" \
	-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
	-DENABLE_EXPORTS=Off
endif

ifdef WASM
# Force every C++ dependency to the C++20 the sprt STL requires — see
# target-wasm/wasm-deps-project-include.cmake for why the toolchain file cannot do it.
CONFIGURE_CMAKE += -DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)wasm-deps-project-include.cmake
endif

ifeq ($(DEBUG),1)
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Debug
else
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Release
endif
