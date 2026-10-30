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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# libFuzzer on the client->server decoders of the remote protocol: what a display server parses
# out of an untrusted client (TAU-P-Gpu-6e). Not part of tests/run-checks.py: xenolith-tau's
# wm-fuzz builds and runs it with WM_FUZZ_TIME.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := wmfuzz

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk
LOCAL_MODULES := xenolith_remote xenolith_font

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))
LOCAL_MAIN := fuzz.cpp

# One sanitizer per flag: the build splits flag lists on commas. The toolchain ships no libFuzzer
# runtime, so the driver flag would also inject a -lc++ the host build has no library for; the
# system's fuzzer archive is freestanding and links as it is.
LOCAL_CFLAGS += -O1 -g -fsanitize=fuzzer-no-link -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=undefined
LOCAL_CXXFLAGS += -O1 -g -fsanitize=fuzzer-no-link -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=undefined
LOCAL_LDFLAGS += /usr/lib/clang/23/lib/linux/libclang_rt.fuzzer-x86_64.a -fsanitize=address -fsanitize=undefined

APPCONFIG_APP_NAME := WmFuzz
APPCONFIG_BUNDLE_NAME := org.stappler.WmFuzz

include $(STAPPLER_BUILD_ROOT)/universal.mk
