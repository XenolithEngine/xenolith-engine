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

# Раскладка intermediate/<triple> -> targets/<triple>.
#
# По сравнению с target-linux/install-target.mk добавлено три вещи, и все три —
# ради того, чтобы xenolith-os могла работать против ГОТОВОГО таргета, а не
# против дерева сборки:
#
#   usr/lib/*.so*        libdrm/libvulkan/libcrypt плюс разделяемые
#                        libc++/libc++abi/libunwind — линковка драйвера mesa и
#                        девайсовых утилит;
#   usr/lib/pkgconfig    mesa ищет libdrm через pkg-config, без .pc сборка
#                        драйвера не сконфигурируется вовсе;
#   runtime/             base-rootfs устройства (bin/sbin/lib/usr/etc) — то, что
#                        сборщик образа распаковывает и накладывает поверх свою
#                        борд-специфику и драйвер GPU.

GIT_TAG ?= $(shell git describe --tags --abbrev=0)

INSTALL_MAKE_ROOT := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

# Заголовки X11/xcb/dbus/xkbcommon, которые Linux-таргеты кладут в usr/include,
# лежат в target-linux — сторонние библиотеки для этого таргета собирает он же.
LINUX_MAKE_ROOT := $(abspath $(INSTALL_MAKE_ROOT)../target-linux)

T_INTERMEDIATE ?= $(INSTALL_MAKE_ROOT)intermediate/aarch64-xenolithos-linux-gnu
T_TARGET ?= $(INSTALL_MAKE_ROOT)targets/aarch64-xenolithos-linux-gnu

# libc++.a исключается: движок несёт собственный портированный libcxx и её
# присутствие увело бы линковку не туда (так же, как в target-linux).
ALL_STATIC_LIBS := $(filter-out %/libc++.a %/libc++experimental.a,\
	$(wildcard $(T_INTERMEDIATE)/usr/lib/*.a))
ALL_INSTALL_STATIC_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_STATIC_LIBS))

ALL_SHARED_LIBS := $(wildcard $(T_INTERMEDIATE)/usr/lib/*.so*)
ALL_INSTALL_SHARED_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_SHARED_LIBS))

$(T_TARGET):
	mkdir -p $(T_TARGET)/share $(T_TARGET)/usr/lib

$(T_TARGET)/include_libc: $(T_INTERMEDIATE)/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@
	cp -rf $(LINUX_MAKE_ROOT)/runtime/include/* $@
	rm -rf $@/c++

# Сторонние библиотеки собираются с CMAKE_INSTALL_PREFIX/--prefix, равным пути
# intermediate, и записывают его в .pc абсолютом (`libdir=<intermediate>/usr/lib`).
# Потребитель запускает pkg-config с PKG_CONFIG_SYSROOT_DIR=<таргет>, который
# дописывает свой префикс сверху — получается путь из двух склеенных корней.
# Срезаем корень дерева сборки, оставляя sysroot-относительные /usr/lib и
# /usr/include: именно их PKG_CONFIG_SYSROOT_DIR и должен префиксовать.
$(T_TARGET)/usr/lib/pkgconfig: $(T_INTERMEDIATE)/usr/lib/pkgconfig | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@
	sed -i 's#$(T_INTERMEDIATE)##g' $@/*.pc

$(T_TARGET)/lib: $(T_INTERMEDIATE)/lib | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@
	rm -rf $@/clang/include
	touch $@

# Рантайм-слой устройства. Копируется -a: внутри есть симлинки (ферма апплетов
# busybox, lib64 -> lib, resolv.conf -> /tmp/...), и разыменовать их значило бы
# раздуть дерево и сломать раскладку, которую ждёт загрузчик.
$(T_TARGET)/runtime: $(T_INTERMEDIATE)/runtime | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -a $< $@
	touch $@

$(T_TARGET)/%: $(T_INTERMEDIATE)/% | $(T_TARGET)
	@mkdir -p $(dir $@)
	cp -af $< $@

$(T_TARGET)/share/licenses: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf ../licenses $(T_TARGET)/share

$(T_TARGET)/release: $(T_TARGET)
	echo "$(GIT_TAG)" > $@
	touch $@

# target.ini / toolchain.cmake едут в таргет (у target-linux остаются в
# intermediate): против ЭТОГО таргета внешняя сборка драйвера mesa в xenolith-os
# конфигурируется через meson/cmake, и cross-файлы нужны ей на месте. @DIRNAME@
# в target.ini — плейсхолдер: потребитель подставляет туда путь таргета и свою
# секцию [binaries] (в SDK компилятор лежит в hosts/<host-id>, а не внутри
# таргета, поэтому зашить путь здесь нельзя).
all: $(ALL_INSTALL_STATIC_LIBS) $(ALL_INSTALL_SHARED_LIBS) \
		$(T_TARGET)/include_libc $(T_TARGET)/lib $(T_TARGET)/usr/include \
		$(T_TARGET)/usr/lib/pkgconfig $(T_TARGET)/runtime \
		$(T_TARGET)/share/licenses \
		$(T_TARGET)/target.mk $(T_TARGET)/target.ini $(T_TARGET)/toolchain.cmake \
	$(T_TARGET) $(T_TARGET)/release

.PHONY: all
.DEFAULT_GOAL := all
