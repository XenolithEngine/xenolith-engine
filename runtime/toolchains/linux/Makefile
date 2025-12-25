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

LCC_TOOLCHAIN_PATH ?= /opt/mcst/lcc-1.29.12.e2k-v5.8c2.linux-6.1

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

MKDIR = mkdir -p

SP_OPT := -O3 -fPIC -DPIC

LIBS := \
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
	libgost.a \
	libidn2.a \
	libcurl-openssl.a \
	libzip-openssl.a \
	libcurl-mbedtls.a \
	libzip-mbedtls.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a

WASM_LIBS := \
	libiwasm-release.a \
	libiwasm-debug.a

#
#	libdbus-1.a \
#	libxcb.a \
#	libxkbcommon.a \
#	libxcb-keysyms.a \
#	libffi.a \
#	libexpat.a \
#	libwayland-client.a \
#	libvulkan-1.a

BINS := \
	spirv-opt \
	glslangValidator

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

e2k:
	@mkdir -p $(abspath ../e2k-unknown-linux) $(abspath ../e2k-unknown-linux/include) $(abspath ../e2k-unknown-linux/lib) $(abspath ../e2k-unknown-linux/bin)
	$(MAKE) -f $(THIS_FILE) ARCH=e2k SP_TARGET=e2k-unknown-linux SP_TOOLCHAIN_PREFIX=$(LCC_TOOLCHAIN_PATH)/fs

aarch64:
	$(MAKE) -f $(THIS_FILE) ARCH=aarch64 SP_TARGET=aarch64-unknown-linux-gnu OUT_SYSROOT=$(OUT_SYSROOT)

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

ifeq ($(ARCH),e2k)
SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/../bin.toolchain/e2k-linux-gcc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/../bin.toolchain/e2k-linux-g++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/../bin.toolchain/e2k-linux-ar)

LIBS := $(filter-out $(WASM_LIBS),$(LIBS))

SP_IS_NATIVE := 1

else
SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/cc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/c++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/ar)
endif

endif # ($(ARCH),native)

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	LINUX=1 \
	ARCH=$(ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
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

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/include/,$(INCLUDES))

ARCH_BINS  = $(addprefix $(SP_INSTALL_PREFIX)/bin/,$(BINS))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(ARCH_BINS) $(SP_INSTALL_PREFIX)/share/licenses

$(SP_INSTALL_PREFIX)/lib/libbz2.a: $(MAKE_ROOT)bzip2.mk
	$(MAKE) -f $(MAKE_ROOT)bzip2.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/liblzma.a: $(MAKE_ROOT)xz.mk
	$(MAKE) -f $(MAKE_ROOT)xz.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libjpeg.a: $(MAKE_ROOT)libjpeg-turbo.mk
	$(MAKE) -f $(MAKE_ROOT)libjpeg-turbo.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libzstd.a: $(MAKE_ROOT)zstd.mk
	$(MAKE) -f $(MAKE_ROOT)zstd.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libpng16.a: $(MAKE_ROOT)libpng.mk
	$(MAKE) -f $(MAKE_ROOT)libpng.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libbrotlienc.a: $(MAKE_ROOT)brotli.mk
	$(MAKE) -f $(MAKE_ROOT)brotli.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libgif.a: $(MAKE_ROOT)gif.mk
	$(MAKE) -f $(MAKE_ROOT)gif.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libwebp.a: $(MAKE_ROOT)libwebp.mk $(SP_INSTALL_PREFIX)/lib/libpng16.a $(SP_INSTALL_PREFIX)/lib/libjpeg.a $(SP_INSTALL_PREFIX)/lib/libgif.a
	$(MAKE) -f $(MAKE_ROOT)libwebp.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libtiff.a: $(MAKE_ROOT)tiff.mk $(SP_INSTALL_PREFIX)/lib/libwebp.a $(SP_INSTALL_PREFIX)/lib/liblzma.a $(SP_INSTALL_PREFIX)/lib/libzstd.a
	$(MAKE) -f $(MAKE_ROOT)tiff.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libfreetype.a: $(MAKE_ROOT)freetype.mk $(SP_INSTALL_PREFIX)/lib/libbrotlienc.a $(SP_INSTALL_PREFIX)/lib/libpng16.a $(SP_INSTALL_PREFIX)/lib/libbz2.a
	$(MAKE) -f $(MAKE_ROOT)freetype.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libharfbuzz.a: $(MAKE_ROOT)harfbuzz.mk $(SP_INSTALL_PREFIX)/lib/libfreetype.a
	$(MAKE) -f $(MAKE_ROOT)harfbuzz.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libsqlite3.a: $(MAKE_ROOT)sqlite.mk
	$(MAKE) -f $(MAKE_ROOT)sqlite.mk $(FORWARD_VARS)

# Actually, use libuidna to mimic libidn2
$(SP_INSTALL_PREFIX)/lib/libidn2.a: $(MAKE_ROOT)uidna.mk $(SP_INSTALL_PREFIX)/lib/libicuuc.a
	$(MAKE) -f $(MAKE_ROOT)uidna.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libicuuc.a: $(MAKE_ROOT)icu.mk
	$(MAKE) -f $(MAKE_ROOT)icu.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libcurl-openssl.a: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/libssl.a $(SP_INSTALL_PREFIX)/lib/libidn2.a
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/lib/libcurl-mbedtls.a: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/libmbedtls.a $(SP_INSTALL_PREFIX)/lib/libidn2.a
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/lib/libcurl-gnutls.a: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/libidn2.a
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=gnutls

$(SP_INSTALL_PREFIX)/lib/libmbedtls.a: $(MAKE_ROOT)mbedtls.mk
	$(MAKE) -f $(MAKE_ROOT)mbedtls.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libnghttp3.a: $(MAKE_ROOT)nghttp3.mk
	$(MAKE) -f $(MAKE_ROOT)nghttp3.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libzip-openssl.a: $(MAKE_ROOT)libzip.mk $(SP_INSTALL_PREFIX)/lib/libssl.a $(SP_INSTALL_PREFIX)/lib/libgost.a
	$(MAKE) -f $(MAKE_ROOT)libzip.mk $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/lib/libzip-mbedtls.a: $(MAKE_ROOT)libzip.mk $(SP_INSTALL_PREFIX)/lib/libmbedtls.a
	$(MAKE) -f $(MAKE_ROOT)libzip.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/lib/libzip-gnutls.a: $(MAKE_ROOT)libzip.mk
	$(MAKE) -f $(MAKE_ROOT)libzip.mk $(FORWARD_VARS) VARIANT=gnutls

$(SP_INSTALL_PREFIX)/lib/libz.a: $(MAKE_ROOT)zlib.mk
	$(MAKE) -f $(MAKE_ROOT)zlib.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libssl.a: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $(MAKE_ROOT)openssl.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libgost.a: $(MAKE_ROOT)gost.mk $(SP_INSTALL_PREFIX)/lib/libssl.a
	$(MAKE) -f $(MAKE_ROOT)gost.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk $(SP_INSTALL_PREFIX)/lib/libz.a
	$(MAKE) -f $(MAKE_ROOT)backtrace.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libdbus-1.a: $(MAKE_ROOT)dbus.mk
	$(MAKE) -f $(MAKE_ROOT)dbus.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libxkbcommon.a: $(MAKE_ROOT)xkbcommon.mk $(SP_INSTALL_PREFIX)/lib/libxcb.a
	$(MAKE) -f $(MAKE_ROOT)xkbcommon.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libxcb.a: $(MAKE_ROOT)xcb.mk
	$(MAKE) -f $(MAKE_ROOT)xcb.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libxcb-keysyms.a: $(MAKE_ROOT)xcb-utils.mk $(SP_INSTALL_PREFIX)/lib/libxkbcommon.a
	$(MAKE) -f $(MAKE_ROOT)xcb-utils.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libffi.a: $(MAKE_ROOT)ffi.mk
	$(MAKE) -f $(MAKE_ROOT)ffi.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libexpat.a: $(MAKE_ROOT)expat.mk
	$(MAKE) -f $(MAKE_ROOT)expat.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libwayland-client.a: $(MAKE_ROOT)wayland.mk $(SP_INSTALL_PREFIX)/lib/libxkbcommon.a $(SP_INSTALL_PREFIX)/lib/libffi.a $(SP_INSTALL_PREFIX)/lib/libexpat.a
	$(MAKE) -f $(MAKE_ROOT)wayland.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libvulkan-1.a: $(MAKE_ROOT)vulkan-loader.mk $(SP_INSTALL_PREFIX)/include/vulkan/vulkan.h
	$(MAKE) -f $(MAKE_ROOT)vulkan-loader.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/lib/libiwasm-debug.a: $(MAKE_ROOT)wamr.mk $(SP_INSTALL_PREFIX)/lib/libz.a
	$(MAKE) -f $(MAKE_ROOT)wamr.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/include/vulkan/vulkan.h: $(MAKE_ROOT)vulkan-headers.mk
	$(MAKE) -f $(MAKE_ROOT)vulkan-headers.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/include/spirv/unified1/spirv.h: $(MAKE_ROOT)spirv-headers.mk
	$(MAKE) -f $(MAKE_ROOT)spirv-headers.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/include/simde/simde-arch.h: $(MAKE_ROOT)simde.mk
	$(MAKE) -f $(MAKE_ROOT)simde.mk $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/include/simde/simde-arch.h

$(SP_INSTALL_PREFIX)/bin/spirv-opt: $(MAKE_ROOT)spirv-tools.mk $(SP_INSTALL_PREFIX)/include/spirv/unified1/spirv.h
	$(MAKE) -f $(MAKE_ROOT)spirv-tools.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/bin/glslangValidator: $(MAKE_ROOT)glslang.mk $(SP_INSTALL_PREFIX)/bin/spirv-opt $(SP_INSTALL_PREFIX)/include/vulkan/vulkan.h
	$(MAKE) -f $(MAKE_ROOT)glslang.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/share/licenses:
	rm -rf $(SP_INSTALL_PREFIX)/share/licenses
	cp -rf runtime/include/* $(SP_INSTALL_PREFIX)/include
	cp -rf ../licenses $(SP_INSTALL_PREFIX)/share/licenses

.PHONY: all

endif
