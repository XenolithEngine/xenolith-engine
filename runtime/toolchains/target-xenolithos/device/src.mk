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

# Источники для device-таргета Xenolith OS.
#
# Разложены на три группы:
#
#   1. GNU-цепочка (gcc, binutils, ядро, make) — ОБЩАЯ с target-linux/glibc.
#      Это те же самые тарболлы (~4 ГБ) и тот же host-бутстрап (~1.8 ГБ,
#      несколько часов сборки), дублировать их бессмысленно. Каталог берётся
#      как есть; если его нет — материализуется делегированием в
#      target-linux/glibc (правила без предпосылок, поэтому уже собранное
#      никогда не пересобирается).
#
#   2. Общие исходники движка (zlib, expat, llvm-project, vulkan-*) — из
#      runtime/toolchains/src, ровно те же, что использует любой другой таргет.
#
#   3. Специфика устройства (glibc 2.39, busybox, dropbear, libxcrypt) — своя
#      src/ в этом каталоге.

# ── макрос загрузки ───────────────────────────────────────────────────────────
# $(1) = каталог распаковки (напр. src/mesa)
# $(2) = имя тарболла
# $(3) = URL
define download_tarball_target =
src/$(2):
	mkdir -p src
	wget -O src/$(2) $(3)
$(1): src/$(2)
	rm -rf $(1)
	mkdir -p $(1)
	cd src; tar -xf $(2) --strip-components=1 -C $(notdir $(1))
	touch $(1)
download: $(1)
endef


# ── 1. общая GNU-цепочка (target-linux/glibc) ─────────────────────────────────
GNU_TOOLCHAIN_DIR ?= $(abspath $(MAKE_ROOT)../../target-linux/glibc)
GNU_SRC_DIR := $(GNU_TOOLCHAIN_DIR)/src

GCC_SRC_DIR      := $(GNU_SRC_DIR)/gcc
BINUTILS_SRC_DIR := $(GNU_SRC_DIR)/binutils
LINUX_KERNEL_DIR := $(GNU_SRC_DIR)/linux
MAKE_SRC_DIR     := $(GNU_SRC_DIR)/make-4.3

# UAPI ядра. LTS 5.10 — то же, что у target-linux: консервативно и совместимо
# со всеми glibc от 2.26 до 2.39. Ядро самого устройства (linux-rpi 6.x)
# намеренно новее — mesa несёт собственные drm-uapi.
LINUX_KERNEL_TARBALL := linux-5.10.258.tar.xz

# Правила БЕЗ предпосылок: срабатывают, только если каталога физически нет,
# и потому не могут пометить устаревшим уже собранный host-бутстрап.
$(GCC_SRC_DIR):
	$(MAKE) -C $(GNU_TOOLCHAIN_DIR) download SP_ARCH_TARGET=$(SP_ARCH_HOST)

$(BINUTILS_SRC_DIR):
	$(MAKE) -C $(GNU_TOOLCHAIN_DIR) download SP_ARCH_TARGET=$(SP_ARCH_HOST)

$(LINUX_KERNEL_DIR):
	$(MAKE) -C $(GNU_TOOLCHAIN_DIR) download SP_ARCH_TARGET=$(SP_ARCH_HOST)

$(MAKE_SRC_DIR):
	$(MAKE) -C $(GNU_TOOLCHAIN_DIR) download SP_ARCH_TARGET=$(SP_ARCH_HOST)

# Заголовки ядра для целевой арки. Каталог общий с target-linux/glibc: набор
# UAPI зависит только от арки и версии ядра, а не от libc и не от вендора в
# триплете, так что переиспользование здесь корректно и экономит пересборку.
LINUX_TARGET_HEADERS_DIR := $(GNU_SRC_DIR)/linux-headers-target-$(SP_ARCH_TARGET)

$(LINUX_TARGET_HEADERS_DIR): | $(LINUX_KERNEL_DIR)
	mkdir -p $@
	cd $(LINUX_KERNEL_DIR); make headers_install ARCH=$(SP_ARCH_LINUX) \
		INSTALL_HDR_PATH=$@
	cd $(LINUX_KERNEL_DIR); make clean
	touch -m -r $(GNU_SRC_DIR)/$(LINUX_KERNEL_TARBALL) $(LINUX_KERNEL_DIR)


# ── 2. общие исходники движка ─────────────────────────────────────────────────
TOOLCHAINS_ROOT := $(abspath $(MAKE_ROOT)../..)
ENGINE_SRC_DIR  := $(TOOLCHAINS_ROOT)/src

ZLIB_SRC_DIR           := $(ENGINE_SRC_DIR)/zlib
EXPAT_SRC_DIR          := $(ENGINE_SRC_DIR)/expat
LLVM_SRC_DIR           := $(ENGINE_SRC_DIR)/llvm-project
VULKAN_HEADERS_SRC_DIR := $(ENGINE_SRC_DIR)/vulkan-headers
VULKAN_LOADER_SRC_DIR  := $(ENGINE_SRC_DIR)/vulkan-loader
VULKAN_TOOLS_SRC_DIR   := $(ENGINE_SRC_DIR)/vulkan-tools

# Правила движка объявлены на абсолютных путях (SRC_ROOT := <root>/src), так что
# каталог можно запросить у корневого Makefile по имени. Предпосылок нет — уже
# выкачанный чекаут никогда не перевыкачивается.
$(ZLIB_SRC_DIR) $(EXPAT_SRC_DIR) $(LLVM_SRC_DIR) \
$(VULKAN_HEADERS_SRC_DIR) $(VULKAN_LOADER_SRC_DIR) $(VULKAN_TOOLS_SRC_DIR):
	$(MAKE) -C $(TOOLCHAINS_ROOT) $@


# ── 3. специфика устройства ───────────────────────────────────────────────────

# glibc 2.39 — рантайм-libc устройства. Соответствует Ubuntu 24.04 (noble),
# поэтому бинари, собранные против этого sysroot, ведут себя на устройстве так
# же, как на референсной системе.
#
# ВАЖНО: это НЕ та же версия, что у target-linux (2.33). Именно поэтому таргет
# называется <arch>-xenolithos-linux-gnu, а не <arch>-unknown-linux-gnu —
# одинаковое имя при разном ABI приводило бы к перепутанным сборкам.
GLIBC_SRC_VER := 2.39
GLIBC_SRC_DIR := src/glibc-$(GLIBC_SRC_VER)

$(eval $(call download_tarball_target,$(GLIBC_SRC_DIR),glibc-$(GLIBC_SRC_VER).tar.xz,\
	https://ftp.gnu.org/gnu/glibc/glibc-$(GLIBC_SRC_VER).tar.xz))

# libxcrypt даёт crypt(3): glibc 2.39 больше не собирает libcrypt, а dropbear без
# неё не проверит пароль из /etc/shadow. --enable-obsolete-api (см. libs.mk)
# сохраняет SONAME libcrypt.so.1, как у дистрибутивной glibc.
LIBXCRYPT_VER := 4.4.38
LIBXCRYPT_SRC_DIR := src/libxcrypt

$(eval $(call download_tarball_target,$(LIBXCRYPT_SRC_DIR),libxcrypt-$(LIBXCRYPT_VER).tar.xz,\
	https://github.com/besser82/libxcrypt/releases/download/v$(LIBXCRYPT_VER)/libxcrypt-$(LIBXCRYPT_VER).tar.xz))

BUSYBOX_VER := 1.38.0
BUSYBOX_SRC_DIR := src/busybox

$(eval $(call download_tarball_target,$(BUSYBOX_SRC_DIR),busybox-$(BUSYBOX_VER).tar.bz2,\
	https://busybox.net/downloads/busybox-$(BUSYBOX_VER).tar.bz2))

DROPBEAR_VER := 2025.88
DROPBEAR_SRC_DIR := src/dropbear

$(eval $(call download_tarball_target,$(DROPBEAR_SRC_DIR),dropbear-$(DROPBEAR_VER).tar.bz2,\
	https://matt.ucc.asn.au/dropbear/releases/dropbear-$(DROPBEAR_VER).tar.bz2))


# ── провенанс ─────────────────────────────────────────────────────────────────
# Печатать перед долгой сборкой: неожиданный чекаут молча меняет ABI таргета.
sources:
	@echo "GNU toolchain (shared with target-linux/glibc): $(GNU_TOOLCHAIN_DIR)"
	@echo "  gcc            = $(GCC_SRC_DIR)"
	@echo "  binutils       = $(BINUTILS_SRC_DIR)"
	@echo "  linux          = $(LINUX_KERNEL_DIR)"
	@echo "  linux headers  = $(LINUX_TARGET_HEADERS_DIR)"
	@echo "  make           = $(MAKE_SRC_DIR)"
	@echo "engine src: $(ENGINE_SRC_DIR)"
	@echo "  zlib           = $(ZLIB_SRC_DIR)"
	@echo "  expat          = $(EXPAT_SRC_DIR)"
	@echo "  llvm-project   = $(LLVM_SRC_DIR)"
	@echo "  vulkan-headers = $(VULKAN_HEADERS_SRC_DIR)"
	@echo "  vulkan-loader  = $(VULKAN_LOADER_SRC_DIR)"
	@echo "  vulkan-tools   = $(VULKAN_TOOLS_SRC_DIR)"
	@echo "device src:"
	@echo "  glibc          = $(GLIBC_SRC_DIR)"
	@echo "  libxcrypt      = $(LIBXCRYPT_SRC_DIR)"
	@echo "  busybox        = $(BUSYBOX_SRC_DIR)"
	@echo "  dropbear       = $(DROPBEAR_SRC_DIR)"

.PHONY: sources download
