# Copyright (c) 2026 Xenloith Team <admin@xenolith.studio>
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

$(call print_verbose,(init-sh.mk) Init with sh)

" := "

UNAME = $(XL_UNAME_SYSNAME)

GLOBAL_SHELL := xlmake

GLOBAL_AR ?= ar rcs
GLOBAL_ECHO ?= $(ECHO)

WRITE_START = $(WRITE) $$@
WRITE_END =
APPEND_START = $(APPEND) $$@
APPEND_END =

rule_rm = $(REMOVE) $(1)
rule_cp = $(CP) $(1) $(2)
rule_mkdir = $(MKDIR) $(1)
rule_write = $(WRITE) $(2) $(1)

shell_arith = 

shell_mkdir = $(xl_mkdir $(1))
shell_override_file = $(xl_write $(1),$(2))
shell_append_file = $(xl_append $(1),$(2))
shell_cat = $(xl_cat $(1))

STAPPLER_HOST_ARCH ?= $(XL_UNAME_MACHINE)

ifeq ($(STAPPLER_HOST_ARCH),aarch64)
ANDROID_DISTRIB_ARCH := arm64
else
ANDROID_DISTRIB_ARCH := $(STAPPLER_HOST_ARCH)
endif

ifeq ($(UNAME),Darwin)

ANDROID_HOST := darwin-$(ANDROID_DISTRIB_ARCH)

STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-apple-macosx

else ifeq ($(UNAME),Linux)

ANDROID_HOST := linux-$(ANDROID_DISTRIB_ARCH)

ifdef XL_GLIBC_VERSION # exact glibc
STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-unknown-linux-gnu
else # assume musl
STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-unknown-linux-musl
endif

else ifeq ($(UNAME),Windows)

ANDROID_HOST := windows-$(ANDROID_DISTRIB_ARCH)

STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-pc-windows-msvc

else ifeq ($(UNAME),WASM)

# xlmake itself is the compiler driver: clang.wasm runs in a host Web Worker.
STAPPLER_HOST_ARCH ?= wasm32
STAPPLER_HOST ?= wasm32-unknown-unknown

else

$(error Unknown host OS)

endif

$(call print_verbose,(init-sh.mk) UNAME: $(UNAME))
$(call print_verbose,(init-sh.mk) STAPPLER_HOST_ARCH: $(STAPPLER_HOST_ARCH))
$(call print_verbose,(init-sh.mk) STAPPLER_HOST: $(STAPPLER_HOST))
$(call print_verbose,(init-sh.mk) ANDROID_HOST: $(ANDROID_HOST))
