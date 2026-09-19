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

THIS_FILE := $(lastword $(MAKEFILE_LIST))

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk

# The host toolchain's clang major moves with rebuilds and can disagree with
# the pinned SP_LLVM_VER; discover the installed resource dir instead.
HOST_CLANG_RESOURCE := $(notdir $(firstword $(wildcard $(dir $(THIS_FILE))../hosts/$(HOST_ID)/lib/clang/*)))

T_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate/x86_64-unknown-linux-gnu
T_TARGET ?= $(abspath $(LIBS_MAKE_ROOT))/targets/x86_64-unknown-linux-gnu

ALL_STATIC_LIBS := $(filter-out %/libc++.a %/libc++experimental.a,\
	$(wildcard $(T_INTERMEDIATE)/usr/lib/*.a))
ALL_INSTALL_STATIC_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_STATIC_LIBS))

$(T_TARGET):
	mkdir -p $(T_TARGET)/share $(T_TARGET)/usr/lib

$(T_TARGET)/include_libc: $(T_INTERMEDIATE)/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@
	rm -rf $@/c++

$(T_TARGET)/lib: $(T_INTERMEDIATE)/lib | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	# Copy by parts: a plain `cp -rf` on the whole lib dereferences the
	# clang/include -> host symlink inside and dies on the self-intersection.
	mkdir -p $@/clang
	cp -f $(T_INTERMEDIATE)/lib/*.o $(T_INTERMEDIATE)/lib/*.so $@/
	cp -rf $(T_INTERMEDIATE)/lib/clang/lib $@/clang/lib
	# The freestanding flow compiles with -resource-dir <sysroot>/lib/clang,
	# so builtin headers (stddef.h etc) must resolve inside the sysroot.
	# Copy the full clang builtin set, then drop the libc wrappers
	# (stdint.h, inttypes.h, limits.h, tgmath.h): they use #include_next and
	# would shadow bionic's own copies in include_libc, leaving types
	# undefined. Everything else (incl. the __stddef_*/arm_* partials) stays.
	mkdir -p $@/clang/include
	cd $(dir $(THIS_FILE))../hosts/$(HOST_ID)/lib/clang/$(HOST_CLANG_RESOURCE)/include; \
		cp -f *.h $(T_TARGET)/lib/clang/include/
	rm -f $(addprefix $(T_TARGET)/lib/clang/include/,stdint.h inttypes.h limits.h tgmath.h)
	touch $@

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
	$(T_TARGET)/include_libc $(T_TARGET)/lib $(T_TARGET)/usr/include $(T_TARGET)/share/licenses $(T_TARGET)/target.mk \
	$(T_TARGET)/release \
	$(T_TARGET)

.PHONY: all
.DEFAULT_GOAL := all
