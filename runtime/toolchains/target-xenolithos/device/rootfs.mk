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

# Стейджинг рантайм-rootfs — то, ради чего этот таргет отличается от target-linux.
#
# Компайл-тайм-часть таргета (lib/, include/, usr/) — это sysroot с заголовками,
# статическими библиотеками и неотрезанной отладочной информацией. Этот слой
# производит то, что реально едет на железо: бинари userland плюс транзитивное
# замыкание разделяемых библиотек, стрипнутое.
#
# Результат — переиспользуемый base-rootfs НА АРКУ: без приложения, без борды и
# БЕЗ ДРАЙВЕРА GPU. Драйвер (mesa) отличается от устройства к устройству
# (broadcom на RPi4, lavapipe на QEMU, ...), поэтому собирается в xenolith-os
# под конкретную борду и накладывается оверлеем поверх этого дерева — вместе с
# inittab, fstab, init-скриптами борды, слотом приложения и хешем пароля.
#
# Всё, что нужно для сборки драйвера снаружи, таргет уже несёт: libdrm, expat,
# zlib, заголовки Vulkan и cross-файлы (target.ini / toolchain.cmake).
#
# Логика сборки живёт в mkrootfs.sh: обход замыкания плюс правила стейджинга не
# укладываются в make-рецепт читаемо.
#
# ВАЖНО: этот слой — ПРЕДПОСЛЕДНИЙ шаг сборки таргета, а не часть `make out`.
# Разделяемые библиотеки, которые едут в образ, появляются позже device-слоя:
# libdrm — из общего набора сторонних библиотек Linux-таргетов (шаг 2),
# libc++/libc++abi/libunwind — из ../libc++.mk (шаг 3). Поэтому
# target-xenolithos/Makefile вызывает `make -C device rootfs` отдельно, четвёртым
# шагом. Если запустить его раньше, mkrootfs.sh честно упадёт со списком
# MISSING — молча неполного образа не получится.

OUT_RUNTIME := $(OUT_SYSROOT)/runtime
OUT_ROOTFS  := $(abspath $(OUT_RUNTIME)/rootfs)

# Второй источник библиотек для обхода замыкания: usr/lib разложенного sysroot'а,
# куда шаги 2-3 кладут libdrm и разделяемые libc++/libc++abi/libunwind. Ищется
# ПЕРВЫМ — в device-sysroot лежит своя сборочная копия того же libc++, и в образ
# должна ехать именно та, что собрана для образа.
OUT_EXTRA_LIBDIR := $(abspath $(OUT_SYSROOT))/usr/lib

# То, что этот каталог даёт образу. Правил на них здесь нет — их производят шаги
# 2-3, — поэтому список идёт через $(wildcard): если файлы на месте, они честные
# предпосылки и пересборка libc++ на шаге 3 перекладывает rootfs; если их ещё
# нет, wildcard даёт пусто, make не ругается на отсутствующее правило, а нехватку
# ловит mkrootfs.sh со списком MISSING.
#
# Без этого была тихая устарелость: пересобрал разделяемую libc++ — в образе
# осталась предыдущая, потому что все предпосылки ниже относятся к device-слою и
# не менялись.
OUT_EXTRA_LIBS := $(wildcard \
	$(OUT_EXTRA_LIBDIR)/libc++.so.1 \
	$(OUT_EXTRA_LIBDIR)/libc++abi.so.1 \
	$(OUT_EXTRA_LIBDIR)/libunwind.so.1 \
	$(OUT_EXTRA_LIBDIR)/libdrm.so.2)

MKROOTFS := $(MAKE_ROOT)mkrootfs.sh

# Локали, которые компилируются в base-rootfs. Имя вида <input>.<charmap>, как в
# localedata/SUPPORTED.
#
# По умолчанию ровно одна: C.UTF-8 (~410 КБ) — она нужна, чтобы многобайтные
# функции libc вообще работали на UTF-8. Встроены в glibc только C/POSIX, а
# C.UTF-8 с 2.35 — обычная локаль и требует скомпилированных данных; без них
# setlocale(LC_ALL,"C.UTF-8") возвращает NULL, и mbstowcs/wcstombs молча ведут
# себя как в C.
#
# Языковые локали здесь НЕ место: их набор — продуктовая политика, и весят они
# заметно. Их докладывает сборщик образа, из той же usr/share/i18n таргета.
#
# Компилирует их целевой localedef (формат бинарный и завязан на версию glibc):
# напрямую, когда арка совпадает с хостовой, иначе через qemu-user — то есть для
# кросс-арок в зависимостях хоста появляется qemu-user. Отказаться явно:
# SP_LOCALES= (пусто).
SP_LOCALES ?= C.UTF-8

# Зависит от всей рантайм-поверхности: бинарей userland, libc, лоадера Vulkan и
# шима libgcc_s из этого каталога плюс разделяемых библиотек шагов 2-3 (см.
# OUT_EXTRA_LIBS выше).
$(OUT_ROOTFS)/rootfs.manifest: $(TARGET_BUSYBOX) $(TARGET_DROPBEAR) $(TARGET_VULKANINFO) \
                               $(TARGET_VULKAN_LOADER) $(TARGET_LIBCXX) \
                               $(TARGET_LIBXCRYPT) $(TARGET_LIBGCC_S) $(MKROOTFS) \
                               $(OUT_EXTRA_LIBS) $(TARGET_GLIBC)
	$(MKROOTFS) \
		$(SYSROOT_ABS) \
		$(abspath $(USERLAND_SYSROOT)) \
		$(OUT_ROOTFS) \
		$(abspath $(HOST_LLVM_READELF)) \
		$(abspath $(HOST_LLVM_STRIP)) \
		$(SP_ARCH_TARGET) \
		$(TARGET_LIBGCC_S) \
		$(OUT_EXTRA_LIBDIR) \
		"$(SP_LOCALES)"

rootfs: $(OUT_ROOTFS)/rootfs.manifest

.PHONY: rootfs
