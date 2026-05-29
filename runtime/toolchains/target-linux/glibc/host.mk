# Copyright (c) 2026 Xenolith Team <admin@xenloth.studio>
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

HOST_SYSROOT := sysroot-host
HOST_GCC_CC := $(HOST_SYSROOT)/bin/gcc
HOST_GCC_CXX := $(HOST_SYSROOT)/bin/g++
HOST_GCC_MAKE := $(HOST_SYSROOT)/bin/make
HOST_GCC_GLIBC := $(HOST_SYSROOT)/lib/libc.so.6

TARGET_AS := $(HOST_SYSROOT)/bin/as

# Отдельно отметим синдром Туретта у GCC, под который необходимо подстроится, дублировав выходной путь с помощью
#
# mkdir -p $(STAGE0_SYSROOT)$(abspath $(STAGE0_SYSROOT))
# cd $(STAGE0_SYSROOT)$(abspath $(STAGE0_SYSROOT)); ln -s $(abspath $(STAGE0_SYSROOT))/lib lib
#
# Мы подставляем ссылку на реальное положение библиотек в локацию, которую ожидает GCC

$(HOST_SYSROOT)/sysroot: $(LINUX_HOST_HEADERS_DIR)
	mkdir -p $(HOST_SYSROOT)/etc
	mkdir -p $(HOST_SYSROOT)/usr
	mkdir -p $(HOST_SYSROOT)/lib
	mkdir -p $(HOST_SYSROOT)$(abspath $(HOST_SYSROOT))
	cp -rL $(LINUX_HOST_HEADERS_DIR)/include $(HOST_SYSROOT)/include
	cd $(HOST_SYSROOT); rm lib64; ln -s lib lib64
	cd $(HOST_SYSROOT)/usr; rm include; ln -s ../include include
	cd $(HOST_SYSROOT)/usr; rm lib; ln -s ../lib lib
	cd $(HOST_SYSROOT)$(abspath $(HOST_SYSROOT)); rm lib; ln -s $(abspath $(HOST_SYSROOT))/lib lib
	touch $(HOST_SYSROOT)/sysroot

$(HOST_GCC_MAKE): $(HOST_SYSROOT)/sysroot $(MAKE_SRC_DIR)
	rm -rf build/host-make
	mkdir -p build/host-make
	cd build/host-make; $(abspath $(MAKE_SRC_DIR))/configure --prefix $(abspath $(HOST_SYSROOT))
	make -C build/host-make $(SP_NJOBS)
	make -C build/host-make install

$(HOST_GCC_GLIBC): $(HOST_GCC_MAKE) $(GLIBC_SRC_DIR)
	rm -rf build/host-glibc
	mkdir -p build/host-glibc
	cd build/host-glibc; $(abspath $(GLIBC_SRC_DIR))/configure \
		--prefix $(abspath $(HOST_SYSROOT)) \
		--with-headers=$(abspath $(HOST_SYSROOT))/include \
		--enable-kernel=$(LINUX_KERNEL_VER) \
		--disable-werror \
		--without-selinux \
		--disable-timezone-tools \
		--enable-multi-arch \
		CFLAGS="-O3 -fPIC" \
		CC=gcc \
		CXX=gcc
	$(HOST_GCC_MAKE) -C build/host-glibc $(SP_NJOBS)
	$(HOST_GCC_MAKE) -C build/host-glibc install

#	cp -rf replacements/$(notdir $(GLIBC_MIN_SRC_DIR))/include/* $(HOST_SYSROOT)/include

$(HOST_GCC_CC): $(HOST_GCC_MAKE)
	rm -rf build/host-gcc
	mkdir -p build/host-gcc
	ln -f -s ../gmp  $(abspath $(GCC_SRC_DIR))/gmp
	ln -f -s ../mpfr  $(abspath $(GCC_SRC_DIR))/mpfr
	ln -f -s ../mpc  $(abspath $(GCC_SRC_DIR))/mpc
	ln -f -s ../isl  $(abspath $(GCC_SRC_DIR))/isl
	ln -f -s ../gettext  $(abspath $(GCC_SRC_DIR))/gettext
	cd build/host-gcc; $(abspath $(GCC_SRC_DIR))/configure \
		--prefix $(abspath $(HOST_SYSROOT)) \
		--libdir $(abspath $(HOST_SYSROOT))/lib \
		--disable-multilib \
		--disable-libsanitizer \
		--disable-bootstrap \
		--disable-werror \
		--enable-languages=c,c++ \
		LDFLAGS=""
	make -C build/host-gcc $(SP_NJOBS)
	make -C build/host-gcc install

host: $(HOST_GCC_CC) # $(HOST_GCC_GLIBC)
