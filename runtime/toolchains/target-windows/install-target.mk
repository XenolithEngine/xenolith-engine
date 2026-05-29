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

T_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate/x86_64-unknown-linux-gnu
T_TARGET ?= $(abspath $(LIBS_MAKE_ROOT))/targets/x86_64-unknown-linux-gnu

$(T_TARGET):
	mkdir -p $(T_TARGET)/share $(T_TARGET)/usr/lib
	cp -f $(T_INTERMEDIATE)/usr/lib/*.lib $(T_TARGET)/usr/lib

$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

# We need compiler builtins from precompiled host files, we can not compile it with target
$(T_TARGET)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib:  | $(T_TARGET)
	@mkdir -p $(dir $@)
	cp -af ../hosts/$(SP_ARCH_TARGET_CLANG)/lib/clang/21/lib/windows/*.lib $(dir $@)

$(T_TARGET)/%: $(T_INTERMEDIATE)/% | $(T_TARGET)
	@mkdir -p $(dir $@)
	cp -af $< $@

$(T_TARGET)/share/licenses: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf ../licenses $(T_TARGET)/share

all: $(ALL_INSTALL_STATIC_LIBS) \
	$(T_TARGET)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib \
	$(T_TARGET)/usr/include \
	$(T_TARGET)/share/licenses \
	$(T_TARGET)/target.mk \
	$(T_TARGET)

.PHONY: all
.DEFAULT_GOAL := all

