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

LIBNAME = openssl

include ../common/configure.mk

OPENSSL_TARGET := linux-x86_64-clang

ifeq ($(SP_ARCH),aarch64)
OPENSSL_TARGET := linux-aarch64
endif

ifeq ($(SP_ARCH),riscv64)
OPENSSL_TARGET := linux64-riscv64
endif

ifeq ($(SP_ARCH),loongarch64)
OPENSSL_TARGET := linux64-loongarch64
endif

# e2k32 — ILP32 (long=4): дефолтный linux-x86_64-clang даёт SIXTY_F64_BIT_LONG
# (BN_ULONG = 8-байтный unsigned long) — компилировалось бы, но математика
# bignum была бы тихо неверна. generic32 даёт THIRTY_TWO_BIT и ставит пакеты
# сразу в usr/lib (без пост-переноса lib64→lib ниже).
ifeq ($(SP_ARCH),e2k32)
OPENSSL_TARGET := linux-generic32
endif

ifdef SP_TOOLCHAIN_PREFIX
export CFLAGS=$(SP_CFLAGS)
endif

CONFIGURE := $(OPENSSL_TARGET) \
	--prefix=$(SP_INSTALL_PREFIX)/usr \
	CC=$(SP_CC) \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	no-tests \
	no-module \
	no-legacy \
	no-srtp \
	no-srp \
	no-dso \
	no-filenames \
	no-shared \
	no-autoload-config

# lcc (все режимы e2k) не собирает ассемблерные части openssl (perlasm и
# .S рассчитаны на GNU as с x86-синтаксисом) — только portable C. Экспортируемый
# CFLAGS openssl 3.x игнорирует, позиционные аргументы Configure — нет, поэтому
# флаг режима (-m32/-m128; для e2k SP_TARGET_FLAGS — ровно он) едет только здесь.
ifneq (,$(filter $(SP_ARCH),e2k32 e2k64 e2k128))
CONFIGURE += no-asm -mno-sse4.2 $(SP_TARGET_FLAGS)
endif

ifeq ($(DEBUG),1)
CONFIGURE += -d
endif

# e2k64/e2k128 собираются дефолтным linux-x86_64-clang (no-asm выше) — install
# тоже идёт в usr/lib64, поэтому пост-перенос общий с x86_64. e2k32 (generic32)
# ставит сразу в usr/lib и попадает в ветку else.
ifneq (,$(filter $(SP_ARCH),x86_64 e2k64 e2k128))
all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	rm -rf $(LIBNAME)
	mv -f $(SP_INSTALL_PREFIX)/usr/lib64/libssl.a $(SP_INSTALL_PREFIX)/usr/lib/libssl.a 
	mv -f $(SP_INSTALL_PREFIX)/usr/lib64/libcrypto.a $(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a 
	mv -f $(SP_INSTALL_PREFIX)/usr/lib64/pkgconfig/libssl.pc $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc
	mv -f $(SP_INSTALL_PREFIX)/usr/lib64/pkgconfig/libcrypto.pc $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libcrypto.pc
	rm -rf $(SP_INSTALL_PREFIX)/usr/lib64 $(SP_INSTALL_PREFIX)/bin/c_rehash
	sed -i -e 's/ -lssl/ -lssl -lpthread/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc
	cp $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/openssl.pc
	sed -i -e 's/ -lssl/ -lssl -lcrypto/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/openssl.pc
	sed -i -e 's/{exec_prefix}\/lib64/{exec_prefix}\/lib/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc
else
all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	rm -rf $(LIBNAME)
	rm -rf $(SP_INSTALL_PREFIX)/bin/c_rehash
	sed -i -e 's/ -lssl/ -lssl -lpthread/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc
endif

.PHONY: all
