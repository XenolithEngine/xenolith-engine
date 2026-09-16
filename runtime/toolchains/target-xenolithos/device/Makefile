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

# Базовый слой device-таргета Xenolith OS: libc устройства плюс всё, что нужно
# самому устройству в рантайме.
#
# Аналог target-linux/glibc, но с двумя отличиями:
#   * glibc 2.39 (а не 2.33) и триплет <arch>-xenolithos-linux-gnu;
#   * помимо компайл-тайм-sysroot производится РАНТАЙМ-слой: userland (busybox,
#     dropbear, vulkaninfo), лоадер Vulkan и готовый base-rootfs, из которого
#     xenolith-os собирает образ для железа.
#
# Драйвера GPU здесь НЕТ намеренно: mesa отличается от устройства к устройству,
# поэтому собирается в xenolith-os под конкретную борду и накладывается
# оверлеем. Всё необходимое для этого таргет несёт: libdrm, expat, zlib,
# заголовки Vulkan и cross-файлы (target.ini / toolchain.cmake).
#
# Порядок слоёв:
#   host.mk     host-бутстрап gcc (общий с target-linux/glibc)
#   target.mk   binutils -> gcc-min -> glibc 2.39
#   libs.mk     compiler-rt -> libc++ -> zlib/expat -> libxcrypt
#   vulkan.mk   Vulkan-заголовки + ICD-лоадер
#   userland.mk busybox + dropbear + vulkaninfo
#   rootfs.mk   стейджинг рантайм-rootfs
#
# Цели:
#   make download   выкачать исходники
#   make sources    напечатать, откуда фактически берётся каждая зависимость
#   make host       host-бутстрап (делегируется в target-linux/glibc)
#   make target     binutils + gcc-min + glibc
#   make libs       рантаймы поверх glibc
#   make vulkan / userland
#   make out        всё вместе + раскладка в OUT_SYSROOT
#   make all        host + target + libs + vulkan + userland
#   make rootfs     стейджинг рантайм-rootfs; ОТДЕЛЬНЫЙ последний шаг — ему нужны
#                   разделяемые библиотеки из общего набора сторонних библиотек,
#                   который идёт уже поверх разложенного OUT_SYSROOT

.DEFAULT_GOAL := all

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
MAKE_ROOT := $(dir $(realpath $(LOCAL_MAKEFILE)))

SP_NJOBS ?= -j$(shell nproc)

# ── целевая арка -> триплет / загрузчик / ARCH ядра ───────────────────────────
SP_ARCH_TARGET ?= aarch64

# SP_CPU — значение meson host_machine.cpu; meson отвергает пустую строку,
# поэтому каждая поддерживаемая арка обязана его задать. Переопределяется под
# конкретную борду (например SP_CPU=cortex-a53).
ifeq ($(SP_ARCH_TARGET),x86_64)
SP_ARCH_LINUX := x86_64
SP_LDSO_NAME  := ld-linux-x86-64.so.2
SP_HAS_LIBMVEC := 1
SP_CPU        ?= x86_64

else ifeq ($(SP_ARCH_TARGET),aarch64)
SP_ARCH_LINUX := arm64
SP_LDSO_NAME  := ld-linux-aarch64.so.1
SP_HAS_LIBMVEC := 1
SP_CPU        ?= cortex-a72

else ifeq ($(SP_ARCH_TARGET),riscv64)
SP_ARCH_LINUX := riscv
SP_LDSO_NAME  := ld-linux-riscv64-lp64d.so.1
SP_CPU        ?= rv64gc

else
$(error Unknown target arch: $(SP_ARCH_TARGET). Supported: x86_64 aarch64 riscv64)
endif

SP_TARGET_TRIPLE := $(SP_ARCH_TARGET)-xenolithos-linux-gnu

# ── арка хоста ────────────────────────────────────────────────────────────────
SP_ARCH_HOST := $(shell uname -m)
SP_HOST_TRIPLE := $(SP_ARCH_HOST)-unknown-linux-gnu

# Host-тулчейн движка: clang/clang++/ld.lld + llvm-strip/readelf. Задаётся
# снаружи (target-xenolithos/Makefile передаёт hosts/$(HOST_ID)); значение по
# умолчанию нужно, чтобы каталог можно было собирать и напрямую.
OUT_HOST ?= $(abspath $(MAKE_ROOT)../../hosts/$(SP_HOST_TRIPLE))
ENGINE_HOST_DIR := $(OUT_HOST)

# ── каталоги сборки ───────────────────────────────────────────────────────────
# Всё ниже host-бутстрапа именуется по триплету, чтобы несколько арок могли
# собираться рядом (и даже одновременно), не переиспользуя чужие cross-файлы.
TBUILD := build/$(SP_TARGET_TRIPLE)

OUT_SYSROOT ?= $(SP_TARGET_TRIPLE)

# ── слои ──────────────────────────────────────────────────────────────────────
include $(MAKE_ROOT)src.mk
include $(MAKE_ROOT)host.mk
include $(MAKE_ROOT)target.mk
include $(MAKE_ROOT)libs.mk
include $(MAKE_ROOT)vulkan.mk
include $(MAKE_ROOT)userland.mk
include $(MAKE_ROOT)rootfs.mk


#
# Раскладка компайл-тайм-sysroot в OUT_SYSROOT
#
# Форма ровно та же, что у остальных таргетов движка (target-linux/glibc), чтобы
# install-target.mk и make/utils/defaults.mk работали без исключений:
#   lib/         libc, CRT-объекты, загрузчик, resource-dir clang
#   include/     -> include_libc в готовом таргете
#   usr/lib/     статические библиотеки (заполняется сборкой сторонних либ)
#   usr/include/ заголовки сторонних библиотек
#

# Dev-файлы (CRT-объекты, libc_nonshared) и рантайм-.so разделены только для
# читаемости — благодаря принудительным libdir/slibdir (target.mk) и те, и
# другие лежат в одном каталоге lib/ на всех арках.
OUT_LIBC_DEV_FILES := \
	crt1.o \
	crti.o \
	crtn.o \
	gcrt1.o \
	Mcrt1.o \
	Scrt1.o \
	libc_nonshared.a

OUT_LIBC_RT_FILES := \
	$(SP_LDSO_NAME) \
	libanl.so.1 \
	libBrokenLocale.so.1 \
	libc.so.6 \
	libdl.so.2 \
	libm.so.6 \
	libmemusage.so \
	libpthread.so.0 \
	libresolv.so.2 \
	librt.so.1 \
	libthread_db.so.1 \
	libutil.so.1

# libmvec входит в дефолтный набор арки на x86_64 и (с 2.39) на aarch64; на
# riscv64 его нет вовсе.
ifdef SP_HAS_LIBMVEC
OUT_LIBC_RT_FILES += libmvec.so.1
endif

# libdir=/usr/lib -> ../lib и slibdir=/lib (см. target.mk), поэтому и dev-файлы,
# и рантайм-.so оказываются в одном каталоге lib/ на всех трёх арках.
OUT_LIBC_FILES := $(OUT_LIBC_DEV_FILES) $(OUT_LIBC_RT_FILES)

OUT_LIBC := $(addprefix $(OUT_SYSROOT)/lib/,$(OUT_LIBC_FILES))

OUT_LIBS := $(OUT_LIBC) $(OUT_SYSROOT)/lib/libc.so $(OUT_SYSROOT)/lib/libm.so

OUT_USR_LIBS := $(OUT_SYSROOT)/usr/lib/libunwind.a

# Объявляем, ЧЕМ порождается каждый исходник стейджинга. Без этого сборка из
# чистого дерева падает на «нет правила для сборки .../lib/crt1.o»: отдельного
# правила у этих файлов нет, они появляются побочным эффектом install у glibc /
# libc++. Пустой рецепт (`@:`) ровно это и фиксирует — сам файл кладёт
# зависимость, здесь только порядок.
#
# Зависимость ОБЯЗАНА быть order-only (`|`). При обычной glibc install пишет
# CRT-объекты раньше, чем libc.so.6, так что crt1.o всегда старше — make счёл бы
# его устаревшим, «пересобрал» пустым рецептом, пометил обновлённым и потянул
# каскадом весь стейджинг и раскладку таргета. Сборка переставала быть
# идемпотентной, а `make` без изменений всё равно перекладывал сотни мегабайт.
# Order-only даёт ровно то, что нужно: порядок — да, устаревание — нет.
GLIBC_PRODUCTS := $(filter-out $(TARGET_GLIBC),\
	$(addprefix $(TARGET_SYSROOT)/lib/,$(OUT_LIBC_FILES) libc.so libm.so))

$(GLIBC_PRODUCTS): | $(TARGET_GLIBC)
	@:

$(TARGET_SYSROOT)/lib/libunwind.a: | $(TARGET_LIBCXX)
	@:

$(OUT_LIBC): $(OUT_SYSROOT)/lib/%: $(TARGET_SYSROOT)/lib/% $(TARGET_GLIBC)
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	cp -f $< $@
	chmod +x $@

# libc.so и libm.so glibc делает ЛИБО линкер-скриптом, ЛИБО простым симлинком на
# .so.N — что именно, зависит от арки и набора библиотек. libc.so скрипт всегда
# (нужно подтянуть libc_nonshared.a), а libm.so — скрипт только там, где есть
# libmvec для AS_NEEDED; на riscv64 libmvec'а нет, и libm.so там симлинк.
#
# Скрипт надо переписать: его GROUP() ссылается на абсолютные девайсовые пути
# (/lib/libc.so.6, /usr/lib/libc_nonshared.a), которые ld резолвит относительно
# --sysroot, а в раскладке таргета всё лежит плоско в lib/ — поэтому пути
# сводятся к голым именам, их ld находит по своему -L-пути. Ровно то же делает
# target-linux/glibc (там это sed по префиксу дерева сборки).
#
# А вот симлинк надо скопировать как есть. Гнать через sed нужно ИМЕННО скрипт:
# на симлинке sed читает содержимое цели, то есть ELF, вырезает из него всякую
# байтовую последовательность "/lib/" и молча отдаёт битую библиотеку — вместо
# 9-байтового симлинка в таргет уезжает искажённая копия на сотни килобайт.
# Поэтому решает не арка, а фактический тип файла.
define stage_libc_script =
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	if head -c 64 $< | grep -qa 'GNU ld script'; then \
		sed -e 's#/usr/lib/##g' -e 's#/lib/##g' $< > $@; \
	else \
		cp -a $< $@; \
	fi
endef

$(OUT_SYSROOT)/lib/libc.so: $(TARGET_SYSROOT)/lib/libc.so $(TARGET_GLIBC)
	$(stage_libc_script)

$(OUT_SYSROOT)/lib/libm.so: $(TARGET_SYSROOT)/lib/libm.so $(TARGET_GLIBC)
	$(stage_libc_script)

$(OUT_SYSROOT)/usr/lib/libunwind.a: $(TARGET_SYSROOT)/lib/libunwind.a $(TARGET_GLIBC)
	@mkdir -p $(OUT_SYSROOT)/usr/lib
	@rm -f $@
	cp -f $< $@

# ── dev-файлы рантайм-библиотек устройства ────────────────────────────────────
# .so + pkg-config заголовки Vulkan/libxcrypt. Без них против готового таргета
# нельзя собрать драйвер mesa под конкретную борду.
#
# libdrm сюда больше не входит: она собирается общим набором сторонних библиотек
# (common/libdrm.mk) прямо в usr/lib и usr/include этого же дерева — вторым
# шагом сборки таргета.
#
# Кладутся именно в usr/lib, а не в lib: lib — это libc и CRT, всё остальное по
# соглашению таргетов движка живёт в usr.
OUT_DEVICE_DEV := $(OUT_SYSROOT)/usr/lib/.device-dev

$(OUT_DEVICE_DEV): $(TARGET_VULKAN_LOADER) $(TARGET_LIBXCRYPT)
	@mkdir -p $(OUT_SYSROOT)/usr/lib/pkgconfig $(OUT_SYSROOT)/usr/include
	cp -a $(TARGET_SYSROOT)/lib/libvulkan.so* $(OUT_SYSROOT)/usr/lib/
	cp -a $(TARGET_SYSROOT)/lib/libcrypt.so* $(OUT_SYSROOT)/usr/lib/
	cp -a $(TARGET_SYSROOT)/lib/pkgconfig/*.pc $(OUT_SYSROOT)/usr/lib/pkgconfig/
# Часть .pc собирается с абсолютным CMAKE_INSTALL_PREFIX дерева сборки
# (vulkan.pc: prefix=<sysroot>/usr). Потребитель запускает pkg-config с
# PKG_CONFIG_SYSROOT_DIR=<таргет>, который дописывает свой префикс СВЕРХУ, —
# и путь получается склеенным из двух корней. Сводим prefix к девайсовому /usr:
# в sysroot .pc обязан быть sysroot-относительным. Тот же класс бага, что glibc
# или лоадер Vulkan с чужим --prefix.
	sed -i 's#$(SYSROOT_ABS)##g' $(OUT_SYSROOT)/usr/lib/pkgconfig/*.pc
	touch $@

# ── resource-dir clang ────────────────────────────────────────────────────────
# compiler-rt собран с LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off, поэтому файлы
# лежат в lib/linux с арх-суффиксом — именно там их и ищет clang, когда
# -resource-dir указывает на lib/clang (проверено линковкой).
#
# Пер-триплетного каталога lib/clang/lib/<triple>/ здесь намеренно нет:
# target-linux объявляет для него правила, но никогда не ставит их в
# предпосылки, так что и в его intermediate, и в готовом таргете этого каталога
# не существует. Копировать сюда мёртвую ветку смысла нет — тем более что она
# требует liborc_rt, а ORC для устройства не собирается.
#
# На финальном шаге сборки сторонних библиотек compiler_rt.mk движка заменяет
# этот каталог своим — с asan и остальными санитайзерами.
$(OUT_SYSROOT)/lib/clang/lib/linux: $(TARGET_COMPILERRT)
	@mkdir -p $(dir $@)
	@rm -rf $@
	cp -rf $(TARGET_SYSROOT)/lib/linux $@

# ── заголовки ─────────────────────────────────────────────────────────────────
# UAPI ядра + glibc + c++/v1 + vulkan/ — всё, что glibc и рантаймы поставили в
# include/ целевого sysroot. Уезжает в include_libc готового таргета.
$(OUT_SYSROOT)/include: $(TARGET_GLIBC) $(TARGET_LIBCXX) $(TARGET_VULKAN_HEADERS)
	@rm -rf $(OUT_SYSROOT)/include
	@mkdir -p $(OUT_SYSROOT)/include
	cp -rf $(TARGET_SYSROOT)/include/* $(OUT_SYSROOT)/include

# ── описания таргета для сборочных систем ─────────────────────────────────────
$(OUT_SYSROOT)/target.mk: $(LOCAL_MAKEFILE)
	@echo 'Build $@'
	@mkdir -p $(dir $@)
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Linux' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH_TARGET)' >> $@
	@echo 'TARGET_NAME := $(SP_TARGET_TRIPLE)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/include_libc' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
# Без -lc++abi, в отличие от target-linux/glibc. Там в usr/lib лежит только
# libc++abi.a, поэтому флаг просто дублирует -l:libc++abi.a из
# runtime/runtime.mk. Здесь же рядом с ней лежит libc++abi.so (её несёт образ —
# ею пользуются лоадер Vulkan и драйвер GPU), и -lc++abi выбрал бы разделяемую:
# приложение получило бы DT_NEEDED на libc++abi.so.1 ПЛЮС статическую копию
# внутри себя. Два C++-ABI-рантайма в одном процессе — ровно то, ради чего всё
# это и делается. Приложение линкует libc++abi статически (runtime.mk), а
# единство ABI в процессе обеспечивает точечный экспорт из исполняемого файла
# (make/os/linux.mk).
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@
	@echo '' >> $@
	@echo '# Рантайм-слой устройства: то, из чего xenolith-os собирает образ.' >> $@
	@echo '# runtime/rootfs — готовый base-rootfs на арку (bin/sbin/lib/usr/etc,' >> $@
	@echo '# стрипнутый), без приложения, без борды и без драйвера GPU: драйвер' >> $@
	@echo '# зависит от железа и накладывается оверлеем сборщиком образа.' >> $@
	@echo 'TARGET_DEVICE_RUNTIME_DIR := $$(TARGET_SYSROOT)/runtime' >> $@
	@echo 'TARGET_DEVICE_ROOTFS := $$(TARGET_DEVICE_RUNTIME_DIR)/rootfs' >> $@

$(OUT_SYSROOT)/target.ini: $(LOCAL_MAKEFILE)
	@echo 'Build $@'
	@mkdir -p $(dir $@)
	@echo '[host_machine]' > $@
	@echo "system = 'linux'" >> $@
	@echo "cpu_family = '$(SP_ARCH_TARGET)'" >> $@
	@echo "cpu = '$(SP_CPU)'" >> $@
	@echo "endian = 'little'" >> $@
	@echo '[binaries]' >> $@
	@echo "c = '@DIRNAME@/host/bin/cc'" >> $@
	@echo "cpp = '@DIRNAME@/host/bin/c++'" >> $@
	@echo "ar = '@DIRNAME@/host/bin/llvm-ar'" >> $@
	@echo "strip = '@DIRNAME@/host/bin/llvm-strip'" >> $@
	@echo "pkg-config = 'pkg-config'" >> $@
	@echo '[properties]' >> $@
	@echo "sys_root = '@DIRNAME@'" >> $@
	@echo "pkg_config_libdir = '@DIRNAME@/usr/lib/pkgconfig'" >> $@
	@echo '[built-in options]' >> $@
	@echo "prefix = '@DIRNAME@'" >> $@
	@echo "libdir = '@DIRNAME@/lib'" >> $@
	@echo "c_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_TARGET_TRIPLE)','--sysroot=@DIRNAME@']" >> $@
	@echo "c_link_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_TARGET_TRIPLE)','--sysroot=@DIRNAME@']" >> $@
	@echo "cpp_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_TARGET_TRIPLE)','--sysroot=@DIRNAME@']" >> $@
	@echo "cpp_link_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_TARGET_TRIPLE)','--sysroot=@DIRNAME@']" >> $@

$(OUT_SYSROOT)/toolchain.cmake: $(LOCAL_MAKEFILE)
	@mkdir -p $(dir $@)
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR "$(SP_ARCH_TARGET)")' >> $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_TARGET_TRIPLE)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_TARGET_TRIPLE)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_TARGET_TRIPLE)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_TARGET_TRIPLE)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS}  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_TARGET_TRIPLE)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS}  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_TARGET_TRIPLE)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS}  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_TARGET_TRIPLE)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang" CACHE STRING "" FORCE)' >> $@
	@echo 'if(DEFINED SP_CXX_COMPILER)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${SP_CXX_COMPILER}" CACHE STRING "" FORCE)' >> $@
	@echo 'else()' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang" CACHE STRING "" FORCE)' >> $@
	@echo 'endif()' >> $@
	@echo 'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH Off)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(PKG_CONFIG_PATH "$${CMAKE_CURRENT_LIST_DIR}/usr/lib/pkgconfig")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@

# ── итоговая раскладка ────────────────────────────────────────────────────────
# rootfs НЕ в предпосылках: он стейджится третьим шагом, после общего набора
# сторонних библиотек — см. rootfs.mk.
$(OUT_SYSROOT)/sysroot: $(OUT_LIBS) $(OUT_USR_LIBS) $(OUT_DEVICE_DEV) \
		$(OUT_SYSROOT)/lib/clang/lib/linux $(OUT_SYSROOT)/include \
		$(OUT_SYSROOT)/toolchain.cmake $(OUT_SYSROOT)/target.mk $(OUT_SYSROOT)/target.ini
	cd $(OUT_SYSROOT)/lib; ln -fs libanl.so.1 libanl.so
	cd $(OUT_SYSROOT)/lib; ln -fs libBrokenLocale.so.1 libBrokenLocale.so
	cd $(OUT_SYSROOT)/lib; ln -fs libdl.so.2 libdl.so
	cd $(OUT_SYSROOT)/lib; ln -fs libpthread.so.0 libpthread.so
	cd $(OUT_SYSROOT)/lib; ln -fs libresolv.so.2 libresolv.so
	cd $(OUT_SYSROOT)/lib; ln -fs librt.so.1 librt.so
	cd $(OUT_SYSROOT)/lib; ln -fs libthread_db.so.1 libthread_db.so
	cd $(OUT_SYSROOT)/lib; ln -fs libutil.so.1 libutil.so
ifdef SP_HAS_LIBMVEC
	cd $(OUT_SYSROOT)/lib; ln -fs libmvec.so.1 libmvec.so
endif
ifdef OUT_HOST
	rm -f $(OUT_SYSROOT)/host
	cd $(OUT_SYSROOT); ln -fs $(OUT_HOST) host
	cd $(OUT_SYSROOT)/lib/clang; ln -fs $(abspath $(HOST_CLANG_RESOURCE))/include include
endif
	touch $@

download: $(LINUX_TARGET_HEADERS_DIR)

out: all $(OUT_SYSROOT)/sysroot

all: host target libs vulkan userland

# ── очистка ───────────────────────────────────────────────────────────────────
# clean — дерево сборки и sysroot'ы ЭТОЙ арки. Общий host-бутстрап и скачанные
# исходники переживают: их пересборка стоит часы и от арки не зависит.
clean:
	rm -rf $(TBUILD) $(TARGET_SYSROOT) $(USERLAND_SYSROOT) $(OUT_SYSROOT)
	@echo "[clean] $(SP_TARGET_TRIPLE) removed (host bootstrap and src/ kept)"

distclean:
	rm -rf build src sysroot-target-* sysroot-userland-*
	@echo "[distclean] done"

.PHONY: all out clean distclean download
