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

GIT_TAG ?= sdk-v0alpha0

include ../runtime/toolchains/common/utils/detect-platform.mk
include ../runtime/toolchains/common/utils/init-shell.mk

GIT_RELEASE_SOURCE := ftp://stappler.dev/releases/$(GIT_TAG)


ifeq ($(findstring Windows,$(OS)),Windows)
SHELL = powershell.exe
wget_rule = Invoke-WebRequest -Uri "$(1)" -OutFile "$2"
else
wget_rule = wget $1 -O $2
endif


hosts/%.tar.xz:
	@$(call rule_mkdir,hosts)
	$(call wget_rule,$(GIT_RELEASE_SOURCE)/hosts/$(notdir $@),$@)

hosts/%: hosts/%.tar.xz
	$(call rule_rm,$@)
	cd hosts; tar -xf $(notdir $<)

targets/%.tar.xz:
	@$(call rule_mkdir,targets)
	$(call wget_rule,$(GIT_RELEASE_SOURCE)/targets/$(notdir $@),$@)

targets/%: targets/%.tar.xz
	$(call rule_rm,$@)
	cd targets; tar -xf $(notdir $<)

host: hosts/$(HOST_ID)

target: targets/$(HOST_ID)

all: host target

clean:
	@$(call rule_rm,targets)
	@$(call rule_rm,hosts)

.PHONY: all host target clean


#
# Android targets
#

target-unknown-ndk-linux-android: targets/unknown-ndk-linux-android
target-aarch64-unknown-linux-android: targets/aarch64-unknown-linux-android
target-armv7a-unknown-linux-androideabi: targets/armv7a-unknown-linux-androideabi
target-i686-unknown-linux-android: targets/i686-unknown-linux-android
target-x86_64-unknown-linux-android: targets/x86_64-unknown-linux-android

target-android-ndk: target-unknown-ndk-linux-android

target-android: \
	target-aarch64-unknown-linux-android \
	target-armv7a-unknown-linux-androideabi \
	target-i686-unknown-linux-android \
	target-x86_64-unknown-linux-android


#
# Linux targets
#

target-x86_64-unknown-linux-gnu: targets/x86_64-unknown-linux-gnu
target-aarch64-unknown-linux-gnu: targets/aarch64-unknown-linux-gnu
target-riscv64-unknown-linux-gnu: targets/riscv64-unknown-linux-gnu

target-linux: \
	target-x86_64-unknown-linux-gnu \
	target-aarch64-unknown-linux-gnu \
	target-riscv64-unknown-linux-gnu \


#
# Xenolith OS device targets
#
# Помимо обычного sysroot несут рантайм устройства (runtime/rootfs), из которого
# xenolith-os собирает образ для железа.
#

target-x86_64-xenolithos-linux-gnu: targets/x86_64-xenolithos-linux-gnu
target-aarch64-xenolithos-linux-gnu: targets/aarch64-xenolithos-linux-gnu
target-riscv64-xenolithos-linux-gnu: targets/riscv64-xenolithos-linux-gnu

target-xenolithos: \
	target-aarch64-xenolithos-linux-gnu \
	target-x86_64-xenolithos-linux-gnu \


#
# Apple targets
#

target-x86_64-apple-macosx: targets/x86_64-apple-macosx
target-aarch64-apple-macosx: targets/aarch64-apple-macosx
target-x86_64-apple-macosx+sprt: targets/x86_64-apple-macosx+sprt
target-aarch64-apple-macosx+sprt: targets/aarch64-apple-macosx+sprt
target-aarch64-apple-ios: targets/aarch64-apple-ios
target-x86_64-apple-ios-simulator: targets/x86_64-apple-ios-simulator
target-aarch64-apple-ios-simulator: targets/aarch64-apple-ios-simulator

target-macosx: \
	target-x86_64-apple-macosx \
	target-aarch64-apple-macosx \
	target-x86_64-apple-macosx+sprt \
	target-aarch64-apple-macosx+sprt

target-ios: \
	target-aarch64-apple-ios \
	target-x86_64-apple-ios-simulator \
	target-aarch64-apple-ios-simulator

target-apple: target-macosx # target-ios


#
# Windows targets
#

target-x86_64-pc-windows-msvc: targets/x86_64-pc-windows-msvc
target-aarch64-pc-windows-msvc: targets/aarch64-pc-windows-msvc

# Sysroots built around sprt.dll instead of the static runtime archive.
target-x86_64-pc-windows-msvc+dll: targets/x86_64-pc-windows-msvc+dll
target-aarch64-pc-windows-msvc+dll: targets/aarch64-pc-windows-msvc+dll

target-windows: \
	target-x86_64-pc-windows-msvc \
#	target-aarch64-pc-windows-msvc

target-windows-dll: \
	target-x86_64-pc-windows-msvc+dll \
#	target-aarch64-pc-windows-msvc+dll

.PHONY:\
	target-x86_64-unknown-linux-gnu \
	target-aarch64-unknown-linux-gnu \
	target-riscv64-unknown-linux-gnu \
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
	target-x86_64-apple-macosx+sprt \
	target-aarch64-apple-macosx+sprt \
	target-ios \
	target-aarch64-apple-ios \
	target-x86_64-apple-ios-simulator \
	target-aarch64-apple-ios-simulator \
	target-windows \
	target-x86_64-pc-windows-msvc \
	target-aarch64-pc-windows-msvc \
	target-windows-dll \
	target-x86_64-pc-windows-msvc+dll \
	target-aarch64-pc-windows-msvc+dll
