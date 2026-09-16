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

LIBNAME = libjpeg-turbo

include ../common/configure.mk

# WITH_TOOLS=0 skips the command-line tools (cjpeg/djpeg/jpegtran/rdjpgcom/wrjpgcom)
# — only the static lib is ever shipped. It also avoids linking those executables,
# which the SDK-free "+open" sysroot cannot satisfy (its curated libSystem stub omits
# tool-only symbols like _printf/___strcpy_chk that the libraries themselves never use).
CONFIGURE := \
	$(CONFIGURE_CMAKE) -DENABLE_SHARED=FALSE -DWITH_TURBOJPEG=FALSE \
	-DWITH_TOOLS=0 -DWITH_TESTS=Off -DWITH_CRT_DLL=1

ifeq ($(SP_ARCH),riscv64)
CONFIGURE += -DWITH_SIMD=Off
endif

ifeq ($(SP_ARCH),armv7a)
CONFIGURE += -DWITH_SIMD=Off
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	@# Build only the static library target (with its SIMD object deps), NOT the whole
	@# project: the default `all` also builds simd/simdcoverage — a SIMD-coverage helper
	@# executable (if(WITH_SIMD AND ENABLE_STATIC)) that WITH_TOOLS cannot disable and
	@# that pulls _printf, which the +open sysroot's curated libSystem stub omits. The
	@# separate `cmake --install` step never rebuilds, and simdcoverage has no install
	@# rule, so only libjpeg.a + headers land in the sysroot (SIMD kept).
	cd $(LIBNAME); cmake --build . --config Release --target jpeg-static --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))
	$(if $(WINDOWS),$(call rule_mv,$(SP_INSTALL_PREFIX)/usr/lib/jpeg-static.lib,$(SP_INSTALL_PREFIX)/usr/lib/jpeg.lib))
	$(if $(WINDOWS),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/cmake/libjpeg-turbo/libjpeg-turboTargets-release.cmake))
