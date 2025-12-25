# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

TOOLCHAIN_OUTPUT_DIR ?= $(realpath $(MAKE_ROOT)/../intermediate)

MKDIR = mkdir -p

NDK ?= $(HOME)/Android/Ndk
NDKPATH = $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin

ANDROID_PLATFORM_LEVEL = 24

INCLUDES := \
	idn2.h \
	simde/simde-arch.h

LIBS := \
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
	libgost.a \
	libcurl-openssl.a \
	libzip-openssl.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a

#	libmbedtls.a \
#	libcurl-mbedtls.a \
#	libzip-mbedtls.a \

ifndef ARCH

ARCHS := x86 x86_64 arm64-v8a armeabi-v7a

all: $(ARCHS)

clean:
	rm -rf x86 x86_64 arm64-v8a armeabi-v7a

x86_64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-linux-android/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-linux-android/lib
	$(MAKE) -f $(THIS_FILE) ARCH=x86_64 ANDROID_ARCH=x86_64 TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR) \
			NDKP=x86_64-linux-android$(ANDROID_PLATFORM_LEVEL) SP_TARGET=x86_64-linux-android

x86:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/i686-linux-android/include $(TOOLCHAIN_OUTPUT_DIR)/i686-linux-android/lib
	$(MAKE) -f $(THIS_FILE) ARCH=x86 ANDROID_ARCH=x86 TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR) \
			NDKP=i686-linux-android$(ANDROID_PLATFORM_LEVEL) SP_TARGET=i686-linux-android

arm64-v8a:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-linux-android/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-linux-android/lib
	$(MAKE) -f $(THIS_FILE) ARCH=arm64-v8a ANDROID_ARCH=arm64 TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR) \
			NDKP=aarch64-linux-android$(ANDROID_PLATFORM_LEVEL) SP_TARGET=aarch64-linux-android

armeabi-v7a:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/armv7a-linux-androideabi/include $(TOOLCHAIN_OUTPUT_DIR)/armv7a-linux-androideabi/lib
	$(MAKE) -f $(THIS_FILE) ARCH=armeabi-v7a ANDROID_ARCH=arm TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR) \
			NDKP=armv7a-linux-androideabi$(ANDROID_PLATFORM_LEVEL) SP_TARGET=armv7a-linux-androideabi

.PHONY: all x86 x86_64 arm64-v8a armeabi-v7a

else

SP_CC := $(NDKPATH)/$(NDKP)-clang
SP_CXX := $(NDKPATH)/$(NDKP)-clang++
SP_AR := $(NDKPATH)/llvm-ar
SP_OPT := -Oz -fPIC -DPIC -ffunction-sections -fdata-sections

SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)/$(SP_TARGET)

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	ANDROID=1 \
	ANDROID_ARCH=$(ANDROID_ARCH) \
	ARCH=$(ARCH) \
	NDK=$(NDK) \
	NDKP=$(NDKP) \
	NDKPATH=$(NDKPATH) \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	SP_TARGET=$(SP_TARGET) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/include/,$(INCLUDES))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(SP_INSTALL_PREFIX)/share/licenses $(SP_INSTALL_PREFIX)/toolchain.mk

$(SP_INSTALL_PREFIX)/lib/libbz2.a: $(MAKE_ROOT)bzip2.mk
	$(MAKE) -f $(MAKE_ROOT)bzip2.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/liblzma.a: $(MAKE_ROOT)xz.mk
	$(MAKE) -f $(MAKE_ROOT)xz.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libzstd.a: $(MAKE_ROOT)zstd.mk
	$(MAKE) -f $(MAKE_ROOT)zstd.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libbrotlienc.a: $(MAKE_ROOT)brotli.mk
	$(MAKE) -f $(MAKE_ROOT)brotli.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libjpeg.a: $(MAKE_ROOT)libjpeg-turbo.mk
	$(MAKE) -f $(MAKE_ROOT)libjpeg-turbo.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libpng16.a: $(MAKE_ROOT)libpng.mk
	$(MAKE) -f $(MAKE_ROOT)libpng.mk $(FORWARD_VARS)

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

$(SP_INSTALL_PREFIX)/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk
	$(MAKE) -f $(MAKE_ROOT)backtrace.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libsqlite3.a: $(MAKE_ROOT)sqlite.mk
	$(MAKE) -f $(MAKE_ROOT)sqlite.mk $(FORWARD_VARS)

# Actually, use libuidna to mimic libidn2
$(SP_INSTALL_PREFIX)/lib/libidn2.a: $(MAKE_ROOT)uidna.mk
	$(MAKE) -f $(MAKE_ROOT)uidna.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libssl.a: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $(MAKE_ROOT)openssl.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libmbedtls.a: $(MAKE_ROOT)mbedtls.mk
	$(MAKE) -f $(MAKE_ROOT)mbedtls.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libcurl-mbedtls.a: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/libmbedtls.a $(SP_INSTALL_PREFIX)/lib/libidn2.a
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/lib/libcurl-openssl.a: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/lib/libssl.a $(SP_INSTALL_PREFIX)/lib/libidn2.a
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/lib/libzip-mbedtls.a: $(MAKE_ROOT)libzip.mk $(SP_INSTALL_PREFIX)/lib/libmbedtls.a
	$(MAKE) -f $(MAKE_ROOT)libzip.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/lib/libzip-openssl.a: $(MAKE_ROOT)libzip.mk $(SP_INSTALL_PREFIX)/lib/libssl.a
	$(MAKE) -f $(MAKE_ROOT)libzip.mk $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/lib/libgost.a: $(MAKE_ROOT)gost.mk $(SP_INSTALL_PREFIX)/lib/libssl.a
	$(MAKE) -f $(MAKE_ROOT)gost.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/lib/libiwasm-debug.a: $(MAKE_ROOT)wamr.mk
	$(MAKE) -f $(MAKE_ROOT)wamr.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/libnghttp3.a: $(MAKE_ROOT)nghttp3.mk
	$(MAKE) -f $(MAKE_ROOT)nghttp3.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/include/idn2.h: replacements/idn2.h
	$(MKDIR) $(SP_INSTALL_PREFIX)/include
	cp -f replacements/idn2.h $@

$(SP_INSTALL_PREFIX)/include/simde/simde-arch.h: $(MAKE_ROOT)simde.mk
	$(MAKE) -f $(MAKE_ROOT)simde.mk $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/include/simde/simde-arch.h

$(SP_INSTALL_PREFIX)/toolchain.mk:
	@echo 'TOOLCHAIN_SYSROOT := $$(abspath $$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TOOLCHAIN_NDKPATH ?= $$(NDK)/toolchains/llvm/prebuilt/$$(SP_OS)-$(SP_HOST_ARCH)/bin' >> $@
	@echo 'TOOLCHAIN_TARGET := $(SP_TARGET)' >> $@
	@echo 'TOOLCHAIN_LIBDIR := $$(TOOLCHAIN_SYSROOT)/lib' >> $@
	@echo 'TOOLCHAIN_INCLUDEDIR := $$(TOOLCHAIN_SYSROOT)/include' >> $@
	@echo 'TOOLCHAIN_CC := $$(TOOLCHAIN_NDKPATH)/$(SP_TARGET)-clang' >> $@
	@echo 'TOOLCHAIN_CXX := $$(TOOLCHAIN_NDKPATH)/$(SP_TARGET)-clang++' >> $@
	@echo 'TOOLCHAIN_AR := $$(TOOLCHAIN_NDKPATH)/llvm-ar' >> $@
	@echo 'TOOLCHAIN_GENERAL_CXXFLAGS :=' >> $@
	@echo 'TOOLCHAIN_LIB_LDFLAGS :=' >> $@
	@echo 'TOOLCHAIN_EXEC_LDFLAGS :=' >> $@
	@echo '$$(info Available toolchain: $$(TOOLCHAIN_TARGET): $$(TOOLCHAIN_SYSROOT))' >> $@

$(SP_INSTALL_PREFIX)/share/licenses: ../licenses
	$(MKDIR) $(SP_INSTALL_PREFIX)/share
	cp -rf $< $@

.PHONY: all
.DEFAULT_GOAL := all

endif
