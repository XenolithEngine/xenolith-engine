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

include $(dir $(THIS_FILE))common/utils/detect-platform.mk

SRC_ROOT := $(LIBS_MAKE_ROOT)src
TMP_DIR := $(LIBS_MAKE_ROOT)tmp
XWIN_ROOT := $(LIBS_MAKE_ROOT)xwin

ifeq ($(findstring Windows,$(OS)),Windows)
$(info Using powershell)

WGET = curl.exe
SHELL := powershell.exe
MKDIR ?= powershell New-Item -ItemType Directory -Force -Path
RM := powershell Remove-Item -Recurse -Force -ErrorAction Ignore -Path 

rule_rm = powershell 'if (Test-Path "$(1)") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(1)" }'
rule_cp = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force

else
$(info Using sh)

WGET = wget
MKDIR ?= mkdir -p
RM := rm -rf

rule_rm = rm -rf $(1)
rule_cp = cp -f $(1) $(2)
endif

include src.mk

ALL_SRC := $(addprefix $(SRC_ROOT)/,$(LIBS)) replacements/curl/cacert.pem

TOOLCHAIN_INTERMEDIATE := $(abspath $(LIBS_MAKE_ROOT))/intermediate
TOOLCHAIN_TARGETS := $(abspath $(LIBS_MAKE_ROOT))/targets

prepare:
	$(call rule_rm,$(TMP_DIR))

download: $(ALL_SRC)

download: prepare

clean:
	$(call rule_rm,$(SRC_ROOT))
	$(call rule_rm,$(XWIN_ROOT))
	$(call rule_rm,$(TMP_DIR))

xwin: $(SRC_ROOT)/xwin/splat


$(TOOLCHAIN_INTERMEDIATE)/x86_64-pc-windows-msvc:
	$(MKDIR) $(TOOLCHAIN_INTERMEDIATE)
	$(MAKE) -C toolchain-windows OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-pc-windows-msvc out
	$(MAKE) -C windows x86_64 OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-pc-windows-msvc

$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc: $(TOOLCHAIN_INTERMEDIATE)/x86_64-pc-windows-msvc
	$(MAKE) -f windows.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/x86_64-pc-windows-msvc \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc

linux-glibc-x86_64: $(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu

windows-x86_64: $(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc

android.tar.xz: $(TOOLCHAIN_TARGETS)/android
	tar -cJf $@ $<

x86_64-unknown-linux-gnu.tar.xz: $(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu
	tar -cJf $@ -C $(TOOLCHAIN_TARGETS) x86_64-unknown-linux-gnu

x86_64-pc-windows-msvc.tar.xz: $(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc
	tar -cJf $@ -C $(TOOLCHAIN_TARGETS) x86_64-pc-windows-msvc

host:
	$(MAKE) -C $(HOST_TOOLCHAIN) out OUT_SYSROOT=$(abspath hosts/$(HOST_ID))

#
# Android NDK targets
#


ANDROID_INTERMEDIATE_TOOLCHAINS := $(addprefix $(TOOLCHAIN_INTERMEDIATE)/,\
	aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android)

ANDROID_TARGETS_TOOLCHAINS := $(addprefix $(TOOLCHAIN_TARGETS)/,\
	aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android)

$(ANDROID_INTERMEDIATE_TOOLCHAINS):
	$(MKDIR) $(TOOLCHAIN_INTERMEDIATE)

$(TOOLCHAIN_TARGETS)/android: $(ANDROID_INTERMEDIATE_TOOLCHAINS)
	$(MAKE) -f android.mk TOOLCHAIN_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE) TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS)
	touch $@


unknown-ndk-linux-android:
	$(MAKE) -C deps-android TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_INTERMEDIATE) SP_NDK=1 all
	$(MAKE) -f android.mk TOOLCHAIN_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE) TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS)

#
# Linux targets
#

target-x86_64-unknown-linux-gnu: host
	$(MAKE) -C target-linux-glibc out \
		OUT_HOST=../../hosts/$(HOST_ID) \
		OUT_SYSROOT=$(abspath intermediate/x86_64-unknown-linux-gnu) SP_ARCH_TARGET=x86_64
	$(MAKE) -C deps-linux x86_64 OUT_SYSROOT=$(abspath intermediate/x86_64-unknown-linux-gnu)
	$(MAKE) -f linux.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu

target-aarch64-unknown-linux-gnu: host
	$(MAKE) -C target-linux-glibc out \
		OUT_HOST=../../hosts/$(HOST_ID) \
		OUT_SYSROOT=$(abspath intermediate/aarch64-unknown-linux-gnu) SP_ARCH_TARGET=aarch64
	$(MAKE) -C deps-linux aarch64 OUT_SYSROOT=$(abspath intermediate/aarch64-unknown-linux-gnu)
	$(MAKE) -f linux.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/aarch64-unknown-linux-gnu \
		T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-unknown-linux-gnu

target-riscv64-unknown-linux-gnu: host
	$(MAKE) -C target-linux-glibc out \
		OUT_HOST=../../hosts/$(HOST_ID) \
		OUT_SYSROOT=$(abspath intermediate/riscv64-unknown-linux-gnu) SP_ARCH_TARGET=riscv64
	$(MAKE) -C deps-linux riscv64 OUT_SYSROOT=$(abspath intermediate/riscv64-unknown-linux-gnu)
	$(MAKE) -f linux.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/riscv64-unknown-linux-gnu \
		T_TARGET=$(TOOLCHAIN_TARGETS)/riscv64-unknown-linux-gnu



.PHONY: all clean prepare compress host download \
	intermediate-x86_64-unknown-linux-gnu

.DEFAULT_GOAL := host