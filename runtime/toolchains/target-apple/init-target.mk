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

ifeq ($(UNAME),Darwin)
SP_MACOS_SDK ?= $(shell xcrun --show-sdk-path)
SP_IOS_SDK ?= $(shell xcrun --sdk iphoneos --show-sdk-path)
SP_IOSSIM_SDK ?= $(shell xcrun --sdk iphonesimulator --show-sdk-path)
else
SP_MACOS_SDK ?= $(abspath $(dir $(THIS_FILE))../src)/MacOSX.sdk
SP_IOS_SDK ?= $(abspath $(dir $(THIS_FILE))../src)/iPhoneOS.sdk
SP_IOSSIM_SDK ?= $(abspath $(dir $(THIS_FILE))../src)/iPhoneSimulator.sdk
endif

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
else # SP_IOSSIM
SP_SDK_NAME := iphoneos
SP_SDK_ROOT := $(SP_IOS_SDK)
SP_SDK_FALLBACK := iPhoneOS.sdk
endif # SP_IOSSIM
SP_DEPFLAGS := -mios-version-min=$(SP_OSVER)
endif

TOOLCHAIN_CFLAGS :=  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_TARGET) -arch $(SP_ARCH)

$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake: $(THIS_FILE)
	@echo Build $@
	@echo 'set(CMAKE_SYSTEM_NAME $(SP_SYSNAME))' > $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_OSX_DEPLOYMENT_TARGET "$(SP_OSVER)")' >> $@
	@echo 'set(CMAKE_OSX_ARCHITECTURES "$(SP_ARCH)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_OBJC_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_OBJCXX_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_TARGET)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_OBJC_FLAGS_INIT "-ObjC $${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_OBJCXX_FLAGS_INIT "-ObjC++ $${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_OBJC_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_OBJCXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
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
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/21/include include

$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := $(SP_SYSNAME)' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(SP_TARGET)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
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
	@echo 'TARGET_SDK_FALLBACK := $$(realpath $$(TARGET_SYSROOT)/../../src/$(SP_SDK_FALLBACK))' >> $@

CSU_DIR := $(dir $(THIS_FILE))csu

$(CSU_DIR):
	git clone https://github.com/apple-oss-distributions/Csu.git --branch Csu-88 $(CSU_DIR)

CC := $(TOOLCHAIN_OUTPUT_DIR)/host/bin/clang

ifeq ($(UNAME),Darwin)
LD := ld
else
LD := /usr/bin/x86_64-apple-darwin-ld
endif

CFLAGS := --target=$(SP_TARGET) -arch $(SP_ARCH) -isysroot $(SP_SDK_ROOT) -fuse-ld=$(LD)

$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/crt1.o: | $(CSU_DIR) $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake
	$(MAKE) -C $(CSU_DIR) CC="$(CC)" ARCH_CFLAGS="$(CFLAGS)" DSTROOT="$(TOOLCHAIN_OUTPUT_DIR)" clean
	$(MAKE) -C $(CSU_DIR) CC="$(CC)" ARCH_CFLAGS="$(CFLAGS)" DSTROOT="$(TOOLCHAIN_OUTPUT_DIR)" install
	$(MAKE) -C $(CSU_DIR) CC="$(CC)" ARCH_CFLAGS="$(CFLAGS)" DSTROOT="$(TOOLCHAIN_OUTPUT_DIR)" clean

all: $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake $(TOOLCHAIN_OUTPUT_DIR)/target.mk \
	$(CSU_DIR) $(TOOLCHAIN_OUTPUT_DIR)/usr/lib/crt1.o

.PHONY: all
.DEFAULT_GOAL := all
