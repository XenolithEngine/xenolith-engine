# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

.DEFAULT_GOAL := all

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

MKDIR = mkdir -p

SP_OPT := -O3 -fPIC -DPIC

LIBS := \
	libc++.a \
	libz.a \
	libbz2.a \
	liblzma.a \
	libzstd.a \
	libbrotlienc.a \
	libicuuc.a \
	libjpeg.a \
	libpng16.a \
	libgif.a \
	libwebp.a \
	libtiff.a \
	libfreetype.a \
	libharfbuzz.a \
	libsqlite3.a \
	libmbedtls.a \
	libnghttp3.a \
	libssl.a \
	libgost_engine.a \
	libidn2.a \
	libcurl-openssl.a \
	libzip-openssl.a \
	libcurl-mbedtls.a \
	libzip-mbedtls.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a \
	libxml2.a \
	libffi.a \
	libexpat.a \
	libwayland-client.a

WASM_LIBS := \
	libiwasm-release.a \
	libiwasm-debug.a

BINS := \
	spirv-opt \
	glslang

INCLUDES := \
	spirv/unified1/spirv.h \
	vulkan/vulkan.h \
	simde/simde-arch.h

ifndef ARCH

ARCHS := e2k x86_64

all: $(ARCHS)

clean:

native:
	@mkdir -p $(abspath ../native) $(abspath ../native/include) $(abspath ../native/lib) $(abspath ../native/bin)
	$(MAKE) -f $(THIS_FILE) ARCH=native

x86_64:
	$(MAKE) -f $(THIS_FILE) ARCH=x86_64 SP_TARGET=x86_64-unknown-linux-gnu OUT_SYSROOT=$(OUT_SYSROOT)

aarch64:
	$(MAKE) -f $(THIS_FILE) ARCH=aarch64 SP_TARGET=aarch64-unknown-linux-gnu OUT_SYSROOT=$(OUT_SYSROOT)

riscv64:
	$(MAKE) -f $(THIS_FILE) ARCH=riscv64 SP_TARGET=riscv64-unknown-linux-gnu OUT_SYSROOT=$(OUT_SYSROOT)

e2k:
	@mkdir -p $(abspath ../e2k-unknown-linux) $(abspath ../e2k-unknown-linux/include) $(abspath ../e2k-unknown-linux/lib) $(abspath ../e2k-unknown-linux/bin)
	$(MAKE) -f $(THIS_FILE) ARCH=e2k SP_TARGET=e2k-unknown-linux SP_TOOLCHAIN_PREFIX=$(LCC_TOOLCHAIN_PATH)/fs

.PHONY: all x86_64 e2k aarch64

else # ARCH

ifeq ($(ARCH),native)

SP_INSTALL_PREFIX ?= $(realpath ../native)

SP_CC := gcc
SP_CXX := g++
SP_AR := ar

ARCH := $(shell uname -m)
SP_TARGET := $(ARCH)-unknown-linux
SP_IS_NATIVE := 1

else # ($(ARCH),native)

OUT_SYSROOT ?= $(realpath ../$(SP_TARGET))

SP_TOOLCHAIN_PREFIX ?= $(OUT_SYSROOT)
SP_INSTALL_PREFIX ?= $(OUT_SYSROOT)
SP_TOOLCHAIN_FILE ?= $(realpath $(OUT_SYSROOT)/toolchain.cmake)

SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/cc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/c++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/ar)

endif # ($(ARCH),native)

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	LINUX=1 \
	ARCH=$(ARCH) \
	SP_ARCH=$(ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_TOOLCHAIN_FILE) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ifdef SP_TOOLCHAIN_PREFIX
FORWARD_VARS += SP_TOOLCHAIN_PREFIX=$(SP_TOOLCHAIN_PREFIX)
endif

ifdef SP_IS_NATIVE
FORWARD_VARS += SP_NATIVE=1
endif

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(SP_INSTALL_PREFIX)/share/licenses

$(SP_INSTALL_PREFIX)/usr/lib/libc++.a: ../common/libc++.mk
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

# Actually, use libuidna to mimic libidn2
$(SP_INSTALL_PREFIX)/usr/lib/libidn2.a: $(MAKE_ROOT)uidna.mk $(SP_INSTALL_PREFIX)/usr/lib/libicuuc.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libicuuc.a: $(MAKE_ROOT)icu.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a $(SP_INSTALL_PREFIX)/usr/lib/libidn2.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-mbedtls.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a $(SP_INSTALL_PREFIX)/usr/lib/libidn2.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/libzip-openssl.a: ../common/libzip.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a $(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libzip-mbedtls.a: ../common/libzip.mk $(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/usr/lib/libiwasm-debug.a:  ../common/wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libxml2.a: ../common/libxml2.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libffi.a: $(MAKE_ROOT)ffi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libexpat.a: $(MAKE_ROOT)expat.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libwayland-client.a: $(MAKE_ROOT)wayland.mk $(SP_INSTALL_PREFIX)/usr/lib/libxml2.a $(SP_INSTALL_PREFIX)/usr/lib/libffi.a $(SP_INSTALL_PREFIX)/usr/lib/libexpat.a
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
	cp -rf runtime/include/* $(SP_INSTALL_PREFIX)/include
	cp -rf ../licenses $(SP_INSTALL_PREFIX)/share/licenses

.PHONY: all

endif
