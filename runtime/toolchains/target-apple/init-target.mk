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

THIS_FILE := $(lastword $(MAKEFILE_LIST))

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/find-recursive.mk
include $(dir $(THIS_FILE))../common/utils/llvm-version.mk

ifeq ($(UNAME),Darwin)
SP_MACOS_SDK ?= $(shell xcrun --show-sdk-path)
SP_IOS_SDK ?= $(shell xcrun --sdk iphoneos --show-sdk-path)
SP_IOSSIM_SDK ?= $(shell xcrun --sdk iphonesimulator --show-sdk-path)
else
SP_MACOS_SDK ?= $(abspath $(dir $(THIS_FILE))../src)/MacOSX.sdk
SP_IOS_SDK ?= $(abspath $(dir $(THIS_FILE))../src)/iPhoneOS.sdk
SP_IOSSIM_SDK ?= $(abspath $(dir $(THIS_FILE))../src)/iPhoneSimulator.sdk
endif

# Apple's <sys/cdefs.h> keys the modern (no legacy "$UNIX2003"/"$INODE64" symbol
# suffix) ABI off #ifdef XNU_PLATFORM_<platform>, a macro the real Apple clang
# driver injects but our cross clang does not — so define it explicitly per target.
# Without it a "+open" link fails on _fopen$UNIX2003 & co. (absent from the minimal
# .tbd stubs); it flows into TARGET_GENERAL_C{,XX}FLAGS in the generated target.mk.
ifeq ($(SP_SYSNAME),Darwin)
SP_SDK_ROOT := $(SP_MACOS_SDK)
SP_DEPFLAGS := -mmacosx-version-min=$(SP_OSVER)
SP_SDK_NAME := macosx
SP_SDK_FALLBACK := MacOSX.sdk
endif # Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
SP_SDK_NAME := iphonesimulator
SP_SDK_ROOT := $(SP_IOSSIM_SDK)
SP_SDK_FALLBACK := iPhoneSimulator.sdk
SP_DEPFLAGS := -mios-simulator-version-min=$(SP_OSVER)
else # SP_IOSSIM
SP_SDK_NAME := iphoneos
SP_SDK_ROOT := $(SP_IOS_SDK)
SP_SDK_FALLBACK := iPhoneOS.sdk
SP_DEPFLAGS := -mios-version-min=$(SP_OSVER)
endif # SP_IOSSIM
endif

# No -DXNU_PLATFORM_<platform> anywhere: the +open sysroot bakes the macro into
# <sys/cdefs.h> at assembly time (open-sysroot.mk. Without it Apple's <sys/cdefs.h>
# would rename open/chmod/fopen &c. to their legacy $UNIX2003 variants on x86_64.
#
# +open is Xcode-SDK-free: its toolchain.cmake -isysroot/-L/-F point at the +open sysroot
# itself (== CMAKE_CURRENT_LIST_DIR, already CMAKE_SYSROOT), NOT MacOSX.sdk. This is now
# safe because everything that consumes toolchain.cmake builds SDK-free: libcxx.mk brings
# its OWN libunwind (_Unwind_*) + copyfile.h + the curated libSystem symbols, and the deps
# resolve against the sysroot.
ifneq (,$(findstring +open,$(TOOLCHAIN_OUTPUT_DIR)))
TOOLCHAIN_SDK_ROOT := $${CMAKE_CURRENT_LIST_DIR}
# +open keeps the SDK-like headers (apple-oss + overlay + libc++/libunwind) in
# include_libc, mirroring the Linux targets; usr/include holds only the deps' own
# headers. Every toolchain.cmake consumer must therefore search include_libc.
TOOLCHAIN_ISYSTEM := -isystem $${CMAKE_CURRENT_LIST_DIR}/include_libc
else
TOOLCHAIN_SDK_ROOT := $(SP_SDK_ROOT)
TOOLCHAIN_ISYSTEM :=
endif
TOOLCHAIN_CFLAGS := $(TOOLCHAIN_ISYSTEM) -isysroot $(TOOLCHAIN_SDK_ROOT) -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_TARGET) -arch $(SP_ARCH)
TOOLCHAIN_LDFLAGS := -L$(TOOLCHAIN_SDK_ROOT)/usr/lib -F$(TOOLCHAIN_SDK_ROOT)/System/Library/Frameworks

$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake: $(THIS_FILE)
	@echo Build $@
	@echo 'set(CMAKE_SYSTEM_NAME $(SP_SYSNAME))' > $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_OSX_DEPLOYMENT_TARGET "$(SP_OSVER)")' >> $@
	@echo 'set(CMAKE_OSX_ARCHITECTURES "$(SP_ARCH)")' >> $@
	@echo 'set(CMAKE_MACOSX_BUNDLE OFF)' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_OBJC_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_OBJCXX_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_OBJC_FLAGS_INIT "-ObjC $${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_OBJCXX_FLAGS_INIT "-ObjC++ $${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS} $(TOOLCHAIN_LDFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS} $(TOOLCHAIN_LDFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_OBJC_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_OBJCXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@# Cross-linking Mach-O from a non-Darwin host needs ld64.lld (the system ld
	@# cannot emit Mach-O); on a native macOS build keep the default ld64. Required
	@# to link dylibs (e.g. cross-building libc++/libc++abi into a +open sysroot).
	@[ "$(UNAME)" != "Darwin" ] && echo 'set(CMAKE_LINKER_TYPE LLD)' >> $@ || true
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
	rm -f $(TOOLCHAIN_OUTPUT_DIR)/host
	cd $(TOOLCHAIN_OUTPUT_DIR); ln -fs ../../../hosts/$(HOST_ID) host
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/$(SP_LLVM_VER)/include include

$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := $(SP_SYSNAME)' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(SP_TARGET)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@$(if $(findstring +open,$(TOOLCHAIN_OUTPUT_DIR)),echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/include_libc' >> $@,true)
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -arch $(SP_ARCH) -resource-dir $$(TARGET_SYSROOT)/lib/clang $(SP_DEPFLAGS)' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -arch $(SP_ARCH) -resource-dir $$(TARGET_SYSROOT)/lib/clang $(SP_DEPFLAGS)' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -arch $(SP_ARCH) -resource-dir $$(TARGET_SYSROOT)/lib/clang -L$$(TARGET_LIB_DIR) -nostdlib++ -nostdlib $(SP_DEPFLAGS)' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@
	@echo 'TARGET_OSVER := $(SP_OSVER)' >> $@
	@echo 'TARGET_SDK_NAME := $(SP_SDK_NAME)' >> $@
	@# The fallback SDK is not shipped inside the target sysroot; it always lives in
	@# <repo>/runtime/toolchains/src. Anchor to GLOBAL_ROOT (the repo root, set by the
	@# build system before this file is included) so it resolves regardless of where the
	@# target.mk itself is loaded from. Covers both toolchains layouts.
	@echo 'TARGET_SDK_FALLBACK := $$(firstword $$(wildcard $$(GLOBAL_ROOT)/runtime/toolchains/src/$(SP_SDK_FALLBACK) $$(GLOBAL_ROOT)/toolchains/src/$(SP_SDK_FALLBACK)))' >> $@

CSU_DIR := $(dir $(THIS_FILE))csu

$(CSU_DIR):
	git clone https://github.com/apple-oss-distributions/Csu.git --branch Csu-88 $(CSU_DIR)

CC := $(TOOLCHAIN_OUTPUT_DIR)/host/bin/clang

ifeq ($(UNAME),Darwin)
LD := ld
else
LD := /usr/bin/x86_64-apple-darwin-ld
endif

# CSU (crt1.o) is compiled against the target sysroot. For +open that's the +open sysroot
# itself (SDK-free) rather than MacOSX.sdk — but its usr/include only exists AFTER
# open-sysroot.mk runs, so the +open flow builds crt1 as a SEPARATE `crt1` goal invoked
# after open-sysroot (see target-apple/Makefile). The default `all` still bundles crt1 for
# the SDK-based stock macosx/iOS targets, whose sysroot IS the SDK.
ifneq (,$(findstring +open,$(TOOLCHAIN_OUTPUT_DIR)))
CSU_SDK_ROOT := $(TOOLCHAIN_OUTPUT_DIR)
else
CSU_SDK_ROOT := $(SP_SDK_ROOT)
endif

CFLAGS := --target=$(SP_TARGET) -arch $(SP_ARCH) -isysroot $(CSU_SDK_ROOT) -fuse-ld=$(LD)
# +open sysroot headers live in include_libc, not <sysroot>/usr/include.
ifneq (,$(findstring +open,$(TOOLCHAIN_OUTPUT_DIR)))
CFLAGS += -isystem $(TOOLCHAIN_OUTPUT_DIR)/include_libc
endif

$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/crt1.o: | $(CSU_DIR) $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake
	$(MAKE) -C $(CSU_DIR) CC="$(CC)" ARCH_CFLAGS="$(CFLAGS)" DSTROOT="$(TOOLCHAIN_OUTPUT_DIR)" clean
	$(MAKE) -C $(CSU_DIR) CC="$(CC)" ARCH_CFLAGS="$(CFLAGS)" DSTROOT="$(TOOLCHAIN_OUTPUT_DIR)" install
	$(MAKE) -C $(CSU_DIR) CC="$(CC)" ARCH_CFLAGS="$(CFLAGS)" DSTROOT="$(TOOLCHAIN_OUTPUT_DIR)" clean

# bootstrap = toolchain.cmake + target.mk + the CSU checkout, WITHOUT crt1 (so the +open
# flow can populate the sysroot in between); crt1 = the crt objects; all = both (stock/iOS).
bootstrap: $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake $(TOOLCHAIN_OUTPUT_DIR)/target.mk $(CSU_DIR)

crt1: $(TOOLCHAIN_OUTPUT_DIR)/usr/lib/crt1.o

all: bootstrap crt1

.PHONY: all bootstrap crt1
.DEFAULT_GOAL := all
