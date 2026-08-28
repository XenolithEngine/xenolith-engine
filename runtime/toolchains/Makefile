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

SHELL := powershell.exe
MKDIR ?= powershell New-Item -ItemType Directory -Force -Path
RM := powershell Remove-Item -Recurse -Force -ErrorAction Ignore -Path 

rule_rm = powershell 'if (Test-Path "$(1)") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(1)" }'
rule_cp = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force

else
$(info Using sh)

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

host-cross-x86_64-pc-windows-msvc: target-x86_64-pc-windows-msvc+dll
	$(MAKE) -C host-windows-msvc/cross out OUT_SYSROOT=$(abspath hosts/x86_64-pc-windows-msvc) SP_ARCH_HOST=x86_64

host-cross-aarch64-pc-windows-msvc: target-aarch64-pc-windows-msvc+dll
	$(MAKE) -C host-windows-msvc/cross out OUT_SYSROOT=$(abspath hosts/aarch64-pc-windows-msvc) SP_ARCH_HOST=aarch64

host-cross-x86_64-apple-macosx: target-x86_64-apple-macosx+open
	$(MAKE) -C host-macos/cross out OUT_SYSROOT=$(abspath hosts/x86_64-apple-macosx) SP_ARCH_HOST=x86_64

host-cross-aarch64-apple-macosx: target-aarch64-apple-macosx+open
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
# Xenolith OS device targets
#
# Отдельный вендор в триплете обязателен: libc здесь другая (glibc 2.39 против
# 2.33 у -unknown-linux-gnu), а одинаковое имя при разном ABI ведёт прямо к
# перепутанным сборкам. Кроме sysroot таргет несёт рантайм устройства
# (base-rootfs с userland), из которого xenolith-os собирает образ для железа.
#

target-x86_64-xenolithos-linux-gnu: host
	$(MAKE) -C target-xenolithos TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-xenolithos-linux-gnu

target-aarch64-xenolithos-linux-gnu: host
	$(MAKE) -C target-xenolithos TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-xenolithos-linux-gnu

target-riscv64-xenolithos-linux-gnu: host
	$(MAKE) -C target-xenolithos TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) riscv64-xenolithos-linux-gnu

# riscv64 собирается по имени: реальных борд под него пока нет.
target-xenolithos: \
	target-aarch64-xenolithos-linux-gnu \
	target-x86_64-xenolithos-linux-gnu \
	target-riscv64-xenolithos-linux-gnu

#
# Apple targets
#

target-x86_64-apple-macosx:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-apple-macosx

target-aarch64-apple-macosx:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-macosx

target-x86_64-apple-macosx+open:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-apple-macosx+open

target-aarch64-apple-macosx+open:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-macosx+open

target-aarch64-apple-ios:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-ios

target-x86_64-apple-ios-simulator:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64-apple-ios-simulator

target-aarch64-apple-ios-simulator:
	$(MAKE) -C target-apple TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64-apple-ios-simulator

target-macosx: \
	target-x86_64-apple-macosx \
	target-aarch64-apple-macosx

# Xcode-SDK-free macOS sysroots (Apple open-source headers mixed in). Kept out of
# the default `target-macosx` aggregate for now — build explicitly by name.
target-macosx-open: \
	target-x86_64-apple-macosx+open \
	target-aarch64-apple-macosx+open

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

# Sysroots built around sprt.dll instead of the static runtime archive. Separate targets,
# not a flag: the two write the same file names with incompatible content, and the sprt.lib
# rule does not depend on the flag, so switching modes over an up-to-date sysroot rebuilds
# nothing and silently hands out the previous mode's artifact. They carry the runtime layer
# only - see the "+dll" recipes in target-windows/Makefile. Kept out of `target-windows`,
# the way target-macosx-open is kept out of target-macosx.
target-x86_64-pc-windows-msvc+dll: host
	$(MAKE) -C target-windows TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) x86_64+dll

target-aarch64-pc-windows-msvc+dll: host
	$(MAKE) -C target-windows TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) aarch64+dll

target-windows-dll: \
	target-x86_64-pc-windows-msvc+dll \
	target-aarch64-pc-windows-msvc+dll

target-wasm32-unknown-unknown: host
	$(MAKE) -C target-wasm TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) wasm32

target-wasm: target-wasm32-unknown-unknown

target-aarch64-nuttx-none-elf: host
	$(MAKE) -C target-nuttx TOOLCHAIN_TARGETS=$(TOOLCHAIN_TARGETS) target-aarch64

target-nuttx: target-aarch64-nuttx-none-elf

target-all: \
	target-linux target-linux-musl \
	target-xenolithos \
	target-android target-android-ndk \
	target-apple \
	target-windows target-windows-dll \
	target-wasm \
	target-nuttx

clean-targets:
	rm -rf targets
	rm -rf target-windows/intermediate
	rm -rf target-linux/intermediate
	rm -rf target-xenolithos/intermediate
	rm -rf target-apple/intermediate
	rm -rf target-android/intermediate
	rm -rf target-nuttx/intermediate


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

#
# GitHub release export
#
# Assets of a GitHub release live in one flat namespace, while a host package and
# a target package for the same triplet are both called <triplet>.tar.xz - upload
# both and the second one overwrites the first. The sets below stage prefixed
# copies under release/, so the asset name says which half of the SDK it is:
#
#   release/host_x86_64-unknown-linux-gnu.tar.xz[.sig]
#   release/target_x86_64-unknown-linux-gnu.tar.xz[.sig]
#
# The sets are explicit lists rather than $(wildcard): a release has to fail
# loudly when a package was not built, and targets/ also holds artifacts that are
# deliberately not published - the +dll Windows sysroots, which exist only to
# build the Windows host toolchain from.
#
# Run `make release-check` first: it reports missing packages and packages whose
# release tag does not match the current one, without building anything.

GIT_TAG ?= $(shell git describe --tags --abbrev=0)

RELEASE_DIR := release

RELEASE_HOSTS := \
	x86_64-unknown-linux-gnu \
	aarch64-unknown-linux-gnu \
	riscv64-unknown-linux-gnu \
	x86_64-unknown-linux-musl \
	aarch64-unknown-linux-musl \
	riscv64-unknown-linux-musl \
	x86_64-pc-windows-msvc \
	aarch64-pc-windows-msvc \
	x86_64-apple-macosx \
	aarch64-apple-macosx

RELEASE_TARGETS := \
	x86_64-unknown-linux-gnu \
	aarch64-unknown-linux-gnu \
	riscv64-unknown-linux-gnu \
	x86_64-unknown-linux-musl \
	aarch64-unknown-linux-musl \
	riscv64-unknown-linux-musl \
	x86_64-xenolithos-linux-gnu \
	aarch64-xenolithos-linux-gnu \
	riscv64-xenolithos-linux-gnu \
	aarch64-unknown-linux-android \
	armv7a-unknown-linux-androideabi \
	i686-unknown-linux-android \
	x86_64-unknown-linux-android \
	unknown-ndk-linux-android \
	x86_64-apple-macosx \
	aarch64-apple-macosx \
	x86_64-apple-macosx+open \
	aarch64-apple-macosx+open \
	aarch64-apple-ios \
	aarch64-apple-ios-simulator \
	x86_64-apple-ios-simulator \
	x86_64-pc-windows-msvc \
	aarch64-pc-windows-msvc \
	wasm32-unknown-unknown

RELEASE_HOST_FILES := \
	$(addprefix $(RELEASE_DIR)/host_,$(addsuffix .tar.xz,$(RELEASE_HOSTS))) \
	$(addprefix $(RELEASE_DIR)/host_,$(addsuffix .tar.xz.sig,$(RELEASE_HOSTS)))

RELEASE_TARGET_FILES := \
	$(addprefix $(RELEASE_DIR)/target_,$(addsuffix .tar.xz,$(RELEASE_TARGETS))) \
	$(addprefix $(RELEASE_DIR)/target_,$(addsuffix .tar.xz.sig,$(RELEASE_TARGETS)))

$(RELEASE_DIR):
	$(MKDIR) $(RELEASE_DIR)

# The prerequisite is the package's `release` stamp, not the directory: a
# directory's mtime only moves when an entry is added or removed, so a rebuilt
# package whose files merely changed in place would not re-trigger the tar.
$(RELEASE_DIR)/host_%.tar.xz: hosts/%/release | $(RELEASE_DIR)
	tar cJf $@ -C hosts $*

$(RELEASE_DIR)/target_%.tar.xz: targets/%/release | $(RELEASE_DIR)
	tar cJf $@ -C targets $*

# gpg refuses to overwrite an existing signature in a non-interactive run, so a
# re-export has to drop the old one first.
$(RELEASE_DIR)/%.tar.xz.sig: $(RELEASE_DIR)/%.tar.xz
	rm -f $@
	gpg --detach-sign --output $@ $<

release-hosts: $(RELEASE_HOST_FILES)

release-targets: $(RELEASE_TARGET_FILES)

release-export: release-hosts release-targets

# Read-only pre-flight: what the release set expects vs what is actually on disk.
release-check:
	@missing=''; stale=''; \
	for h in $(RELEASE_HOSTS); do \
		if [ ! -f hosts/$$h/release ]; then missing="$$missing hosts/$$h"; \
		elif [ "`cat hosts/$$h/release`" != '$(GIT_TAG)' ]; then \
			stale="$$stale hosts/$$h(`cat hosts/$$h/release`)"; fi; \
	done; \
	for t in $(RELEASE_TARGETS); do \
		if [ ! -f targets/$$t/release ]; then missing="$$missing targets/$$t"; \
		elif [ "`cat targets/$$t/release`" != '$(GIT_TAG)' ]; then \
			stale="$$stale targets/$$t(`cat targets/$$t/release`)"; fi; \
	done; \
	echo "release tag: $(GIT_TAG)"; \
	echo "expected: $(words $(RELEASE_HOSTS)) hosts, $(words $(RELEASE_TARGETS)) targets"; \
	if [ -n "$$missing" ]; then echo "NOT BUILT:$$missing"; fi; \
	if [ -n "$$stale" ]; then echo "WRONG TAG:$$stale"; fi; \
	if [ -z "$$missing$$stale" ]; then echo "all packages present"; else exit 1; fi

clean-release:
	$(call rule_rm,$(RELEASE_DIR))

.PHONY: all clean prepare compress host download \
	hosts-export \
	targets-export \
	release-hosts \
	release-targets \
	release-export \
	release-check \
	clean-release \
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
	target-xenolithos \
	target-x86_64-xenolithos-linux-gnu \
	target-aarch64-xenolithos-linux-gnu \
	target-riscv64-xenolithos-linux-gnu \
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
	target-macosx-open \
	target-x86_64-apple-macosx+open \
	target-aarch64-apple-macosx+open \
	target-ios \
	target-aarch64-apple-ios \
	target-x86_64-apple-ios-simulator \
	target-aarch64-apple-ios-simulator \
	target-windows \
	target-x86_64-pc-windows-msvc \
	target-aarch64-pc-windows-msvc \
	target-windows-dll \
	target-x86_64-pc-windows-msvc+dll \
	target-aarch64-pc-windows-msvc+dll \
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
