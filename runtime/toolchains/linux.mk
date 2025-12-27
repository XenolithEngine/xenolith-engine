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

T_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate/x86_64-unknown-linux-gnu
T_TARGET ?= $(abspath $(LIBS_MAKE_ROOT))/targets/x86_64-unknown-linux-gnu

EXCLUDE_BIN := \
	openssl \
	libpng16-config \
	spirv-lesspipe.sh

ALL_BIN := $(filter-out $(addprefix %/,$(EXCLUDE_BIN)),$(wildcard $(T_INTERMEDIATE)/bin/*))
ALL_STATIC_LIBS := $(wildcard $(T_INTERMEDIATE)/lib/*.a)
ALL_SHARED_LIBS := $(wildcard $(T_INTERMEDIATE)/lib/*.so) $(wildcard $(T_INTERMEDIATE)/lib/*.so.*)

ALL_INSTALL_BIN := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_BIN))
ALL_INSTALL_STATIC_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_STATIC_LIBS))
ALL_INSTALL_SHARED_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_SHARED_LIBS))

$(T_TARGET):
	mkdir -p $(T_TARGET)/include $(T_TARGET)/include_libc $(T_TARGET)/bin $(T_TARGET)/lib $(T_TARGET)/share

$(T_TARGET)/include: $(T_INTERMEDIATE)/include $(T_TARGET)
	cp -rf $< $@

$(T_TARGET)/include_libc: $(T_INTERMEDIATE)/include_libc $(T_TARGET)
	cp -rf $< $@

$(T_TARGET)/lib/clang: $(T_INTERMEDIATE)/lib/clang $(T_TARGET)
	cp -rf $< $@

$(T_TARGET)/%: $(T_INTERMEDIATE)/% $(T_TARGET)
	cp -af $< $@

all: $(ALL_INSTALL_BIN) $(ALL_INSTALL_STATIC_LIBS) $(ALL_INSTALL_SHARED_LIBS) \
	$(T_TARGET)/include $(T_TARGET)/include_libc $(T_TARGET)/lib/clang  $(T_TARGET)/toolchain.mk
	cp -rf licenses $(T_TARGET)/share

.PHONY: all
.DEFAULT_GOAL := all

