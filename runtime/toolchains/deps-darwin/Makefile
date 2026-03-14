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
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

TOOLCHAIN_OUTPUT_DIR ?= $(realpath $(MAKE_ROOT)/../intermediate)

MKDIR = mkdir -p

SP_MACOS_VER := 14.4
SP_IOS_VER := 17.4

INCLUDES := \
	simde/simde-arch.h \
	spirv/unified1/spirv.h \
	vulkan/vulkan.h

LIBS := \
	libc++.a \
	libz.a \
	libbz2.a \
	liblzma.a \
	libzstd.a \
	libbrotlienc.a \
	libjpeg.a \
	libpng16.a \
	libgif.a \
	libwebp.a \
	libtiff.a \
	libfreetype.a \
	libharfbuzz.a \
	libsqlite3.a \
	libnghttp3.a \
	libssl.a \
	libgost_engine.a \
	libmbedtls.a \
	libcurl-openssl.a \
	libcurl-mbedtls.a \
	libzip.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a \
	libvulkan.dylib \
	libMoltenVK.dylib

ifndef SP_ARCH

ARCHS := \
	x86_64-apple-macosx \
	aarch64-apple-macosx \
	aarch64-apple-ios \
	x86_64-apple-ios-simulator \
	aarch64-apple-ios-simulator

TARGET_FILE := $(dir $(THIS_FILE))target.mk

all: $(ARCHS)

SP_x86_64_MACOSX_INFO := \
	SP_SYSNAME=Darwin \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=x86_64-apple-macosx \
	SP_ARCH=x86_64 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx

SP_aarch64_MACOSX_INFO := \
	SP_SYSNAME=Darwin \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=aarch64-apple-macosx \
	SP_ARCH=arm64 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx

SP_aarch64_IOS_INFO := \
	SP_SYSNAME=iOS \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=aarch64-apple-ios \
	SP_ARCH=arm64 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios

SP_x86_64_IOSSIM_INFO := \
	SP_SYSNAME=iOS \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=x86_64-apple-ios-simulator \
	SP_ARCH=x86_64 \
	SP_IOSSIM=1 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator

SP_aarch64_IOSSIM_INFO := \
	SP_SYSNAME=iOS \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=aarch64-apple-ios-simulator \
	SP_ARCH=arm64 \
	SP_IOSSIM=1 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator

x86_64-apple-macosx:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_MACOSX_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_MACOSX_INFO)

aarch64-apple-macosx:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_MACOSX_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_MACOSX_INFO)

aarch64-apple-ios:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_IOS_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_IOS_INFO)

x86_64-apple-ios-simulator:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_IOSSIM_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_IOSSIM_INFO)

aarch64-apple-ios-simulator:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_IOSSIM_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_IOSSIM_INFO)

.PHONY: all $(ARCHS)
.DEFAULT_GOAL := all

else

OUT_SYSROOT ?= $(TOOLCHAIN_OUTPUT_DIR)

SP_TOOLCHAIN_PREFIX ?= $(OUT_SYSROOT)
SP_INSTALL_PREFIX ?= $(OUT_SYSROOT)
SP_TOOLCHAIN_FILE ?= $(realpath $(OUT_SYSROOT)/toolchain.cmake)

SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/cc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/c++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/ar)
SP_LIBTOOL := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/llvm-libtool-darwin)
SP_RANLIB := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/llvm-ranlib)

SP_OPT := -O3 -fPIC -DPIC

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	DARWIN=1 \
	SP_ARCH=$(SP_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_TOOLCHAIN_FILE) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_LIBTOOL=$(SP_LIBTOOL) \
	SP_RANLIB=$(SP_RANLIB) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT) \
	SP_MACOS_VER=$(SP_MACOS_VER) \
	SP_IOS_VER=$(SP_IOS_VER)

ifdef SP_TOOLCHAIN_PREFIX
FORWARD_VARS += SP_TOOLCHAIN_PREFIX=$(SP_TOOLCHAIN_PREFIX)
endif

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(SP_INSTALL_PREFIX)/share/licenses

$(SP_INSTALL_PREFIX)/usr/lib/libc++.a: $(MAKE_ROOT)libc++.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libz.a: ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbz2.a: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/liblzma.a: ../common/xz.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a:  ../common/zstd.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a: ../common/brotli.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a: ../common/libjpeg-turbo.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libpng16.a: ../common/libpng.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgif.a: ../common/gif.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libwebp.a: ../common/libwebp.mk $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a $(SP_INSTALL_PREFIX)/usr/lib/libgif.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libtiff.a: ../common/tiff.mk $(SP_INSTALL_PREFIX)/usr/lib/libwebp.a $(SP_INSTALL_PREFIX)/usr/lib/liblzma.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a: ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libbz2.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libharfbuzz.a:../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libsqlite3.a: ../common/sqlite.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a: ../common/mbedtls.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a:  ../common/nghttp3.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libssl.a: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a:  ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-mbedtls.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/libzip.a: ../common/libzip.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/usr/lib/libiwasm-debug.a:  ../common/wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libvulkan.dylib: $(MAKE_ROOT)vulkan-loader.mk $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libMoltenVk.dylib: $(MAKE_ROOT)moltenvk.mk $(SP_INSTALL_PREFIX)/usr/lib/libvulkan.dylib
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h: ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h

$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h: ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h

$(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h

$(SP_INSTALL_PREFIX)/share/licenses:
	rm -rf $(SP_INSTALL_PREFIX)/share/licenses

.PHONY: all

endif
