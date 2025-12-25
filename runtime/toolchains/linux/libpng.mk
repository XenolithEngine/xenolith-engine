# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

.DEFAULT_GOAL := all

LIBNAME = libpng

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_AUTOCONF) \
	--disable-unversioned-links \
	--disable-unversioned-libpng-pc \
	--disable-unversioned-libpng-config \
	--disable-dependency-tracking

all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/configure $(CONFIGURE); \
		make -j8; \
		make install
	cp -f $(LIBNAME)/prefix/bin/libpng16-config $(SP_INSTALL_PREFIX)/bin/libpng16-config
	sed -i -e 's/libs="-lpng16"/libs="-lz -lpng16"/g' $(SP_INSTALL_PREFIX)/bin/libpng16-config
	sed -i -e 's/libdir=".*$$/libdir="lib"/g' $(SP_INSTALL_PREFIX)/bin/libpng16-config
	sed -i -e 's/includedir=".*$$/includedir="include"/g' $(SP_INSTALL_PREFIX)/bin/libpng16-config
	sed -i -e 's/I_opts=".*$$/I_opts=""/g' $(SP_INSTALL_PREFIX)/bin/libpng16-config
	sed -i -e 's/L_opts=".*$$/L_opts=""/g' $(SP_INSTALL_PREFIX)/bin/libpng16-config
	rm -rf $(LIBNAME)
	cp -f $(SP_INSTALL_PREFIX)/include/libpng16/* $(SP_INSTALL_PREFIX)/include
	cp -f $(SP_INSTALL_PREFIX)/lib/pkgconfig/libpng16.pc $(SP_INSTALL_PREFIX)/lib/pkgconfig/libpng.pc
	rm -rf $(SP_INSTALL_PREFIX)/include/libpng16
	rm -rf $(SP_INSTALL_PREFIX)/bin/png*

.PHONY: all
