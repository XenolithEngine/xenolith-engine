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

SHELL := powershell.exe

T_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate/x86_64-unknown-linux-gnu
T_TARGET ?= $(abspath $(LIBS_MAKE_ROOT))/targets/x86_64-unknown-linux-gnu

EXCLUDE_BIN := \
	openssl.exe

EXCLUDE_LIB := \
	SPIRV-Tools-diff.lib \
	SPIRV-Tools-link.lib \
	SPIRV-Tools-lint.lib \
	SPIRV-Tools-opt.lib \
	SPIRV-Tools-reduce.lib \
	SPIRV-Tools-shared.lib \
	SPIRV-Tools.lib

ALL_BIN := $(filter-out $(addprefix %/,$(EXCLUDE_BIN)),$(wildcard $(T_INTERMEDIATE)/bin/*.exe) $(wildcard $(T_INTERMEDIATE)/bin/*.dll))
ALL_OBJ_LIBS := $(wildcard $(T_INTERMEDIATE)/lib/*.o) $(wildcard $(T_INTERMEDIATE)/lib/*.json)
ALL_STATIC_LIBS := $(filter-out $(addprefix %/,$(EXCLUDE_LIB)),$(wildcard $(T_INTERMEDIATE)/lib/*.lib)) \
	$(wildcard $(T_INTERMEDIATE)/lib/*.pdb)

ALL_INSTALL_BIN := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_BIN))
ALL_INSTALL_OBJ_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_OBJ_LIBS))
ALL_INSTALL_STATIC_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_STATIC_LIBS))

$(T_TARGET):
	powershell New-Item -ItemType Directory -Force -Path $(T_TARGET)/bin
	powershell New-Item -ItemType Directory -Force -Path $(T_TARGET)/lib
	powershell New-Item -ItemType Directory -Force -Path $(T_TARGET)/share

$(T_TARGET)/include: $(T_INTERMEDIATE)/include $(T_TARGET)
	powershell Copy-Item -Path "$<" -Destination "$@" -Force -Recurse

$(T_TARGET)/include_libc: $(T_INTERMEDIATE)/include_libc $(T_TARGET)
	powershell Copy-Item -Path "$<" -Destination "$@" -Force -Recurse

$(T_TARGET)/lib/clang: $(T_INTERMEDIATE)/lib/clang $(T_TARGET)
	powershell Copy-Item -Path "$<" -Destination "$@" -Force -Recurse

$(T_TARGET)/%: $(T_INTERMEDIATE)/% $(T_TARGET)
	powershell Copy-Item -Path "$<" -Destination "$@" -Force

all: $(ALL_INSTALL_BIN) $(ALL_INSTALL_STATIC_LIBS) $(ALL_INSTALL_SHARED_LIBS) $(ALL_INSTALL_OBJ_LIBS) \
	$(T_TARGET)/include $(T_TARGET)/lib/clang  $(T_TARGET)/toolchain.mk
	powershell Copy-Item -Path "licenses" -Destination "$(T_TARGET)/share" -Force -Recurse

.PHONY: all
.DEFAULT_GOAL := all
