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

.DEFAULT_GOAL := all

LIBNAME = brotli

ifdef WINDOWS
SP_USER_CFLAGS += -D__SPRT_WINDOWS_PROTECTED
SP_USER_CXXFLAGS += -D__SPRT_WINDOWS_PROTECTED
endif

include ../common/configure.mk

# Freestanding wasm cannot link the `brotli` CLI executable / test harness
# (-nostdlib, no libc archive at dep-build time), so build the libraries only.
# Other targets link the tool against their sprt/libc and keep building it.
BROTLI_CONFIGURE := $(CONFIGURE_CMAKE)
ifdef WASM
BROTLI_CONFIGURE += -DBROTLI_BUILD_TOOLS=OFF -DBROTLI_DISABLE_TESTS=ON
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(BROTLI_CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Release --target install --parallel
	$(if $(LINUX),sed -i -e 's/ -lbrotlidec/ -lbrotlidec -lbrotlicommon/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libbrotlidec.pc)
	$(if $(LINUX),sed -i -e 's/ -lbrotlienc/ -lbrotlienc -lbrotlicommon/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libbrotlienc.pc)
	$(if $(ANDROID),sed -i -e 's/ -lbrotlidec/ -lbrotlidec -lbrotlicommon/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libbrotlidec.pc)
	$(if $(ANDROID),sed -i -e 's/ -lbrotlienc/ -lbrotlienc -lbrotlicommon/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libbrotlienc.pc)
	$(call rule_rm,$(LIBNAME))

.PHONY: all
