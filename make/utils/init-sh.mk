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

# Проверяем хостовую систему, у Darwin нет опции -o для uname
UNAME := $(shell uname)

SH := 1

GLOBAL_SHELL := sh

GLOBAL_RM ?= rm -f
GLOBAL_CP ?= cp -f
GLOBAL_MAKE ?= $(MAKE)
GLOBAL_MKDIR ?= mkdir -p
GLOBAL_AR ?= ar rcs
GLOBAL_ECHO ?= echo

shell_mkdir = $(shell $(GLOBAL_MKDIR) $(1))

rule_mkdir = $(GLOBAL_MKDIR) $(1)

shell_override_file = \
	$(shell echo '$(2)' > $(1))

shell_append_file = \
	$(shell echo '$(2)' >> $(1))

shell_cat = \
	$(shell cat $(1) 2> /dev/null)

shell_arith = $(shell echo $$($(1)) )

STAPPLER_HOST_ARCH ?= $(shell uname -m)

ifeq ($(STAPPLER_HOST_ARCH),arm64)
STAPPLER_HOST_ARCH := aarch64
endif

ifeq ($(UNAME),Darwin)

ANDROID_HOST := darwin-$(STAPPLER_HOST_ARCH)

STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-apple-macosx

else ifeq ($(UNAME),Linux)

ANDROID_HOST := linux-$(STAPPLER_HOST_ARCH)

ifeq ($(shell ldd /bin/ls 2>&1 | grep -q 'musl'),)
STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-unknown-linux-gnu
else
STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-unknown-linux-musl
endif # MUSL

else

$(error Unknown host OS)

endif

$(call print_verbose,(init-sh.mk) UNAME: $(UNAME))
$(call print_verbose,(init-sh.mk) STAPPLER_HOST_ARCH: $(STAPPLER_HOST_ARCH))
$(call print_verbose,(init-sh.mk) STAPPLER_HOST: $(STAPPLER_HOST))
$(call print_verbose,(init-sh.mk) ANDROID_HOST: $(ANDROID_HOST))
