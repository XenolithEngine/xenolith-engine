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

LIBNAME = libpng

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DPNG_SHARED=OFF \
	-DPNG_TARGET_ARCHITECTURE=$(SP_ARCH) \
	-DPNG_TESTS=OFF \
	-DPNG_TOOLS=OFF

ifdef WINDOWS
CONFIGURE += -DCYGWIN=1
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Release --target install --parallel
	$(call rule_rm,$(LIBNAME))
	$(if $(WINDOWS),$(call rule_mv,$(SP_INSTALL_PREFIX)/usr/lib/libpng16_static.lib,$(SP_INSTALL_PREFIX)/usr/lib/png16.lib))
	$(if $(WINDOWS),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/libpng.lib))
	@# On the Generic (baremetal) cmake system used for wasm and NuttX, libpng
	@# uses the Windows-ish static naming (liblibpng16_static.a) and may install
	@# headers under libpng16/. Normalise to the Unix layout the app links
	@# against: libpng16.a + png.h/pngconf.h/pnglibconf.h in usr/include
	@# (mirrors the linux target). The NuttX Makefile stamps on libpng16.a —
	@# without this rename every resume rebuilds png forever.
	@# Drop the stale name-referencing cmake package so find_package(PNG)
	@# resolves via FindPNG -> find_library(png16).
	$(if $(or $(WASM),$(NUTTX),$(EMBOX)),$(call rule_mv,$(SP_INSTALL_PREFIX)/usr/lib/liblibpng16_static.a,$(SP_INSTALL_PREFIX)/usr/lib/libpng16.a))
	$(if $(or $(WASM),$(NUTTX),$(EMBOX)),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/libpng.a))
	$(if $(or $(WASM),$(NUTTX),$(EMBOX)),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/libpng))
	$(if $(or $(WASM),$(NUTTX),$(EMBOX)),$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/include/libpng16/png.h,$(SP_INSTALL_PREFIX)/usr/include/png.h))
	$(if $(or $(WASM),$(NUTTX),$(EMBOX)),$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/include/libpng16/pngconf.h,$(SP_INSTALL_PREFIX)/usr/include/pngconf.h))
	$(if $(or $(WASM),$(NUTTX),$(EMBOX)),$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/include/libpng16/pnglibconf.h,$(SP_INSTALL_PREFIX)/usr/include/pnglibconf.h))

.PHONY: all
