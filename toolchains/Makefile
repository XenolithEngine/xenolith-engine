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

GIT_TAG ?=v0-alpha3
GIT_RELEASE_SOURCE := https://github.com/libstappler/stappler-toolchains/releases/download/$(GIT_TAG)


ANDROID_ARCHS := \
	aarch64-linux-android \
	armv7a-linux-androideabi \
	i686-linux-android \
	x86_64-linux-android

ANDROID_TARGETS := \
	armeabi-v7a \
	arm64-v8a \
	x86 \
	x86_64

LINUX_ARCHS := \
	x86_64-unknown-linux-gnu

targets/android/armeabi-v7a: targets/armv7a-linux-androideabi
	mkdir -p targets/android
	cd targets/android; ln -s ../armv7a-linux-androideabi armeabi-v7a

targets/android/arm64-v8a: targets/aarch64-linux-android
	mkdir -p targets/android
	cd targets/android; ln -s ../aarch64-linux-android arm64-v8a

targets/android/x86: targets/i686-linux-android
	mkdir -p targets/android
	cd targets/android; ln -s ../i686-linux-android x86

targets/android/x86_64: targets/x86_64-linux-android
	mkdir -p targets/android
	cd targets/android; ln -s ../x86_64-linux-android x86_64

targets/%.tar.xz:
	wget $(GIT_RELEASE_SOURCE)/$(notdir $@) -O $@

targets/%: targets/%.tar.xz
	cd targets; tar -xf $(notdir $<)

linux: $(addprefix targets/,$(LINUX_ARCHS))

android: $(addprefix targets/,$(ANDROID_ARCHS)) $(addprefix targets/android/,$(ANDROID_TARGETS))

all: linux android

.PHONY: all android linux
