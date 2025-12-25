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

LIBNAME = sqlite

include ../common/configure.mk

all:
	@mkdir -p $(SP_INSTALL_PREFIX)/lib $(SP_INSTALL_PREFIX)/include
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(SP_CC) $(SP_OPT) -c -o sqlite3.o $(LIB_SRC_DIR)/$(LIBNAME)/sqlite3.c; \
		$(SP_AR) rcs libsqlite3.a sqlite3.o
	mv -f $(LIBNAME)/libsqlite3.a $(SP_INSTALL_PREFIX)/lib/
	rm -rf $(LIBNAME)
	cp -f $(LIB_SRC_DIR)/$(LIBNAME)/*.h $(SP_INSTALL_PREFIX)/include/
