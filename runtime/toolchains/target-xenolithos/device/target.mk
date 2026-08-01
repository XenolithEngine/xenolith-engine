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

# Кросс-сборка целевой libc: binutils -> минимальный gcc -> glibc 2.39.
#
# GCC здесь нужен ИСКЛЮЧИТЕЛЬНО чтобы поднять glibc. Как только libc собрана,
# всё остальное (compiler-rt, libc++, libdrm, vulkan, mesa, userland) собирает
# clang движка из hosts/<host-triple> — см. libs.mk.

TARGET_SYSROOT  := sysroot-target-$(SP_TARGET_TRIPLE)
TARGET_AS       := $(TARGET_SYSROOT)/bin/$(SP_TARGET_TRIPLE)-as
TARGET_GCC_MIN  := $(TARGET_SYSROOT)/bin/$(SP_TARGET_TRIPLE)-gcc
TARGET_GLIBC    := $(TARGET_SYSROOT)/lib/libc.so.6

TARGET_PATH     := $(abspath $(TARGET_SYSROOT)/bin)

# ── скелет sysroot + UAPI ядра целевой арки ───────────────────────────────────
# usr/include -> ../include и usr/lib -> ../lib: glibc ставится с --prefix=/usr,
# и симлинки сводят её вывод в те же include/ и lib/, где уже лежит UAPI ядра.
$(TARGET_SYSROOT)/sysroot: $(LINUX_TARGET_HEADERS_DIR)
	mkdir -p $(TARGET_SYSROOT)/etc
	mkdir -p $(TARGET_SYSROOT)/usr
	mkdir -p $(TARGET_SYSROOT)/lib
	cp -rL $(LINUX_TARGET_HEADERS_DIR)/include $(TARGET_SYSROOT)/include
	cd $(TARGET_SYSROOT); ln -sf lib lib64
	cd $(TARGET_SYSROOT)/usr; ln -sf ../include include; ln -sf ../lib lib
	touch $(TARGET_SYSROOT)/sysroot

# ── binutils для целевого триплета ────────────────────────────────────────────
$(TARGET_AS): $(TARGET_SYSROOT)/sysroot | $(HOST_GCC_CC) $(BINUTILS_SRC_DIR)
	rm -rf $(TBUILD)/binutils
	mkdir -p $(TBUILD)/binutils
	cd $(TBUILD)/binutils; $(BINUTILS_SRC_DIR)/configure \
		--target=$(SP_TARGET_TRIPLE) \
		--prefix $(abspath $(TARGET_SYSROOT)) \
		--disable-nls --disable-werror \
		CC=$(abspath $(HOST_GCC_CC)) \
		CXX=$(abspath $(HOST_GCC_CXX))
	make -C $(TBUILD)/binutils $(SP_NJOBS)
	make -C $(TBUILD)/binutils install

# ── минимальный целевой gcc (только C, freestanding) ──────────────────────────
# Ровно столько, сколько нужно для компиляции glibc: язык C, без потоков, без
# shared, без заголовков. Дальше эстафету принимает clang.
$(TARGET_GCC_MIN): $(TARGET_AS) $(TARGET_SYSROOT)/sysroot | $(HOST_GCC_CC) $(GCC_SRC_DIR)
	rm -rf $(TBUILD)/gcc
	mkdir -p $(TBUILD)/gcc
	cd $(TBUILD)/gcc; PATH=$(TARGET_PATH):$$PATH $(GCC_SRC_DIR)/configure \
		--prefix $(abspath $(TARGET_SYSROOT)) \
		--libdir $(abspath $(TARGET_SYSROOT))/lib \
		--disable-nls \
		--disable-multilib \
		--disable-libsanitizer \
		--disable-threads \
		--disable-shared \
		--enable-languages=c \
		--without-headers \
		--target=$(SP_TARGET_TRIPLE) \
		CFLAGS="-O3 -fPIC" \
		CXXFLAGS="-O3 -fPIC" \
		CC=$(abspath $(HOST_GCC_CC)) \
		CXX=$(abspath $(HOST_GCC_CXX))
	make -C $(TBUILD)/gcc all-gcc $(SP_NJOBS)
	make -C $(TBUILD)/gcc all-target-libgcc $(SP_NJOBS)
	make -C $(TBUILD)/gcc install-gcc
	make -C $(TBUILD)/gcc install-target-libgcc

# ── glibc 2.39 ────────────────────────────────────────────────────────────────
# Рантайм устройства. --prefix=/usr (а не путь дерева сборки) обязателен: иначе
# ld.so и линкер-скрипты уносят абсолютные пути сборочной машины на устройство.
#
# Два флага, о которых стоит знать:
#   --disable-sanity-checks  здесь ОБЯЗАТЕЛЕН. Проверка configure линкует тестовую
#     программу, но целевой libc ещё нет (её и собираем), а $(TARGET_GCC_MIN) —
#     это компилятор --without-headers --disable-shared. Штатная практика для
#     first-stage cross-glibc.
#   mathvec НАМЕРЕННО не отключается. libmvec входит в дефолтный набор арки на
#     x86_64 и (с 2.39) на aarch64, поэтому бинари Ubuntu 24.04 могут нести
#     NEEDED на libmvec.so.1 — принудительное отключение развело бы ABI с той
#     самой дистрибутивной glibc, под которую этот sysroot и делается.
#
# --libdir=/usr/lib + libc_cv_slibdir=/lib задают ЕДИНУЮ раскладку на всех
# арках. Без них при --prefix=/usr glibc выбирает каталоги сама и разводит арки:
# x86_64/aarch64 -> slibdir=/lib64, libdir=/usr/lib64, а riscv64 вообще уходит в
# /lib64/lp64d и /usr/lib64/lp64d (sysdeps/unix/sysv/linux/riscv/configure).
# Стейджинг таргета кладёт всё плоско в lib/, и подстраиваться под три разных
# арх-специфичных раскладки значило бы размножить арх-условия по всему файлу.
# libc_cv_slibdir — кэш-переменная configure: арх-фрагмент выставляет slibdir
# только если она пуста, так что это штатный способ его переопределить.
# Так же поступает target-linux (там --libdir <sysroot>/lib), и его x86_64-таргет
# на этой раскладке работает.
$(TARGET_GLIBC): $(TARGET_GCC_MIN) $(TARGET_SYSROOT)/sysroot $(GLIBC_SRC_DIR)
	rm -rf $(TBUILD)/glibc
	mkdir -p $(TBUILD)/glibc
	cd $(TBUILD)/glibc; PATH=$(TARGET_PATH):$$PATH $(abspath $(GLIBC_SRC_DIR))/configure \
		--prefix=/usr \
		--libdir=/usr/lib \
		libc_cv_slibdir=/lib \
		libc_cv_rtlddir=/lib \
		--with-headers=$(abspath $(TARGET_SYSROOT))/include \
		--with-sysroot=$(abspath $(TARGET_SYSROOT)) \
		--with-binutils=$(TARGET_PATH) \
		--disable-werror \
		--without-selinux \
		--disable-timezone-tools \
		--disable-multilib \
		--disable-sanity-checks \
		--disable-static-nss \
		--disable-nss-crypt \
		--disable-build-nscd \
		--disable-nscd \
		--host=$(SP_TARGET_TRIPLE) \
		--target=$(SP_TARGET_TRIPLE) \
		CFLAGS="-Os -fPIC" \
		CXXFLAGS="-Os -fPIC" \
		CC="$(abspath $(TARGET_GCC_MIN)) -fPIC" \
		CXX="$(abspath $(TARGET_GCC_MIN)) -fPIC"
	$(HOST_GCC_MAKE) -C $(TBUILD)/glibc $(SP_NJOBS)
	$(HOST_GCC_MAKE) -C $(TBUILD)/glibc install DESTDIR=$(abspath $(TARGET_SYSROOT))
# Корневой lib64 обязан указывать на lib: на x86_64 интерпретатор, вшитый в
# бинари, — /lib64/ld-linux-x86-64.so.2, а загрузчик у нас лежит в lib/. Без
# этого симлинка линковка внутри sysroot (busybox, dropbear, драйвер mesa)
# сломалась бы именно на x86_64.
	cd $(TARGET_SYSROOT); rm -f lib64; ln -s lib lib64

target: $(TARGET_AS) $(TARGET_GCC_MIN) $(TARGET_GLIBC)

.PHONY: target
