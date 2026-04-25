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

# Detect host

ifeq ($(findstring Windows,$(OS)),Windows)

UNAME := Windows

HOST_ID := x86_64-pc-windows-msvc
WINARCH := $(shell [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture)

ifeq ($(WINARCH),X64)
HOST_ARCH := x86_64
HOST_ID := x86_64-pc-windows-msvc
else ifeq ($(WINARCH),X86)
HOST_ARCH := x86
HOST_ANDROID := windows-x86
else ifeq ($(WINARCH),Arm64)
HOST_ARCH := aarch64
HOST_ANDROID := windows-aarch64
else ifeq ($(WINARCH),Arm)
HOST_ARCH := armv7
HOST_ID := armv7-pc-windows-msvc
endif # ($(WINARCH),X64)

HOST_ANDROID := windows-$(HOST_ARCH)
HOST_TOOLCHAIN := host-windows-msvc

else # ($(findstring Windows,$(OS)),Windows)

UNAME := $(shell uname)
HOST_ARCH := $(shell uname -m)

ifeq ($(UNAME),Darwin)

ifeq ($(HOST_ARCH),arm64)
HOST_ID := aarch64-apple-macosx
else
HOST_ID := $(HOST_ARCH)-apple-macosx
endif
HOST_TOOLCHAIN := host-macosx
HOST_ANDROID := darwin-$(HOST_ARCH)

else ifeq ($(UNAME),Linux)

HOST_ANDROID := linux-$(HOST_ARCH)

ifeq ($(shell ldd /bin/ls 2>&1 | grep -q 'musl'),)
HOST_ID := $(HOST_ARCH)-unknown-linux-gnu
HOST_TOOLCHAIN := host-linux-glibc
else
HOST_ID := $(HOST_ARCH)-unknown-linux-musl
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
