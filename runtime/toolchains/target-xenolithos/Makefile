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

# Таргеты <arch>-xenolithos-linux-gnu — sysroot устройства Xenolith OS.
#
# Отличие от <arch>-unknown-linux-gnu (target-linux) двоякое:
#
#   1. Другая libc: glibc 2.39 вместо 2.33. Отдельный вендор в триплете нужен
#      именно поэтому — одинаковое имя при разном ABI прямо ведёт к перепутанным
#      сборкам.
#   2. Таргет несёт РАНТАЙМ устройства: bin'ы userland (busybox, dropbear,
#      vulkaninfo), лоадер Vulkan, libdrm и готовый base-rootfs, из которого
#      xenolith-os собирает образ для железа. У обычных таргетов такого слоя нет:
#      там рантайм даёт операционная система, здесь операционной системы нет.
#
# Драйвер GPU (mesa) в таргет НЕ входит: он отличается от устройства к
# устройству (broadcom на RPi4, lavapipe на QEMU, ...), поэтому собирается в
# xenolith-os под конкретную борду и накладывается оверлеем поверх base-rootfs.
# Всё нужное для этого таргет уже несёт: libdrm, expat, zlib, заголовки Vulkan
# и готовые cross-файлы (target.ini / toolchain.cmake).
#
# Сборка идёт в пять шагов на каждый триплет:
#
#   device/ (out)
#             libc устройства + компайл-тайм-слой -> intermediate/<triple>
#   ../target-linux (ветка SP_ARCH)
#             сторонние библиотеки -> intermediate/<triple>/usr
#   libc++.mk
#             разделяемые libc++/libc++abi/libunwind -> intermediate/<triple>/usr/lib
#   device/ (rootfs)
#             стейджинг рантайм-rootfs -> intermediate/<triple>/runtime
#   install-target.mk
#             раскладка -> targets/<triple>
#
# Второй шаг делегируется target-linux намеренно: набор прикладных библиотек у
# Linux-таргетов один и тот же, а та ветка Makefile libc-агностична. Дублировать
# её здесь значило бы гарантированно разъехаться при первом же добавлении
# библиотеки. libdrm с недавних пор тоже оттуда (common/libdrm.mk) — раньше
# device-слой собирал её своей копией.
#
# Третий шаг, наоборот, СВОЙ, а не ключ в target-linux/libc++.mk: разделяемый
# рантайм C++ нужен только образу устройства, и набор обычных Linux-таргетов
# из-за него меняться не должен. Подробности — в libc++.mk рядом.
#
# Четвёртый отделён от первого из-за второго и третьего: в образ едут именно те
# разделяемые библиотеки, которые появляются на шагах 2-3. Стейджить rootfs до
# них было бы нечем.

.DEFAULT_GOAL := all

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk
LIBCXX_MAKE_FILE := $(MAKE_ROOT)libc++.mk
LINUX_MAKE_ROOT := $(abspath $(MAKE_ROOT)../target-linux)

# Сборочные ключи для libc++.mk — тот же набор, который target-linux раздаёт
# своим *.mk через FORWARD_VARS. Дублируется здесь потому, что там он живёт в
# ветке SP_ARCH того Makefile и наружу не выставлен.
#
# Одно отличие намеренное: в SP_OPT стоит -fexceptions, а не -fno-exceptions.
# Это рантайм C++ образа, его потребители — лоадер Vulkan, vulkaninfo и драйвер
# GPU, то есть обычный сторонний C++ с исключениями. Через configure.mk SP_OPT
# попадает в CMAKE_CXX_FLAGS_INIT и перебивает всё, что делает cmake-ключ
# LIBCXX_ENABLE_EXCEPTIONS, поэтому менять надо именно здесь. Подробнее — в
# шапке libc++.mk.
#
# $(1) = арка. Пути к компилятору берутся из разложенного sysroot'а: host —
# симлинк на hosts/<host-id>, его кладёт device-слой первым шагом.
LIBCXX_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

define libcxx_vars =
LINUX=1 \
SP_SYSNAME=Linux \
SP_ARCH=$(1) \
SP_TARGET=$(1)-xenolithos-linux-gnu \
SP_INSTALL_PREFIX=$(2) \
SP_TOOLCHAIN_PREFIX=$(2) \
SP_TOOLCHAIN_FILE=$(2)/toolchain.cmake \
SP_CC=$(2)/host/bin/cc \
SP_CXX=$(2)/host/bin/c++ \
SP_AR=$(2)/host/bin/ar \
SP_OPT="-O3 -fPIC -DPIC -fexceptions" \
LIB_SRC_DIR=$(LIBCXX_SRC_DIR) \
MAKE_ROOT=$(MAKE_ROOT)
endef

OUT_SYSROOT ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

include $(MAKE_ROOT)../common/utils/detect-platform.mk

ARCHS := x86_64 aarch64 riscv64

# По умолчанию — арки, под которые есть реальные борды: aarch64 (RPi4 и родня)
# и x86_64 (QEMU и промышленные x86-панели). riscv64 собирается по имени.
all: aarch64-xenolithos-linux-gnu x86_64-xenolithos-linux-gnu

clean:
	$(MAKE) -C device distclean
	rm -rf $(OUT_SYSROOT)

# $(1) = арка
define xenolithos_target =
$(1)-xenolithos-linux-gnu:
	$$(MAKE) -C device out SP_ARCH_TARGET=$(1) \
		OUT_HOST=$$(abspath $$(MAKE_ROOT)../hosts/$$(HOST_ID)) \
		OUT_SYSROOT=$$(OUT_SYSROOT)/$(1)-xenolithos-linux-gnu
	$$(MAKE) -C $$(LINUX_MAKE_ROOT) SP_ARCH=$(1) \
		SP_TARGET=$(1)-xenolithos-linux-gnu \
		OUT_SYSROOT=$$(OUT_SYSROOT)/$(1)-xenolithos-linux-gnu
	$$(MAKE) -f $$(LIBCXX_MAKE_FILE) \
		$$(call libcxx_vars,$(1),$$(OUT_SYSROOT)/$(1)-xenolithos-linux-gnu)
	$$(MAKE) -C device rootfs SP_ARCH_TARGET=$(1) \
		OUT_HOST=$$(abspath $$(MAKE_ROOT)../hosts/$$(HOST_ID)) \
		OUT_SYSROOT=$$(OUT_SYSROOT)/$(1)-xenolithos-linux-gnu
	$$(MAKE) -f $$(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$$(OUT_SYSROOT)/$(1)-xenolithos-linux-gnu \
		T_TARGET=$$(TOOLCHAIN_TARGETS)/$(1)-xenolithos-linux-gnu
endef

$(foreach a,$(ARCHS),$(eval $(call xenolithos_target,$(a))))

# Провенанс источников (какое дерево выиграло для каждой плавающей зависимости).
sources:
	$(MAKE) -C device sources

.PHONY: all clean sources $(addsuffix -xenolithos-linux-gnu,$(ARCHS))
