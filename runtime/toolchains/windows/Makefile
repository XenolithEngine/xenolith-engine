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

SHELL = powershell.exe

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
	mbedtls.lib \
	nghttp3.lib \
	ssl.lib \
	gost_engine.lib \
	curl-openssl.lib \
	curl-mbedtls.lib \
	zip.lib \
	iwasm-release.lib \
	iwasm-debug.lib \

WASM_LIBS := \
	libiwasm-release.a \
	libiwasm-debug.a

BINS := \
	spirv-opt.exe \
	glslang.exe

INCLUDES := \
	spirv/unified1/spirv.h \
	vulkan/vulkan.h \
	simde/simde-arch.h

ifndef ARCH

ARCHS := x86_64

all: $(ARCHS)

clean:

x86_64:
	$(MAKE) -f $(THIS_FILE) ARCH=x86_64 SP_TARGET=x86_64-pc-windows-msvc OUT_SYSROOT=$(OUT_SYSROOT)

aarch64:
	$(MAKE) -f $(THIS_FILE) ARCH=aarch64 SP_TARGET=aarch64-pc-windows-msvc OUT_SYSROOT=$(OUT_SYSROOT)

.PHONY: all x86_64 aarch64

else # ARCH

ifeq ($(OUT_SYSROOT),)
override OUT_SYSROOT := $(realpath ../$(SP_TARGET))

$(info OUT_SYSROOT $(OUT_SYSROOT) $(realpath ../$(SP_TARGET)) $(SP_TARGET))
endif


SP_TOOLCHAIN_PREFIX ?= $(OUT_SYSROOT)
SP_INSTALL_PREFIX ?= $(OUT_SYSROOT)

SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/clang.exe)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/clang.exe)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/llvm-ar.exe)
SP_RC := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/llvm-rc.exe)

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	WINDOWS=1 \
	ARCH=$(ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	SP_RC="$(SP_RC)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ifdef SP_TOOLCHAIN_PREFIX
FORWARD_VARS += SP_TOOLCHAIN_PREFIX=$(SP_TOOLCHAIN_PREFIX)
endif

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/include/,$(INCLUDES))

ARCH_BINS  = $(addprefix $(SP_INSTALL_PREFIX)/bin/,$(BINS))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(ARCH_BINS) $(SP_INSTALL_PREFIX)/share/licenses

$(SP_INSTALL_PREFIX)/lib/z.lib: $(MAKE_ROOT)zlib.mk
	$(MAKE) -f $(MAKE_ROOT)zlib.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/bz2.lib: $(MAKE_ROOT)bzip2.mk
	$(MAKE) -f $(MAKE_ROOT)bzip2.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/lzma.lib: $(MAKE_ROOT)xz.mk
	$(MAKE) -f $(MAKE_ROOT)xz.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/zstd.lib: $(MAKE_ROOT)zstd.mk
	$(MAKE) -f $(MAKE_ROOT)zstd.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/brotlienc.lib: $(MAKE_ROOT)brotli.mk
	$(MAKE) -f $(MAKE_ROOT)brotli.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/jpeg.lib: $(MAKE_ROOT)libjpeg-turbo.mk
	$(MAKE) -f $(MAKE_ROOT)libjpeg-turbo.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/png16.lib: $(MAKE_ROOT)libpng.mk
	$(MAKE) -f $(MAKE_ROOT)libpng.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/gif.lib: $(MAKE_ROOT)gif.mk
	$(MAKE) -f $(MAKE_ROOT)gif.mk $(FORWARD_VARS)

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
	powershell 'if (Test-Path "$(SP_INSTALL_PREFIX)/share/licenses") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(SP_INSTALL_PREFIX)/share/licenses" }'
	powershell Copy-Item -Path "../licenses" -Destination "$(SP_INSTALL_PREFIX)/share/licenses" -Force -Recurse

.PHONY: all

endif
