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

# Define 0 to disable automatic host building
HOST_REQUIRED ?= 1

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

$(abspath hosts/$(HOST_ID))/release:
	echo $(abspath hosts/$(HOST_ID))/release
	$(MAKE) -C $(HOST_TOOLCHAIN) out OUT_SYSROOT=$(abspath hosts/$(HOST_ID))

ifeq ($(HOST_REQUIRED),1)
host: $(abspath hosts/$(HOST_ID))/release
else
host:
endif

host-cross-x86_64-unknown-linux-gnu: target-x86_64-unknown-linux-gnu
	$(MAKE) -C host-linux-glibc/cross out OUT_SYSROOT=$(abspath hosts/x86_64-unknown-linux-gnu) SP_ARCH_HOST=x86_64

host-cross-aarch64-unknown-linux-gnu: target-aarch64-unknown-linux-gnu
	$(MAKE) -C host-linux-glibc/cross out OUT_SYSROOT=$(abspath hosts/aarch64-unknown-linux-gnu) SP_ARCH_HOST=aarch64

host-cross-riscv64-unknown-linux-gnu: target-riscv64-unknown-linux-gnu
	$(MAKE) -C host-linux-glibc/cross out OUT_SYSROOT=$(abspath hosts/riscv64-unknown-linux-gnu) SP_ARCH_HOST=riscv64

host-cross-x86_64-unknown-linux-musl: target-x86_64-unknown-linux-musl
	$(MAKE) -C host-linux-musl/cross out OUT_SYSROOT=$(abspath hosts/x86_64-unknown-linux-musl) SP_ARCH_HOST=x86_64

host-cross-aarch64-unknown-linux-musl: target-aarch64-unknown-linux-musl
	$(MAKE) -C host-linux-musl/cross out OUT_SYSROOT=$(abspath hosts/aarch64-unknown-linux-musl) SP_ARCH_HOST=aarch64

host-cross-riscv64-unknown-linux-musl: target-riscv64-unknown-linux-musl
	$(MAKE) -C host-linux-musl/cross out OUT_SYSROOT=$(abspath hosts/riscv64-unknown-linux-musl) SP_ARCH_HOST=riscv64

host-cross-x86_64-pc-windows-msvc: target-x86_64-pc-windows-msvc
	$(MAKE) -C host-windows-msvc/cross out OUT_SYSROOT=$(abspath hosts/x86_64-pc-windows-msvc) SP_ARCH_HOST=x86_64

host-cross-aarch64-pc-windows-msvc: target-aarch64-pc-windows-msvc
	$(MAKE) -C host-windows-msvc/cross out OUT_SYSROOT=$(abspath hosts/aarch64-pc-windows-msvc) SP_ARCH_HOST=aarch64

host-cross-x86_64-apple-macosx: target-x86_64-apple-macosx
	$(MAKE) -C host-macos/cross out OUT_SYSROOT=$(abspath hosts/x86_64-apple-macosx) SP_ARCH_HOST=x86_64

host-cross-aarch64-apple-macosx: target-aarch64-apple-macosx
	$(MAKE) -C host-macos/cross out OUT_SYSROOT=$(abspath hosts/aarch64-apple-macosx) SP_ARCH_HOST=aarch64

host-cross-all: $(filter-out host-cross-$(HOST_ID), \
	host-cross-x86_64-unknown-linux-gnu \
	host-cross-aarch64-unknown-linux-gnu \
	host-cross-riscv64-unknown-linux-gnu \
	host-cross-x86_64-unknown-linux-musl \
	host-cross-aarch64-unknown-linux-musl \
	host-cross-riscv64-unknown-linux-musl \
	host-cross-x86_64-pc-windows-msvc \
	host-cross-aarch64-pc-windows-msvc \
	host-cross-x86_64-apple-macosx \
	host-cross-aarch64-apple-macosx)

#
# Android targets
#

target-unknown-ndk-linux-android:
	$(MAKE) -C target-android TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) all-ndk

target-aarch64-unknown-linux-android: host
	$(MAKE) -C target-android TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) arm64-v8a

target-armv7a-unknown-linux-androideabi: host
	$(MAKE) -C target-android TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) armeabi-v7a

target-i686-unknown-linux-android: host
	$(MAKE) -C target-android TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86

target-x86_64-unknown-linux-android: host
	$(MAKE) -C target-android TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64

target-android-ndk: target-unknown-ndk-linux-android

target-android: \
	target-aarch64-unknown-linux-android \
	target-armv7a-unknown-linux-androideabi \
	target-i686-unknown-linux-android \
	target-x86_64-unknown-linux-android


#
# Linux targets (glibc + musl, роутинг по полному триплету как у target-apple)
#

target-x86_64-unknown-linux-gnu: host
	$(MAKE) -C target-linux TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-unknown-linux-gnu

target-aarch64-unknown-linux-gnu: host
	$(MAKE) -C target-linux TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-unknown-linux-gnu

target-riscv64-unknown-linux-gnu: host
	$(MAKE) -C target-linux TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) riscv64-unknown-linux-gnu

target-x86_64-unknown-linux-musl: host
	$(MAKE) -C target-linux TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-unknown-linux-musl

target-aarch64-unknown-linux-musl: host
	$(MAKE) -C target-linux TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-unknown-linux-musl

target-riscv64-unknown-linux-musl: host
	$(MAKE) -C target-linux TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) riscv64-unknown-linux-musl

target-linux: \
	target-x86_64-unknown-linux-gnu \
	target-aarch64-unknown-linux-gnu \
	target-riscv64-unknown-linux-gnu \

target-linux-musl: \
	target-x86_64-unknown-linux-musl \
	target-aarch64-unknown-linux-musl \
	target-riscv64-unknown-linux-musl \


#
# Apple targets
#

target-x86_64-apple-macosx:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-apple-macosx

target-aarch64-apple-macosx:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-macosx

target-x86_64-apple-macosx+sprt:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-apple-macosx+sprt

target-aarch64-apple-macosx+sprt:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-macosx+sprt

target-aarch64-apple-ios:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-ios

target-x86_64-apple-ios-simulator:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-apple-ios-simulator

target-aarch64-apple-ios-simulator:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-ios-simulator

target-macosx: \
	target-x86_64-apple-macosx \
	target-aarch64-apple-macosx \
	target-x86_64-apple-macosx+sprt \
	target-aarch64-apple-macosx+sprt

target-ios: \
	target-aarch64-apple-ios \
	target-x86_64-apple-ios-simulator \
	target-aarch64-apple-ios-simulator

target-apple: target-macosx target-ios


#
# Windows targets
#

target-x86_64-pc-windows-msvc: host
	$(MAKE) -C target-windows TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64

target-aarch64-pc-windows-msvc: host
	$(MAKE) -C target-windows TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64

target-windows: \
	target-x86_64-pc-windows-msvc \
	target-aarch64-pc-windows-msvc

target-all: \
	target-linux target-linux-musl \
	target-android target-android-ndk \
	target-apple \
	target-windows

clean-targets:
	rm -rf targets
	rm -rf target-windows/intermediate
	rm -rf target-linux/intermediate
	rm -rf target-apple/intermediate
	rm -rf target-android/intermediate


HOSTS_AVAILABLE = $(wildcard hosts/*/)
TARGETS_AVAILABLE = $(wildcard targets/*/)

hosts/%.tar.xz: hosts/%
	cd hosts; tar cJf $*.tar.xz $*

hosts/%.sig: hosts/%
	cd hosts; gpg --detach-sign $*

targets/%.tar.xz: targets/%
	cd targets; tar cJf $*.tar.xz $*

targets/%.sig: targets/%
	cd targets; gpg --detach-sign $*

hosts-export:\
	$(addsuffix .tar.xz,$(patsubst %/,%,$(HOSTS_AVAILABLE))) \
	$(addsuffix .tar.xz.sig,$(patsubst %/,%,$(HOSTS_AVAILABLE)))

targets-export:\
	$(addsuffix .tar.xz,$(patsubst %/,%,$(TARGETS_AVAILABLE))) \
	$(addsuffix .tar.xz.sig,$(patsubst %/,%,$(TARGETS_AVAILABLE)))

.PHONY: all clean prepare compress host download \
	hosts-export \
	targets-export \
	clean-targets \
	target-all \
	target-linux \
	target-x86_64-unknown-linux-gnu \
	target-aarch64-unknown-linux-gnu \
	target-riscv64-unknown-linux-gnu \
	target-linux-musl \
	target-x86_64-unknown-linux-musl \
	target-aarch64-unknown-linux-musl \
	target-riscv64-unknown-linux-musl \
	target-android-ndk \
	target-unknown-ndk-linux-android \
	target-android \
	target-aarch64-unknown-linux-android \
	target-armv7a-unknown-linux-androideabi \
	target-armv7a-unknown-linux-androideabi \
	target-i686-unknown-linux-android \
	target-x86_64-unknown-linux-android \
	target-apple \
	target-macosx \
	target-x86_64-apple-macosx \
	target-aarch64-apple-macosx \
	target-x86_64-apple-macosx+sprt \
	target-aarch64-apple-macosx+sprt \
	target-ios \
	target-aarch64-apple-ios \
	target-x86_64-apple-ios-simulator \
	target-aarch64-apple-ios-simulator \
	target-windows \
	target-x86_64-pc-windows-msvc \
	target-aarch64-pc-windows-msvc \
	host-cross-x86_64-unknown-linux-gnu \
	host-cross-aarch64-unknown-linux-gnu \
	host-cross-riscv64-unknown-linux-gnu \
	host-cross-x86_64-unknown-linux-musl \
	host-cross-aarch64-unknown-linux-musl \
	host-cross-riscv64-unknown-linux-musl \
	host-cross-x86_64-pc-windows-msvc \
	host-cross-aarch64-pc-windows-msvc \
	host-cross-x86_64-apple-macosx \
	host-cross-aarch64-apple-macosx \
	host-cross-all

.DEFAULT_GOAL := host
