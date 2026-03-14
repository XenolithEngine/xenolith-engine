# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

ifdef ANDROID_NDK_ROOT
NDK ?= $(ANDROID_NDK_ROOT)
else
NDK ?= $(HOME)/Android/Ndk
endif

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk

NDKPATH = $(NDK)/toolchains/llvm/prebuilt/$(HOST_ANDROID)/bin

ANDROID_PLATFORM_LEVEL = 24

INCLUDES := \
	idn2.h \
	simde/simde-arch.h \
	spirv/unified1/spirv.h \

LIBS := \
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
	libzip-openssl.a \
	libzip-mbedtls.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a

ifndef ARCH

ARCHS_NDK := x86-ndk x86_64-ndk arm64-v8a-ndk armeabi-v7a-ndk

all-ndk: $(ARCHS_NDK)

clean:
	rm -rf x86 x86_64 arm64-v8a armeabi-v7a

#
# Вот с таким подходом к именованию архитектур вам приходится иметь дело в Android
# Радуйтесь!
#

SP_ARCH_x86_64_INFO_BASE := \
	ANDROID_ARCH_ABI=x86_64 ARCH=x86_64 \
	ANDROID_ARCH=x86_64 \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	ANDROID_TARGET=x86_64-linux-android \
	SP_RES_ARCH=x86_64 \
	SP_ARCH_TARGET=x86_64 \
	SP_ARCH_TARGET_CLANG=x86_64-unknown-linux-android \
	NDKP=x86_64-linux-android$(ANDROID_PLATFORM_LEVEL)

SP_ARCH_x86_64_INFO_NDK := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SP_TARGET=x86_64-linux-android \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-ndk-linux-android \
	SP_NDK=1

SP_ARCH_x86_64_INFO := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SP_TARGET=x86_64-unknown-linux-android \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-unknown-linux-android

SP_ARCH_x86_INFO_BASE := \
	ANDROID_ARCH_ABI=x86 ARCH=x86 \
	ANDROID_ARCH=x86 \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	ANDROID_TARGET=i686-linux-android \
	SP_RES_ARCH=i686 \
	SP_ARCH_TARGET=x86 \
	SP_ARCH_TARGET_CLANG=i686-unknown-linux-android \
	NDKP=i686-linux-android$(ANDROID_PLATFORM_LEVEL)

SP_ARCH_x86_INFO_NDK := \
	$(SP_ARCH_x86_INFO_BASE) \
	SP_TARGET=i686-linux-android \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/i686-ndk-linux-android \
	SP_NDK=1

SP_ARCH_x86_INFO := \
	$(SP_ARCH_x86_INFO_BASE) \
	SP_TARGET=i686-unknown-linux-android \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/i686-unknown-linux-android

SP_ARCH_arm64_INFO_BASE := \
	ANDROID_ARCH_ABI=arm64-v8a ARCH=arm64-v8a \
	ANDROID_ARCH=arm64 \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	ANDROID_TARGET=aarch64-linux-android \
	SP_RES_ARCH=aarch64 \
	SP_ARCH_TARGET=aarch64 \
	SP_ARCH_TARGET_CLANG=aarch64-unknown-linux-android \
	NDKP=aarch64-linux-android$(ANDROID_PLATFORM_LEVEL)

SP_ARCH_arm64_INFO_NDK := \
	$(SP_ARCH_arm64_INFO_BASE) \
	SP_TARGET=aarch64-linux-android \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-ndk-linux-android \
	SP_NDK=1

SP_ARCH_arm64_INFO := \
	$(SP_ARCH_arm64_INFO_BASE) \
	SP_TARGET=aarch64-unknown-linux-android \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-unknown-linux-android

SP_ARCH_arm_INFO_BASE := \
	ANDROID_ARCH_ABI=armeabi-v7a ARCH=armeabi-v7a \
	ANDROID_ARCH=arm \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	ANDROID_TARGET=arm-linux-androideabi \
	SP_RES_ARCH=arm \
	SP_ARCH_TARGET=armv7a \
	SP_ARCH_TARGET_CLANG=armv7a-unknown-linux-androideabi \
	NDKP=armv7a-linux-androideabi$(ANDROID_PLATFORM_LEVEL)

SP_ARCH_arm_INFO_NDK := \
	$(SP_ARCH_arm_INFO_BASE) \
	SP_TARGET=armv7a-linux-androideabi \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/armv7a-ndk-linux-androideabi \
	SP_NDK=1

SP_ARCH_arm_INFO := \
	$(SP_ARCH_arm_INFO_BASE) \
	SP_TARGET=armv7a-unknown-linux-androideabi \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/armv7a-unknown-linux-androideabi

x86_64-ndk:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-ndk-linux-android/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-ndk-linux-android/usr/lib
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_INFO_NDK)

x86-ndk:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/i686-ndk-linux-android/usr/include $(TOOLCHAIN_OUTPUT_DIR)/i686-ndk-linux-android/usr/lib
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_INFO_NDK)

arm64-v8a-ndk:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-ndk-linux-android/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-ndk-linux-android/usr/lib
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_arm64_INFO_NDK)

armeabi-v7a-ndk:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/armv7a-ndk-linux-androideabi/usr/include $(TOOLCHAIN_OUTPUT_DIR)/armv7a-ndk-linux-androideabi/usr/lib
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_arm_INFO_NDK)

TARGET_FILE := $(dir $(THIS_FILE))target.mk

x86_64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-unknown-linux-android/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-unknown-linux-android/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_INFO)

x86:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/i686-unknown-linux-android/usr/include $(TOOLCHAIN_OUTPUT_DIR)/i686-unknown-linux-android/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_INFO)

arm64-v8a:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-unknown-linux-android/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-unknown-linux-android/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_arm64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_arm64_INFO)

armeabi-v7a:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/armv7a-unknown-linux-androideabi/usr/include $(TOOLCHAIN_OUTPUT_DIR)/armv7a-unknown-linux-androideabi/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_arm_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_arm_INFO)

.PHONY: all-ndk x86 x86_64 arm64-v8a armeabi-v7a x86-ndk x86_64-ndk arm64-v8a-ndk armeabi-v7a-ndk
.DEFAULT_GOAL := all-ndk

else

SP_OPT := -Oz -fPIC -DPIC -ffunction-sections -fdata-sections

SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

ifdef SP_NDK

SP_CC := $(NDKPATH)/$(NDKP)-clang
SP_CXX := $(NDKPATH)/$(NDKP)-clang++
SP_AR := $(NDKPATH)/llvm-ar

FORWARD_VARS := \
	ANDROID=1 \
	ANDROID_ARCH=$(ANDROID_ARCH) \
	ANDROID_TARGET=$(ANDROID_TARGET) \
	ARCH=$(ARCH) \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	SP_TARGET=$(SP_TARGET) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT) \
	NDK=$(NDK) \
	NDKP=$(NDKP) \
	NDKPATH=$(NDKPATH)

else

HOST_BINDIR := $(abspath $(MAKE_ROOT)../hosts/$(HOST_ID)/bin)

SP_CC := $(HOST_BINDIR)/clang
SP_CXX := $(HOST_BINDIR)/clang++
SP_AR := $(HOST_BINDIR)/llvm-ar

FORWARD_VARS := \
	ANDROID=1 \
	ANDROID_ARCH=$(ANDROID_ARCH) \
	ANDROID_TARGET=$(ANDROID_TARGET) \
	ARCH=$(ARCH) \
	ANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	SP_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	SP_TOOLCHAIN_PREFIX=$(TOOLCHAIN_OUTPUT_DIR) \
	SP_TARGET=$(SP_TARGET) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

INCLUDES += vulkan/vulkan.h

endif

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(SP_INSTALL_PREFIX)/share/licenses

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

$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a:  ../common/nghttp3.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libssl.a: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a:  ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a: ../common/mbedtls.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-mbedtls.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a \
		$(SP_INSTALL_PREFIX)/usr/include/idn2.h
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
		$(SP_INSTALL_PREFIX)/usr/include/idn2.h
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libzip-openssl.a: ../common/libzip.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a $(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libzip-mbedtls.a: ../common/libzip.mk $(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/usr/lib/libiwasm-debug.a:  ../common/wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/idn2.h: replacements/idn2.h
	$(MKDIR) $(SP_INSTALL_PREFIX)/usr/include
	cp -f replacements/idn2.h $@

$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h: ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h

$(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h

$(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h: ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h

$(SP_INSTALL_PREFIX)/share/licenses: ../licenses
	$(MKDIR) $(SP_INSTALL_PREFIX)/share
	cp -rf $< $@

.PHONY: all
.DEFAULT_GOAL := all

endif
