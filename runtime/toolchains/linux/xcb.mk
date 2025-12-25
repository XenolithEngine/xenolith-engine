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

LIBRAME_XCB_PROTO = xcb-proto
LIBRAME_XORGPROTO = xorgproto
LIBRAME_XAU = libXau

LIBNAME = libxcb

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_AUTOCONF) \
	--disable-dependency-tracking

$(SP_INSTALL_PREFIX)/lib/pkgconfig/xcb-proto.pc: $(lastword $(MAKEFILE_LIST))


$(SP_INSTALL_PREFIX)/include/X11/Xatom.h: $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(LIBNAME)
	@rm -rf $(LIBRAME_XORGPROTO)
	@mkdir -p $(LIBRAME_XORGPROTO)
	cd $(LIBRAME_XORGPROTO); \
		$(LIB_SRC_DIR)/$(LIBRAME_XORGPROTO)/configure $(CONFIGURE); \
		make -j8; \
		make install
	@rm -rf $(LIBRAME_XORGPROTO)

$(SP_INSTALL_PREFIX)/lib/libXau.a: $(SP_INSTALL_PREFIX)/include/X11/Xatom.h $(SP_INSTALL_PREFIX)/lib/pkgconfig/xcb-proto.pc $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(LIBNAME)
	@rm -rf $(LIBRAME_XAU)
	@mkdir -p $(LIBRAME_XAU)
	cd $(LIBRAME_XAU); \
		$(LIB_SRC_DIR)/$(LIBRAME_XAU)/configure $(CONFIGURE); \
		make -j8; \
		make install
	@rm -rf $(LIBRAME_XAU)

all: $(SP_INSTALL_PREFIX)/include/X11 $(SP_INSTALL_PREFIX)/lib/pkgconfig/xcb-proto.pc $(SP_INSTALL_PREFIX)/lib/libXau.a $(lastword $(MAKEFILE_LIST))
	@rm -rf $(LIBNAME)
	@mkdir -p $(LIBNAME)

	@rm -rf $(LIBNAME)/xcbproto
	@mkdir -p $(LIBNAME)/xcbproto
	cd $(LIBNAME)/xcbproto; \
		$(LIB_SRC_DIR)/xcb-proto/configure $(CONFIGURE); \
		make -j8; \
		make install
	@rm -rf $(LIBNAME)/xcbproto
	cp $(LIBNAME)/prefix/share/pkgconfig/* $(SP_INSTALL_PREFIX)/lib/pkgconfig

	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/configure $(CONFIGURE); \
		make -j8; \
		make install
	sed -i -e 's/ -lxcb/ -lxcb -lXau/g' $(SP_INSTALL_PREFIX)/lib/pkgconfig/xcb.pc

	@mkdir -p $(LIBNAME)/errors
	cd $(LIBNAME)/errors; \
		$(LIB_SRC_DIR)/xcb-util-errors/configure $(CONFIGURE); \
		make -j8; \
		make install
	@rm -rf $(LIBNAME)
	rm -rf $(SP_INSTALL_PREFIX)/local

.PHONY: all
