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

# Detect host

ifeq ($(findstring Windows,$(OS)),Windows)

UNAME := Windows

HOST_ID := x86_64-pc-windows-msvc
WINARCH := $(shell [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture)

ifeq ($(WINARCH),X64)
HOST_ID := x86_64-pc-windows-msvc
else ifeq ($(WINARCH),X86)
HOST_ID := x86-pc-windows-msvc
else ifeq ($(WINARCH),Arm64)
HOST_ID := aarch64-pc-windows-msvc
else ifeq ($(WINARCH),Arm)
HOST_ID := armv7-pc-windows-msvc
endif # ($(WINARCH),X64)

HOST_TOOLCHAIN := host-windows-msvc

else # ($(findstring Windows,$(OS)),Windows)

UNAME := $(shell uname)

ifeq ($(UNAME),Darwin)

HOST_ID := $(shell uname -m)-apple-macosx
HOST_TOOLCHAIN := host-macosx

else ifeq ($(UNAME),Linux)

ifeq ($(shell ldd /bin/ls 2>&1 | grep -q 'musl'),)
HOST_ID := $(shell uname -m)-unknown-linux-gnu
HOST_TOOLCHAIN := host-linux-glibc
else
HOST_ID := $(shell uname -m)-unknown-linux-musl
HOST_TOOLCHAIN := host-linux-musl
endif # MUSL

# Native android host will be implemented later
#
#else ifeq ($(UNAME),Android)
#
#HOST_ID := $(shell uname -m)-unknown-linux-android
#
else

$(error Unknown host)

endif # $(UNAME)

endif # ($(findstring Windows,$(OS)),Windows)


THIS_FILE := $(lastword $(MAKEFILE_LIST))
LIBS_MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
LIBS_MAKE_ROOT := $(dir $(LIBS_MAKE_FILE))

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

ANDROID_INTERMEDIATE_TOOLCHAINS := $(addprefix $(TOOLCHAIN_INTERMEDIATE)/,\
	aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android)

ANDROID_TARGETS_TOOLCHAINS := $(addprefix $(TOOLCHAIN_TARGETS)/,\
	aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android)

$(ANDROID_INTERMEDIATE_TOOLCHAINS):
	$(MKDIR) $(TOOLCHAIN_INTERMEDIATE)
	$(MAKE) -C android TOOLCHAIN_OUTPUT_DIR=$(abspath $(LIBS_MAKE_ROOT))/intermediate all

$(TOOLCHAIN_TARGETS)/android: $(ANDROID_INTERMEDIATE_TOOLCHAINS)
	$(MAKE) -f android.mk TOOLCHAIN_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE) TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS)
	touch $@

$(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu/toolchain:
	$(MKDIR) $(TOOLCHAIN_INTERMEDIATE)
	$(MAKE) -C toolchain-linux-glibc OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-unknown-linux-gnu out
	touch $@

$(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu/deps: $(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu/toolchain
	$(MAKE) -C linux x86_64 OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-unknown-linux-gnu
	touch $@

$(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu: $(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu/deps
	$(MAKE) -f linux.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/x86_64-unknown-linux-gnu \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-unknown-linux-gnu

$(TOOLCHAIN_INTERMEDIATE)/x86_64-pc-windows-msvc:
	$(MKDIR) $(TOOLCHAIN_INTERMEDIATE)
	$(MAKE) -C toolchain-windows OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-pc-windows-msvc out
	$(MAKE) -C windows x86_64 OUT_SYSROOT=$(LIBS_MAKE_ROOT)/intermediate/x86_64-pc-windows-msvc

$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc: $(TOOLCHAIN_INTERMEDIATE)/x86_64-pc-windows-msvc
	$(MAKE) -f windows.mk T_INTERMEDIATE=$(TOOLCHAIN_INTERMEDIATE)/x86_64-pc-windows-msvc \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc

android: $(TOOLCHAIN_TARGETS)/android

android-clean:
	$(RM) $(ANDROID_INTERMEDIATE_TOOLCHAINS) $(TOOLCHAIN_TARGETS)/android

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

intermediate-x86_64-unknown-linux-gnu: host
	$(MAKE) -C target-linux-glibc out \
		OUT_HOST=../../hosts/$(HOST_ID) \
		OUT_SYSROOT=$(abspath intermediate/x86_64-unknown-linux-gnu) SP_ARCH_TARGET=x86_64
	$(MAKE) -C deps-linux x86_64 OUT_SYSROOT=$(abspath intermediate/x86_64-unknown-linux-gnu)


compress: \
	android.tar.xz \
	x86_64-unknown-linux-gnu.tar.xz \
	x86_64-pc-windows-msvc

.PHONY: all clean prepare compress host download \
	intermediate-x86_64-unknown-linux-gnu

.DEFAULT_GOAL := host