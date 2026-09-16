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

GIT_TAG ?= $(shell git describe --tags --abbrev=0)

T_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate/x86_64-unknown-linux-gnu
T_TARGET ?= $(abspath $(LIBS_MAKE_ROOT))/targets/x86_64-unknown-linux-gnu

SPRT_TOOLCHAIN_SHARED ?= 0

# sprt.lib is dropped from a static export on purpose: a consumer of that sysroot rebuilds
# the runtime from source (runtime/runtime.mk) and would otherwise have a second, stale copy
# to link by accident. A shared export has to keep it - it is sprt.dll's import library, and
# unlike an archive it cannot be regenerated from source without the DLL.
$(T_TARGET):
	mkdir -p $(T_TARGET)/share $(T_TARGET)/usr/lib
	cp -f $(T_INTERMEDIATE)/usr/lib/*.lib $(T_TARGET)/usr/lib
ifneq ($(SPRT_TOOLCHAIN_SHARED),1)
	rm $(T_TARGET)/usr/lib/sprt.lib
endif

$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

# compiler-rt (builtins/profile/orc) is cross-built inside the target (compiler_rt.mk);
# install the built libs from the intermediate sysroot instead of copying from host.
$(T_TARGET)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib: $(T_INTERMEDIATE)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib | $(T_TARGET)
	@mkdir -p $(dir $@)
	cp -af $(T_INTERMEDIATE)/lib/clang/lib/windows/*.lib $(dir $@)

$(T_TARGET)/%: $(T_INTERMEDIATE)/% | $(T_TARGET)
	@mkdir -p $(dir $@)
	cp -af $< $@

$(T_TARGET)/share/licenses: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf ../licenses $(T_TARGET)/share

$(T_TARGET)/release: $(T_TARGET)
	echo "$(GIT_TAG)" > $@
	touch $@

all: $(ALL_INSTALL_STATIC_LIBS) \
	$(T_TARGET)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib \
	$(T_TARGET)/usr/include \
	$(T_TARGET)/share/licenses \
	$(T_TARGET)/target.mk \
	$(T_TARGET)/release \
	$(T_TARGET)

# Every image linked against this sysroot imports sprt.dll, and Windows resolves imports
# from the directory of the running image with no rpath equivalent, so the DLL has to travel
# with the export rather than stay behind in the intermediate. Both files are copied by the
# generic $(T_TARGET)/% rule above; listing them here is what asks for them.
ifeq ($(SPRT_TOOLCHAIN_SHARED),1)
all: $(T_TARGET)/usr/bin/sprt.dll $(T_TARGET)/usr/lib/sprt.lib
endif

.PHONY: all
.DEFAULT_GOAL := all

