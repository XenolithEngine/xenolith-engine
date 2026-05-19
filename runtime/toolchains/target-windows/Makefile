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

TARGET_FILE := $(MAKE_ROOT)init-target.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk

TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)

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
#	libwebp.lib \
	libtiff.lib \
	libfreetype.lib \
	libharfbuzz.lib \
	libsqlite3.lib \
	libmbedtls.lib \
	libnghttp3.lib \
	libssl.lib \
	libgost_engine.lib \
	libcurl-openssl.lib \
	libcurl-mbedtls.lib \
	libzip.lib \
	libiwasm-release.lib \
	libiwasm-debug.lib \

INCLUDES := \
#	spirv/unified1/spirv.h \
	vulkan/vulkan.h \
	simde/simde-arch.h

ifndef SP_ARCH

ARCHS := x86_64

SP_ARCH_x86_64_INFO_BASE := \
	SP_ARCH=x86_64 \
	SP_RES_ARCH=x86_64 \
	SP_ARCH_TARGET_CLANG=x86_64-pc-windows-msvc

SP_ARCH_x86_64_INFO := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SP_TARGET=x86_64-pc-windows-msvc \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc

SP_ARCH_aarch64_INFO_BASE := \
	SP_ARCH=aarch64 \
	SP_RES_ARCH=aarch64 \
	SP_ARCH_TARGET_CLANG=aarch64-pc-windows-msvc

SP_ARCH_aarch64_INFO := \
	$(SP_ARCH_arm64_INFO_BASE) \
	SP_TARGET=aarch64-pc-windows-msvc \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc

all: $(ARCHS)

clean:

x86_64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_INFO)

aarch64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_aarch64_INFO)

.PHONY: all x86_64 aarch64

else # SP_ARCH

OUT_SYSROOT ?= $(realpath ../$(SP_TARGET))

SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)

SP_CC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/cc)
SP_CXX := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/c++)
SP_AR := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/ar)

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
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/include/,$(INCLUDES))

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

$(SP_INSTALL_PREFIX)/lib/webp.lib: $(MAKE_ROOT)libwebp.mk $(SP_INSTALL_PREFIX)/lib/png16.lib $(SP_INSTALL_PREFIX)/lib/jpeg.lib $(SP_INSTALL_PREFIX)/lib/gif.lib
	$(MAKE) -f $(MAKE_ROOT)libwebp.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/tiff.lib: $(MAKE_ROOT)tiff.mk $(SP_INSTALL_PREFIX)/lib/webp.lib $(SP_INSTALL_PREFIX)/lib/lzma.lib $(SP_INSTALL_PREFIX)/lib/zstd.lib
	$(MAKE) -f $(MAKE_ROOT)tiff.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/freetype.lib: $(MAKE_ROOT)freetype.mk $(SP_INSTALL_PREFIX)/lib/brotlienc.lib $(SP_INSTALL_PREFIX)/lib/png16.lib $(SP_INSTALL_PREFIX)/lib/bz2.lib
	$(MAKE) -f $(MAKE_ROOT)freetype.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/harfbuzz.lib: $(MAKE_ROOT)harfbuzz.mk $(SP_INSTALL_PREFIX)/lib/freetype.lib
	$(MAKE) -f $(MAKE_ROOT)harfbuzz.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/sqlite3.lib: $(MAKE_ROOT)sqlite.mk
	$(MAKE) -f $(MAKE_ROOT)sqlite.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/mbedtls.lib: $(MAKE_ROOT)mbedtls.mk
	$(MAKE) -f $(MAKE_ROOT)mbedtls.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/nghttp3.lib: $(MAKE_ROOT)nghttp3.mk
	$(MAKE) -f $(MAKE_ROOT)nghttp3.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/ssl.lib: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $(MAKE_ROOT)openssl.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/gost_engine.lib: $(MAKE_ROOT)gost.mk $(SP_INSTALL_PREFIX)/lib/ssl.lib
	$(MAKE) -f $(MAKE_ROOT)gost.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/curl-openssl.lib: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/ssl.lib
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/lib/curl-mbedtls.lib: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/mbedtls.lib
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/lib/zip.lib: $(MAKE_ROOT)libzip.mk
	$(MAKE) -f $(MAKE_ROOT)libzip.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/iwasm-release.lib $(SP_INSTALL_PREFIX)/lib/iwasm-debug.lib: $(MAKE_ROOT)wamr.mk $(SP_INSTALL_PREFIX)/lib/z.lib
	$(MAKE) -f $(MAKE_ROOT)wamr.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/include/vulkan/vulkan.h: $(MAKE_ROOT)vulkan-headers.mk
	$(MAKE) -f $(MAKE_ROOT)vulkan-headers.mk $(FORWARD_VARS)
	(Get-Item "$(SP_INSTALL_PREFIX)/include/vulkan/vulkan.h").LastWriteTime = $$(Get-Date);

$(SP_INSTALL_PREFIX)/include/spirv/unified1/spirv.h: $(MAKE_ROOT)spirv-headers.mk
	$(MAKE) -f $(MAKE_ROOT)spirv-headers.mk $(FORWARD_VARS)
	(Get-Item "$(SP_INSTALL_PREFIX)/include/spirv/unified1/spirv.h").LastWriteTime = $$(Get-Date);

$(SP_INSTALL_PREFIX)/include/simde/simde-arch.h: $(MAKE_ROOT)simde.mk
	$(MAKE) -f $(MAKE_ROOT)simde.mk $(FORWARD_VARS)
	(Get-Item "$(SP_INSTALL_PREFIX)/include/simde/simde-arch.h").LastWriteTime = $$(Get-Date);

$(SP_INSTALL_PREFIX)/bin/spirv-opt.exe: $(MAKE_ROOT)spirv-tools.mk $(SP_INSTALL_PREFIX)/include/spirv/unified1/spirv.h
	$(MAKE) -f $(MAKE_ROOT)spirv-tools.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/bin/glslang.exe: $(MAKE_ROOT)glslang.mk $(SP_INSTALL_PREFIX)/bin/spirv-opt.exe $(SP_INSTALL_PREFIX)/include/vulkan/vulkan.h
	$(MAKE) -f $(MAKE_ROOT)glslang.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/share/licenses:
	$(call rule_rm,$(SP_INSTALL_PREFIX)/share/licenses)
	$(call rule_cpr,../licenses,$(SP_INSTALL_PREFIX)/share/licenses)

.PHONY: all

endif
