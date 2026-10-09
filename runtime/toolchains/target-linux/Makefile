# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk

OUT_SYSROOT ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

# Внешний кросс-компилятор lcc/l++ (MCST) для e2k64: свои компиляторы, binutils
# и libc, layout отличается от собираемого вручную (<prefix>/host/bin). Пути
# подставляются в ветке e2k64 ниже; переопределяется из окружения/CLI.
LCC_TOOLCHAIN_PATH ?= /opt/mcst/lcc-1.31.05.e2k-v5.8c2.linux-6.1

include $(MAKE_ROOT)../common/utils/detect-platform.mk

MKDIR = mkdir -p

SP_OPT := -O3 -fPIC -DPIC -fno-exceptions

INTERNAL_LIBS :=

LIBS := \
	libc++abi.a \
	libunwind.a \
	libz.a \
	libbz2.a \
	liblzma.a \
	libzstd.a \
	libbrotlienc.a \
	libjpeg.a \
	libpng16.a \
	libgif.a \
	libwebp.a \
	libtiff.a \
	libfreetype.a \
	libharfbuzz.a \
	libSheenBidi.a \
	libsqlite3.a \
	libnghttp3.a \
	libngtcp2.a \
	libssl.a \
	libgost_engine.a \
	libcurl-openssl.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a \
	libxml2.a \
	libffi.a \
	libexpat.a \
	libwayland-client.a \
	libdrm.so.2

WASM_LIBS := \
	libiwasm-release.a \
	libiwasm-debug.a

BINS := \
	spirv-opt \
	glslang

INCLUDES := \
	spirv/unified1/spirv.h \
	vulkan/vulkan.h \
	simde/simde-arch.h

ifndef SP_ARCH

# Цели задаются полным триплетом (роутинг как у target-apple). libc выбирается
# по суффиксу: -unknown-linux-gnu -> glibc/, -unknown-linux-musl -> musl/.
# Сборка сторонних библиотек (ветка SP_ARCH ниже) libc-агностична и общая.
GNU_ARCHS := x86_64 aarch64 riscv64 loongarch64
MUSL_ARCHS := x86_64 aarch64 riscv64 loongarch64

# По умолчанию — e2k64 (lcc) + x86_64 (glibc), как раньше.
all: e2k64-unknown-linux x86_64-unknown-linux-gnu

clean:

# $(1)=арка, $(2)=суффикс libc (gnu|musl), $(3)=подкаталог libc (glibc|musl).
define linux_target =
$(1)-unknown-linux-$(2):
	$$(MAKE) -C $(3) out SP_ARCH_TARGET=$(1) \
		OUT_HOST=$$(abspath $$(MAKE_ROOT)../hosts/$$(HOST_ID)) \
		OUT_SYSROOT=$$(OUT_SYSROOT)/$(1)-unknown-linux-$(2)
	$$(MAKE) -f $$(THIS_FILE) SP_ARCH=$(1) SP_TARGET=$(1)-unknown-linux-$(2) \
		OUT_SYSROOT=$$(OUT_SYSROOT)/$(1)-unknown-linux-$(2)
	$$(MAKE) -f $$(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$$(OUT_SYSROOT)/$(1)-unknown-linux-$(2) \
		T_TARGET=$$(TOOLCHAIN_TARGETS)/$(1)-unknown-linux-$(2)
endef

$(foreach a,$(GNU_ARCHS),$(eval $(call linux_target,$(a),gnu,glibc)))
$(foreach a,$(MUSL_ARCHS),$(eval $(call linux_target,$(a),musl,musl)))

# e2k-цели собираются внешним lcc (libc живёт в тулчейне). $(1)=арка,
# $(2)=флаг режима разрядности (lcc -m32/-m64; e2k64 совпадает с умолчанием
# -m64, поэтому пуст), $(3)=каталог libc-архивов в поставке lcc (lib64|lib32).
# -dumpmachine у lcc всегда e2k-linux-gnu, так что режим различается только
# этим флагом; для meson он обязан лежать в cross-файле (env CFLAGS в
# cross-режиме игнорируется), для остальных сборок едет в SP_TARGET_FLAGS
# (configure.mk).
define e2k_target =
$(1)-unknown-linux:
	# Подготовка intermediate ДО сборки зависимостей. libc-шага нет (libc живёт
	# в тулчейне lcc), поэтому: lcc-lib держит симлинки libc-архивов для
	# cmake-проб (find_library видит только intermediate; в пакет это не
	# попадает — install-target берёт только usr/lib), include указывает на
	# заголовки lcc (шаг install-target скопирует их как include_libc), lib/
	# пуст (asan под lcc не собирается).
	@$$(MKDIR) $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/usr/lib) \
		$$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/lcc-lib/lib) \
		$$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/lib)
	@ln -sfn $$(LCC_TOOLCHAIN_PATH)/fs/usr/$(3)/libc.a \
		$$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/lcc-lib/lib/libc.a)
	@ln -sfn $$(LCC_TOOLCHAIN_PATH)/fs/usr/$(3)/libm.a \
		$$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/lcc-lib/lib/libm.a)
	@ln -sfn $$(LCC_TOOLCHAIN_PATH)/fs/usr/include \
		$$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/include)
	# meson cross file для libdrm/wayland (формат glibc/target.mk; @DIRNAME@
	# meson разворачивает сам в каталог файла). built-in options без
	# clang-флагов; c/c++_args несут флаг режима (для e2k64 — пусто).
	@echo '[host_machine]' > $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "system = 'linux'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "cpu_family = 'e2k'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "cpu = 'e2kv4'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "endian = 'little'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo '[binaries]' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "c = '$$(abspath $$(LCC_TOOLCHAIN_PATH))/bin/lcc'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "cpp = '$$(abspath $$(LCC_TOOLCHAIN_PATH))/bin/l++'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "ar = '$$(abspath $$(LCC_TOOLCHAIN_PATH))/binutils/bin/e2k-linux-ar'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "strip = '$$(abspath $$(LCC_TOOLCHAIN_PATH))/binutils/bin/e2k-linux-strip'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "pkg-config = 'pkg-config'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo '[properties]' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "sys_root = '@DIRNAME@'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "pkg_config_libdir = '@DIRNAME@/usr/lib/pkgconfig'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo '[built-in options]' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "prefix = '@DIRNAME@'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "libdir = '@DIRNAME@/usr/lib'" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "c_args = [$(if $(2),'$(2)',)]" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "cpp_args = [$(if $(2),'$(2)',)]" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "c_link_args = [$(if $(2),'$(2)',)]" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	@echo "cpp_link_args = [$(if $(2),'$(2)',)]" >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.ini)
	$$(MAKE) -f $$(THIS_FILE) SP_ARCH=$(1) SP_TARGET=$(1)-unknown-linux \
		SP_TOOLCHAIN_PREFIX=$$(LCC_TOOLCHAIN_PATH) \
		OUT_SYSROOT=$$(OUT_SYSROOT)/$(1)-unknown-linux
	@echo 'TARGET_SYSROOT := $$$$(patsubst %/,%,$$$$(dir $$$$(lastword $$$$(MAKEFILE_LIST))))' > $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_SYSTEM := Linux' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_ARCH := $(1)' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_INCLUDE_DIR := $$$$(TARGET_SYSROOT)/usr/include' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$$$(TARGET_SYSROOT)/include_libc' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_LIB_DIR := $$$$(TARGET_SYSROOT)/usr/lib' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_LIB_DIR_LIBC := $$$$(TARGET_SYSROOT)/lib' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_GENERAL_CFLAGS := $(if $(filter e2k32,$(1)),-D__ILP32__ )-D__SPRT_CONFIG_COMPILER_ASSISTED_INTS=0 -idirafter $$$$(TARGET_INCLUDE_DIR)' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_GENERAL_CXXFLAGS := $(if $(filter e2k32,$(1)),-D__ILP32__ )-D__SPRT_CONFIG_COMPILER_ASSISTED_INTS=0 -idirafter $$$$(TARGET_INCLUDE_DIR)' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_GENERAL_LDFLAGS :=' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_CC := $$(abspath $$(LCC_TOOLCHAIN_PATH))/bin/lcc' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_CXX := $$(abspath $$(LCC_TOOLCHAIN_PATH))/bin/l++' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	@echo 'TARGET_AR := $$(abspath $$(LCC_TOOLCHAIN_PATH))/binutils/bin/e2k-linux-ar' >> $$(abspath $$(OUT_SYSROOT)/$(1)-unknown-linux/target.mk)
	$$(MAKE) -f $$(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$$(OUT_SYSROOT)/$(1)-unknown-linux \
		T_TARGET=$$(TOOLCHAIN_TARGETS)/$(1)-unknown-linux
endef

$(eval $(call e2k_target,e2k64,,lib64))
$(eval $(call e2k_target,e2k32,-m32,lib32))
# -m128: LP64 с 16-байтным указателем (long/size_t = 8), контейнер ELF64.
$(eval $(call e2k_target,e2k128,-m128,lib128))

.PHONY: all clean e2k64-unknown-linux e2k32-unknown-linux e2k128-unknown-linux \
	$(addsuffix -unknown-linux-gnu,$(GNU_ARCHS)) \
	$(addsuffix -unknown-linux-musl,$(MUSL_ARCHS))

else # SP_ARCH

# e2k (e2k64/e2k32/e2k128) живёт не в ../$(SP_TARGET), а в intermediate (libc
# у lcc своя, шаг glibc или musl отсутствует). Условия ниже стоят ДО общих ?=,
# чтобы переопределения с CLI/env имели приоритет.
ifneq (,$(filter $(SP_ARCH),e2k32 e2k64 e2k128))
OUT_SYSROOT ?= $(abspath $(MAKE_ROOT)intermediate/$(SP_TARGET))
SP_TOOLCHAIN_PREFIX ?= $(LCC_TOOLCHAIN_PATH)
endif

OUT_SYSROOT ?= $(realpath ../$(SP_TARGET))

SP_TOOLCHAIN_PREFIX ?= $(OUT_SYSROOT)
SP_INSTALL_PREFIX ?= $(OUT_SYSROOT)
SP_TOOLCHAIN_FILE ?= $(realpath $(OUT_SYSROOT)/toolchain.cmake)

ifneq (,$(filter $(SP_ARCH),e2k32 e2k64 e2k128))
# lcc: bin/ держит драйверы lcc/l++, binutils — префискованные GNU-утилиты.
SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/lcc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/bin/l++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/binutils/bin/e2k-linux-ar)
else
SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/cc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/c++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/ar)
endif

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	LINUX=1 \
	SP_SYSNAME=Linux \
	SP_ARCH=$(SP_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_TOOLCHAIN_FILE) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ifdef SP_TOOLCHAIN_PREFIX
FORWARD_VARS += SP_TOOLCHAIN_PREFIX=$(SP_TOOLCHAIN_PREFIX)
endif

ifdef SP_IS_NATIVE
FORWARD_VARS += SP_NATIVE=1
endif

# WAMR не знает LoongArch (нет WAMR_BUILD_TARGET и 64-битного invokeNative),
# поэтому на loongarch64 iwasm не собирается, а stappler_wasm недоступен.
ifeq ($(SP_ARCH),loongarch64)
LIBS := $(filter-out $(WASM_LIBS),$(LIBS))
endif

# e2k (все режимы): libc++abi/libunwind не собираются — C++ ABI и unwinder
# приходят из libgcc_s.so.1 тулчейна lcc (динамически, на стороне
# приложения/рантайма). WAMR тоже выпадает: e2k он не поддерживает. libffi
# собирается из форка МЦСТ (src.mk/ffi-e2k — апстрим 3.8.0 e2k не портирован),
# поэтому libwayland-client от него остаётся.
ifneq (,$(filter $(SP_ARCH),e2k32 e2k64 e2k128))
LIBS := $(filter-out libc++abi.a libunwind.a $(WASM_LIBS),$(LIBS))
endif

# compiler-rt asan: для glibc собирается через compiler_rt.mk. Для musl libc++ и
# compiler-rt уже даёт подкаталог musl/ (asan под musl в первой версии не строим).
# e2k: compiler-rt собирается только clang'ом, под lcc не строится вовсе.
ifeq (,$(filter $(SP_ARCH),e2k32 e2k64 e2k128))
ifeq (,$(findstring musl,$(SP_TARGET)))
INTERNAL_LIBS += clang/lib/linux/libclang_rt.asan-$(SP_ARCH).a
endif
endif

ARCH_LIBS = \
	$(addprefix $(SP_INSTALL_PREFIX)/lib/,$(INTERNAL_LIBS)) \
	$(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(SP_INSTALL_PREFIX)/share/licenses

# libc++abi + libunwind одной сборкой runtimes (без libc++ — see libcxxabi.mk).
$(SP_INSTALL_PREFIX)/usr/lib/libc++abi.a $(SP_INSTALL_PREFIX)/usr/lib/libunwind.a: $(MAKE_ROOT)libcxxabi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/lib/clang/lib/linux/libclang_rt.asan-$(SP_ARCH).a: $(MAKE_ROOT)compiler_rt.mk $(SP_INSTALL_PREFIX)/usr/lib/libc++abi.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libz.a: ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbz2.a: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/liblzma.a: ../common/xz.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a:  ../common/zstd.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a: ../common/brotli.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a: ../common/libjpeg-turbo.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libpng16.a: ../common/libpng.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgif.a: ../common/gif.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libwebp.a: ../common/libwebp.mk $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a $(SP_INSTALL_PREFIX)/usr/lib/libgif.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libtiff.a: ../common/tiff.mk $(SP_INSTALL_PREFIX)/usr/lib/libwebp.a $(SP_INSTALL_PREFIX)/usr/lib/liblzma.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a: ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libbz2.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libharfbuzz.a:../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libSheenBidi.a: ../common/sheenbidi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libsqlite3.a: ../common/sqlite.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a:  ../common/nghttp3.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# ngtcp2 QUIC transport with the OpenSSL (ossl) crypto backend: libngtcp2.a +
# libngtcp2_crypto_ossl.a, consumed by curl for HTTP/3 (USE_NGTCP2).
$(SP_INSTALL_PREFIX)/usr/lib/libngtcp2.a:  ../common/ngtcp2.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libssl.a: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a:  ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/idn2.h: $(MAKE_ROOT)../../include/sprt/compat/idn2.h
	@$(MKDIR) $(SP_INSTALL_PREFIX)/usr/include
	cp -f $< $@

# ICU is deliberately NOT in LIBS: nothing links it any more (the runtime does its
# own case mapping, collation and IDN), and it is by far the most expensive library
# here - the cross build needs a full native ICU built first just to generate its
# data. The rule stays so a real libicuuc can still be produced on demand, for
# comparing the runtime's Unicode behaviour against the reference implementation:
#
#     make -C target-linux x86_64-unknown-linux-gnu   # sysroot, no ICU
#     make -C target-linux icu SP_ARCH=... SP_TARGET=...
#
# The icu4c *sources* are still downloaded by src.mk - they are the UCD and
# conformance data the generators read. See the icu4c block there.
icu: $(SP_INSTALL_PREFIX)/usr/lib/libicuuc.a

$(SP_INSTALL_PREFIX)/usr/lib/libicuuc.a: $(MAKE_ROOT)icu.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a $(SP_INSTALL_PREFIX)/usr/lib/libngtcp2.a $(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a $(SP_INSTALL_PREFIX)/usr/include/idn2.h
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/usr/lib/libiwasm-debug.a:  ../common/wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libxml2.a: ../common/libxml2.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libffi.a: $(MAKE_ROOT)ffi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libexpat.a: $(MAKE_ROOT)expat.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libwayland-client.a: $(MAKE_ROOT)wayland.mk $(SP_INSTALL_PREFIX)/usr/lib/libxml2.a $(SP_INSTALL_PREFIX)/usr/lib/libffi.a $(SP_INSTALL_PREFIX)/usr/lib/libexpat.a
	$(MAKE) -f $< $(FORWARD_VARS)

# The only shared library in the set (for why, see common/libdrm.mk). Direct KMS
# rendering needs it, and there must be exactly ONE per process: the GPU driver
# links it too.
$(SP_INSTALL_PREFIX)/usr/lib/libdrm.so.2: ../common/libdrm.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h: ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h

$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h: ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h

$(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h

$(SP_INSTALL_PREFIX)/share/licenses:
	rm -rf $(SP_INSTALL_PREFIX)/share/licenses
	cp -rf runtime/include/* $(SP_INSTALL_PREFIX)/include
	cp -rf ../licenses $(SP_INSTALL_PREFIX)/share/licenses

.PHONY: all icu

endif
