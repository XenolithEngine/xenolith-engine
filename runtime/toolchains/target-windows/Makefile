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

.DEFAULT_GOAL := all

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

TARGET_FILE := $(MAKE_ROOT)init-target.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk

TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

SP_OPT := -O3

LIBS := \
	z.lib \
	bz2.lib \
	lzma.lib \
	zstd.lib \
	brotlienc.lib \
	jpeg.lib \
	png16.lib \
	gif.lib \
	webp.lib \
	tiff.lib \
	freetype.lib \
	harfbuzz.lib \
	sqlite3.lib \
	nghttp3.lib \
	ssl.lib \
	gost_engine.lib \
	curl-openssl.lib \
	zip.lib

INCLUDES := \
	spirv/unified1/spirv.h \
	vulkan/vulkan.h \
	simde/simde-arch.h

ifndef SP_ARCH

ARCHS := x86_64

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

SP_ARCH_COMMON := \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..)

SP_ARCH_x86_64_INFO_BASE := \
	$(SP_ARCH_COMMON) \
	SP_ARCH=x86_64 \
	SP_RES_ARCH=x86_64 \
	SP_ARCH_TARGET_CLANG=x86_64-pc-windows-msvc

SP_ARCH_x86_64_INFO := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SP_TARGET=x86_64-pc-windows-msvc \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc

SP_ARCH_x86_64_INSTALL := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc \
	T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc

SP_ARCH_aarch64_INFO_BASE := \
	$(SP_ARCH_COMMON) \
	SP_ARCH=aarch64 \
	SP_RES_ARCH=aarch64 \
	SP_ARCH_TARGET_CLANG=aarch64-pc-windows-msvc

SP_ARCH_aarch64_INFO := \
	$(SP_ARCH_arm64_INFO_BASE) \
	SP_TARGET=aarch64-pc-windows-msvc \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc

SP_ARCH_aarch64_INSTALL := \
	$(SP_ARCH_arm64_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc \
	T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-pc-windows-msvc

all: $(ARCHS)

clean:

x86_64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_x86_64_INSTALL)

aarch64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_aarch64_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_aarch64_INSTALL)

.PHONY: all x86_64 aarch64

else # SP_ARCH

OUT_SYSROOT ?= $(realpath ../$(SP_TARGET))

SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)

SP_CC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/cc)
SP_CXX := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/c++)
SP_AR := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/ar)
SP_RC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-rc)

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	WINDOWS=1 \
	SP_SYSNAME=Windows \
	SP_ARCH=$(SP_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	SP_TOOLCHAIN_PREFIX=$(TOOLCHAIN_OUTPUT_DIR) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_RC=$(SP_RC) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)


ARCH_LIBS = \
	$(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

ARCH_BINS  = $(addprefix $(SP_INSTALL_PREFIX)/bin/,$(BINS))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(ARCH_BINS) $(SP_INSTALL_PREFIX)/share/licenses

$(SP_INSTALL_PREFIX)/usr/lib/z.lib:  ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/bz2.lib: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/lzma.lib:  ../common/xz.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/zstd.lib:  ../common/zstd.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/brotlienc.lib:  ../common/brotli.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/jpeg.lib:  ../common/libjpeg-turbo.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/png16.lib:  ../common/libpng.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/gif.lib:  ../common/gif.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/webp.lib:  ../common/libwebp.mk $(SP_INSTALL_PREFIX)/usr/lib/png16.lib $(SP_INSTALL_PREFIX)/usr/lib/jpeg.lib $(SP_INSTALL_PREFIX)/usr/lib/gif.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/tiff.lib:  ../common/tiff.mk $(SP_INSTALL_PREFIX)/usr/lib/webp.lib $(SP_INSTALL_PREFIX)/usr/lib/lzma.lib $(SP_INSTALL_PREFIX)/usr/lib/zstd.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/freetype.lib:  ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/brotlienc.lib $(SP_INSTALL_PREFIX)/usr/lib/png16.lib $(SP_INSTALL_PREFIX)/usr/lib/bz2.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/harfbuzz.lib:  ../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/freetype.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/sqlite3.lib: ../common/sqlite.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/nghttp3.lib: ../common/nghttp3.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/ssl.lib: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $(MAKE_ROOT)openssl.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/gost_engine.lib: ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/ssl.lib
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/curl-openssl.lib: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/ssl.lib
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/curl-mbedtls.lib: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/usr/lib/mbedtls.lib
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/zip.lib: ../common/libzip.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/iwasm-release.lib $(SP_INSTALL_PREFIX)/usr/lib/iwasm-debug.lib: $(MAKE_ROOT)wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/z.lib
	$(MAKE) -f $(MAKE_ROOT)wamr.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h: ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	$(call rule_touch,$(SP_INSTALL_PREFIX)/usr//include/vulkan/vulkan.h)

$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h: ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	$(call rule_touch,$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h)

$(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	$(call rule_touch,$(SP_INSTALL_PREFIX)/usr//include/simde/simde-arch.h)

$(SP_INSTALL_PREFIX)/share/licenses:
	$(call rule_rm,$(SP_INSTALL_PREFIX)/share/licenses)
	$(call rule_cpr,../licenses,$(SP_INSTALL_PREFIX)/share/licenses)

.PHONY: all

endif
