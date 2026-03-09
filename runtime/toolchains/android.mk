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

TOOLCHAIN_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate
TOOLCHAIN_TARGETS ?= $(abspath $(LIBS_MAKE_ROOT))/targets

ANDROID_TARGET := $(TOOLCHAIN_TARGETS)/unknown-ndk-linux-android/usr

DIFF_FILES_A := $(filter $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/%,\
	$(shell diff -qr $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/include/ $(TOOLCHAIN_INTERMEDIATE)/aarch64-ndk-linux-android/usr/include/))
DIFF_FILES_B := $(filter $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/%,\
	$(shell diff -qr $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/include/ $(TOOLCHAIN_INTERMEDIATE)/armv7a-ndk-linux-androideabi/usr/include/))
DIFF_FILES_C := $(filter $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/%,\
	$(shell diff -qr $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/include/ $(TOOLCHAIN_INTERMEDIATE)/i686-ndk-linux-android/usr/include/))

ARCH_CONF_FILES := $(sort $(DIFF_FILES_A) $(DIFF_FILES_B) $(DIFF_FILES_C))

FILES_TO_REMOVE := $(patsubst $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/%,\
	$(ANDROID_TARGET)/%,\
	$(ARCH_CONF_FILES))

ARCH_aarch64-linux-android_SOURCE := aarch64-ndk-linux-android
ARCH_armv7a-linux-androideabi_SOURCE := armv7a-ndk-linux-androideabi
ARCH_i686-linux-android_SOURCE := i686-ndk-linux-android
ARCH_x86_64-linux-android_SOURCE := x86_64-ndk-linux-android

ARCH_TARGET_DIRS := aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android

A_TARGET_INCLUDE := $(ANDROID_TARGET)/include
A_TARGET_LIB := $(ANDROID_TARGET)/lib

A_TARGET_INCLUDE_AARCH64 := $(ANDROID_TARGET)/include/aarch64-linux-android
A_TARGET_INCLUDE_AARCH64_ABI := $(ANDROID_TARGET)/include/arm64-v8a

A_TARGET_INCLUDE_ARMV7A := $(ANDROID_TARGET)/include/armv7a-linux-androideabi
A_TARGET_INCLUDE_ARMV7A_ABI := $(ANDROID_TARGET)/include/armeabi-v7a

A_TARGET_INCLUDE_I686 := $(ANDROID_TARGET)/include/i686-linux-android
A_TARGET_INCLUDE_I686_ABI := $(ANDROID_TARGET)/include/x86

A_TARGET_INCLUDE_X86_64 := $(ANDROID_TARGET)/include/x86_64-linux-android
A_TARGET_INCLUDE_X86_64_ABI := $(ANDROID_TARGET)/include/x86_64

$(A_TARGET_INCLUDE)/aarch64-linux-android/%: $(TOOLCHAIN_INTERMEDIATE)/aarch64-ndk-linux-android/usr/include/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_INCLUDE)/armv7a-linux-androideabi/%: $(TOOLCHAIN_INTERMEDIATE)/armv7a-ndk-linux-androideabi/usr/include/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_INCLUDE)/i686-linux-android/%: $(TOOLCHAIN_INTERMEDIATE)/i686-ndk-linux-android/usr/include/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_INCLUDE)/x86_64-linux-android/%: $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/include/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_LIB)/aarch64-linux-android/%: $(TOOLCHAIN_INTERMEDIATE)/aarch64-ndk-linux-android/usr/lib/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_LIB)/armv7a-linux-androideabi/%: $(TOOLCHAIN_INTERMEDIATE)/armv7a-ndk-linux-androideabi/usr/lib/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_LIB)/i686-linux-android/%: $(TOOLCHAIN_INTERMEDIATE)/i686-ndk-linux-android/usr/lib/% $(ANDROID_TARGET)
	cp -f $< $@

$(A_TARGET_LIB)/x86_64-linux-android/%: $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/lib/% $(ANDROID_TARGET)
	cp -f $< $@

ALL_ARCH_FILES := \
	$(foreach arch,$(ARCH_TARGET_DIRS),\
		$(patsubst $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/include/%,\
		$(A_TARGET_INCLUDE)/$(arch)/%,$(ARCH_CONF_FILES)) \
	)

ALL_ARCH_LIBS := \
	$(foreach arch,$(ARCH_TARGET_DIRS),\
		$(patsubst $(TOOLCHAIN_INTERMEDIATE)/$(ARCH_$(arch)_SOURCE)/usr/lib/%,$(A_TARGET_LIB)/$(arch)/%,\
			$(wildcard $(TOOLCHAIN_INTERMEDIATE)/$(ARCH_$(arch)_SOURCE)/usr/lib/*.a)))

$(ANDROID_TARGET):
	mkdir -p $(ANDROID_TARGET)/include
	mkdir -p $(addprefix $(ANDROID_TARGET)/lib/,$(ARCH_TARGET_DIRS))
	mkdir $(sort $(dir $(ALL_ARCH_FILES)))
	cp -rf $(TOOLCHAIN_INTERMEDIATE)/x86_64-ndk-linux-android/usr/include/* $(ANDROID_TARGET)/include
	rm $(FILES_TO_REMOVE)

$(TOOLCHAIN_TARGETS)/unknown-ndk-linux-android/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Android-NDK' >> $@
	@echo 'TARGET_NDKPATH ?= $$(NDK)/toolchains/llvm/prebuilt/$$(ANDROID_HOST)/bin' >> $@
	@echo 'TARGET_LIBDIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_INCLUDEDIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_CC := ndk-build' >> $@
	@echo 'TARGET_CXX := ndk-build' >> $@
	@echo 'TARGET_AR := $$(TARGET_NDKPATH)/llvm-ar' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo '$$(info Available toolchain: unknown-ndk-linux-android: $$(TARGET_SYSROOT))' >> $@

all: $(ALL_ARCH_FILES) $(ALL_ARCH_LIBS) $(TOOLCHAIN_TARGETS)/unknown-ndk-linux-android/target.mk
	mkdir -p $(TOOLCHAIN_TARGETS)/unknown-ndk-linux-android/share
	cp -rf licenses $(TOOLCHAIN_TARGETS)/unknown-ndk-linux-android/share

.PHONY: all
.DEFAULT_GOAL := all
