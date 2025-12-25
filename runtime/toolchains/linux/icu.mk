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

LIBNAME = icu4c

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_AUTOCONF) \
	--disable-tests \
	--disable-samples \
	--with-data-packaging=static

ifeq ($(ARCH),e2k)

CONFIGURE += -with-cross-build=$(abspath $(MAKE_ROOT)$(LIBNAME)/native)

CONFIGURE_NATIVE := \
	CC=gcc \
	CPP="gcc -E" \
	CXX=g++ \
	CFLAGS="" \
	CXXFLAGS="" \
	CPPFLAGS="" \
	LDFLAGS="" \
	LIBS="" \
	--prefix=$(MAKE_ROOT)$(LIBNAME)/native/prefix \
	--enable-shared=no \
	--enable-static=yes

all:
	rm -rf $(LIBNAME)
	@mkdir -p $(LIBNAME)/native

	cd $(LIBNAME)/native; \
		$(LIB_SRC_DIR)/$(LIBNAME)/source/configure $(CONFIGURE_NATIVE); \
		make -j8; \
		make install

	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/source/configure $(CONFIGURE); \
		make -j8; \
		make install

	$(LIB_REPLACE)
	rm -rf $(LIBNAME)


else # ifeq ($(ARCH),e2k)


ifndef SP_NATIVE
CONFIGURE += LDFLAGS="--sysroot=$(SP_INSTALL_PREFIX) -L$(SP_INSTALL_PREFIX)/lib -lc++ -lc++abi"
LIB_REPLACE := sed -i -e 's/-lpthread -lm/-lpthread -lm -lc++ -lc++abi/g' $(SP_INSTALL_PREFIX)/lib/pkgconfig/icu-uc.pc
endif

all:
	rm -rf $(LIBNAME)
	@mkdir -p $(LIBNAME)

	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/source/configure $(CONFIGURE); \
		make -j8; \
		make install

	$(LIB_REPLACE)
	rm -rf $(LIBNAME)


endif # ifeq ($(ARCH),e2k)

.PHONY: all
