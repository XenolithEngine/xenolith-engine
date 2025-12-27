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
LIBS_MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
LIBS_MAKE_ROOT := $(dir $(LIBS_MAKE_FILE))

SRC_ROOT := $(LIBS_MAKE_ROOT)src
TMP_DIR := $(LIBS_MAKE_ROOT)tmp
XWIN_ROOT := $(LIBS_MAKE_ROOT)xwin

WGET = wget
MKDIR = mkdir -p
TAR_XF = tar -xf

LIBS = \
	bzip2 \
	xz \
	zstd \
	jpeg \
	libjpeg-turbo \
	libpng \
	giflib \
	libwebp \
	tiff \
	brotli \
	curl \
	freetype \
	harfbuzz \
	sqlite \
	libuidna \
	mbedtls \
	nghttp3 \
	libzip \
	zlib \
	openssl \
	openssl-gost-engine \
	wasm-micro-runtime \
	vulkan-headers \
	spirv-headers \
	glslang \
	spirv-tools \
	icu4c \
	ffi \
	expat \
	libbacktrace \
	simde

# Extra Linux deps
#	dbus \
#	libxcb \
#	xcb-proto \
#	libXau \
#	xorgproto \
#	xkbcommon \
#	xcb-util-keysyms \
#	xcb-util-errors \
#	xcb-util-cursor \
#	wayland \
#	vulkan-loader \



UNAME := $(shell uname -a)

# DO NOT use MSYS cmake - it's buggy for compiling with clang on windows
MSYS_PREPARE := pacman --noconfirm --needed -S curl wget tar sed unzip git binutils gcc pkg-config

PREPARE := $(if $(filter MSYS_NT%,$(UNAME)),$(MSYS_PREPARE))

get_tar_top_dir = `tar -tf $(1)  | head -1 | cut -f1 -d"/"`

include src.mk

ALL_SRC := $(addprefix $(SRC_ROOT)/,$(LIBS)) replacements/curl/cacert.pem

TOOLCHAIN_INTERMEDIATE := $(abspath $(LIBS_MAKE_ROOT))/intermediate
TOOLCHAIN_TARGETS := $(abspath $(LIBS_MAKE_ROOT))/targets

prepare:
	$(PREPARE)

download: $(ALL_SRC)

download: prepare
	rm -rf $(TMP_DIR)

clean:
	rm -rf $(SRC_ROOT) $(XWIN_ROOT) $(TMP_DIR) replacements/curl/cacert.pem

xwin: $(SRC_ROOT)/xwin/splat

ANDROID_INTERMEDIATE_TOOLCHAINS := $(addprefix $(TOOLCHAIN_INTERMEDIATE)/,\
	aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android)

ANDROID_TARGETS_TOOLCHAINS := $(addprefix $(TOOLCHAIN_TARGETS)/,\
	aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android)

$(ANDROID_INTERMEDIATE_TOOLCHAINS):
	mkdir -p $(TOOLCHAIN_INTERMEDIATE)
	make -C android TOOLCHAIN_OUTPUT_DIR=$(abspath $(LIBS_MAKE_ROOT))/intermediate all

$(TOOLCHAIN_TARGETS)/android: $(ANDROID_INTERMEDIATE_TOOLCHAINS)
	$(MAKE) -f android.mk TOOLCHAIN_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE) TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS)
	touch $@

$(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu:
	mkdir -p $(TOOLCHAIN_INTERMEDIATE)
	make -C toolchain-linux-glibc OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-unknown-linux-gnu out
	make -C linux x86_64 OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-unknown-linux-gnu

$(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu: $(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu
	$(MAKE) -f linux.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu

android: $(TOOLCHAIN_TARGETS)/android

android-clean:
	-rm -rf $(ANDROID_INTERMEDIATE_TOOLCHAINS) $(TOOLCHAIN_TARGETS)/android

linux-glibc-x86_64: $(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu

android.tar.xz: $(TOOLCHAIN_TARGETS)/android
	tar -cJf $@ $<

compress: \
	android.tar.xz \
	x86_64-unknown-linux-gnu.tar.xz

.PHONY: all clean prepare compress xwin android android-clean linux-glibc-x86_64
.DEFAULT_GOAL := all