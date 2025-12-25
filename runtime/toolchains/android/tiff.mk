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

LIBNAME = tiff

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DBUILD_SHARED_LIBS=OFF \
	-Dprefix=$(SP_INSTALL_PREFIX) \
	-Dtiff-static=ON \
	-Dtiff-tools=OFF \
	-Dtiff-tests=OFF \
	-Dtiff-contrib=OFF \
	-Dtiff-docs=OFF \
	-DZLIB_LIBRARY="$(SP_INSTALL_PREFIX)/lib/libz.a" \
	-DZLIB_INCLUDE_DIR="$(SP_INSTALL_PREFIX)/include" \
	-DJPEG_LIBRARY="$(SP_INSTALL_PREFIX)/lib/libjpeg.a" \
	-DJPEG_INCLUDE_DIR="$(SP_INSTALL_PREFIX)/include" \
	-DLIBLZMA_LIBRARY="$(SP_INSTALL_PREFIX)/lib/liblzma.a" \
	-DLIBLZMA_INCLUDE_DIR="$(SP_INSTALL_PREFIX)/include" \
	-DZSTD_LIBRARY="$(SP_INSTALL_PREFIX)/lib/libzstd.a" \
	-DZSTD_INCLUDE_DIR="$(SP_INSTALL_PREFIX)/include" \
	-DWebP_LIBRARY="$(SP_INSTALL_PREFIX)/lib/libwebp.a" \
	-DWebP_INCLUDE_DIR="$(SP_INSTALL_PREFIX)/include"

all:
	@rm -rf $(LIBNAME)
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		cmake $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE);
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	rm -rf $(LIBNAME)

.PHONY: all
