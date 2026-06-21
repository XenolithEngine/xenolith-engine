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

# Конфигурация исходников для host-тулчейна на Linux/musl.
#
# В отличие от glibc-хоста, здесь не нужно собирать GCC и «двойной» libc:
# на musl-Linux уже есть системный clang, которым мы бутстрапим всё остальное.
# Единственная C-библиотека, которую мы собираем — это запиненная musl из
# репозитория (сабмодуль runtime/musl-libc), используемая как наш sysroot.

GIT_TAG ?= $(shell git describe --tags --abbrev=0 2>/dev/null || echo dev)

# Параллелизм сборки
SP_NJOBS ?= -j$(shell nproc 2>/dev/null || echo 8)

# Архитектура хоста
SP_ARCH ?= $(shell uname -m)
SP_ARCH_CLANG ?= $(SP_ARCH)-unknown-linux-musl
SP_LLVM_VER := 21

# LLVM-имя backend-а для текущей архитектуры
ifeq ($(SP_ARCH),x86_64)
SP_ARCH_LLVM ?= X86
else ifeq ($(SP_ARCH),aarch64)
SP_ARCH_LLVM ?= AArch64
else ifeq ($(SP_ARCH),riscv64)
SP_ARCH_LLVM ?= RISCV
else
SP_ARCH_LLVM ?= X86
endif

# Бутстрап-компилятор. На musl-Linux (Alpine и т.п.) это системный clang/lld.
# Должен уметь -stdlib=libc++ и -fuse-ld=lld; см. README (Prerequisites).
BOOTSTRAP_CC ?= clang
BOOTSTRAP_CXX ?= clang++

# Запиненная musl: сабмодуль runtime/musl-libc (см. .git -> .git/modules/musl-libc).
# Путь относительно host-linux-musl/: ../../musl-libc == runtime/musl-libc.
# Сборка ведётся out-of-tree, исходники сабмодуля не модифицируются.
MUSL_SRC_DIR := $(realpath ../../musl-libc)

ifeq ($(MUSL_SRC_DIR),)
$(error musl sources not found at ../../musl-libc \
    (git submodule update --init runtime/musl-libc))
endif

# Остальные исходники переиспользуются из общего toolchains/src,
# который наполняется через 'make download' в runtime/toolchains.
SP_SRC_DIR ?= ../src

LLVM_DIR := $(realpath $(SP_SRC_DIR)/llvm-project)

ifeq ($(LLVM_DIR),)
$(error LLVM not found, try 'make download' from runtime/toolchains)
endif

VULKAN_HEADERS_DIR := $(realpath $(SP_SRC_DIR)/vulkan-headers)
SPIRV_HEADERS_DIR  := $(realpath $(SP_SRC_DIR)/spirv-headers)
GLSLANG_DIR        := $(realpath $(SP_SRC_DIR)/glslang)
SPIRV_TOOLS_DIR    := $(realpath $(SP_SRC_DIR)/spirv-tools)

# Linux kernel headers (запиненные). Нужны как minimum libc++ (linux/futex.h
# в atomic-wait) и ряду libc-зависимых частей. Версия — как у glibc-хоста
# (наименьшая поддерживаемая LTS), чтобы musl+headers были версионно согласованы.
LINUX_KERNEL_FAMILY ?= v5.x
LINUX_KERNEL_VER ?= 5.10.258
LINUX_KERNEL_TARBALL := linux-$(LINUX_KERNEL_VER).tar.xz
LINUX_KERNEL_URL := https://cdn.kernel.org/pub/linux/kernel/$(LINUX_KERNEL_FAMILY)/$(LINUX_KERNEL_TARBALL)
LINUX_KERNEL_SRC := src/linux-$(LINUX_KERNEL_VER)
LINUX_HEADERS_DIR := $(abspath src/linux-headers-$(SP_ARCH))
LINUX_HEADERS := $(LINUX_HEADERS_DIR)/include/linux/futex.h

# SP_ARCH -> kernel ARCH
ifeq ($(SP_ARCH),x86_64)
SP_ARCH_KERNEL ?= x86
else ifeq ($(SP_ARCH),aarch64)
SP_ARCH_KERNEL ?= arm64
else ifeq ($(SP_ARCH),riscv64)
SP_ARCH_KERNEL ?= riscv
else
SP_ARCH_KERNEL ?= $(SP_ARCH)
endif

src/$(LINUX_KERNEL_TARBALL):
	mkdir -p src
	wget -O $@ $(LINUX_KERNEL_URL)

$(LINUX_HEADERS): src/$(LINUX_KERNEL_TARBALL)
	rm -rf $(LINUX_KERNEL_SRC) $(LINUX_HEADERS_DIR)
	cd src; tar xf $(LINUX_KERNEL_TARBALL)
	$(MAKE) -C $(LINUX_KERNEL_SRC) headers_install ARCH=$(SP_ARCH_KERNEL) INSTALL_HDR_PATH=$(LINUX_HEADERS_DIR)
	$(MAKE) -C $(LINUX_KERNEL_SRC) clean
