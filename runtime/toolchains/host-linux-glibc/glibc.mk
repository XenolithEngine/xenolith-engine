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

# Нам нужно чем-то собрать начальную версию clang и glibc. Почему бы не GCC.
# https://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.gz
GCC_SRC_VER := 15.2.0
GCC_SRC_TARBALL := gcc-$(GCC_SRC_VER).tar.xz
GCC_SRC_DIR := src/gcc
GCC_SRC_URL := https://ftp.gnu.org/gnu/gcc/gcc-$(GCC_SRC_VER)/gcc-$(GCC_SRC_VER).tar.gz

gmp_TARBALL = gmp-6.2.1.tar.bz2
mpfr_TARBALL = mpfr-4.1.0.tar.bz2
mpc_TARBALL = mpc-1.2.1.tar.gz
isl_TARBALL = isl-0.24.tar.bz2
gettext_TARBALL = gettext-0.22.tar.gz

define download_tarbell_target =
src/$(2):
	mkdir -p src
	wget -O src/$(2) $(3)
$(1): src/$(2)
	rm -rf $(1)
	mkdir -p $(1)
	cd src; tar -xf $(2) --strip-components=1 -C $(notdir $(1))
download: $(1)
endef

$(eval $(call download_tarbell_target,$(GCC_SRC_DIR),gcc-$(GCC_SRC_VER).tar.gz,$(GCC_SRC_URL)))

$(eval $(call download_tarbell_target,src/gmp,$(gmp_TARBALL),\
	https://gcc.gnu.org/pub/gcc/infrastructure/$(gmp_TARBALL)))

$(eval $(call download_tarbell_target,src/mpfr,$(mpfr_TARBALL),\
	https://gcc.gnu.org/pub/gcc/infrastructure/$(mpfr_TARBALL)))

$(eval $(call download_tarbell_target,src/mpc,$(mpc_TARBALL),\
	https://gcc.gnu.org/pub/gcc/infrastructure/$(mpc_TARBALL)))

$(eval $(call download_tarbell_target,src/isl,$(isl_TARBALL),\
	https://gcc.gnu.org/pub/gcc/infrastructure/$(isl_TARBALL)))

$(eval $(call download_tarbell_target,src/gettext,$(gettext_TARBALL),\
	https://gcc.gnu.org/pub/gcc/infrastructure/$(gettext_TARBALL)))


# Заголовки и glibc должны соотвествовать друг другу и быть минимально доступной версии
# Используем LTS ядро 5.10

LINUX_KERNEL_FAMILY := v5.x
LINUX_KERNEL_VER := 5.10.251
LINUX_KERNEL_TARBALL := linux-$(LINUX_KERNEL_VER).tar.xz
LINUX_KERNEL_DIR := src/linux
LINUX_KERNEL_URL := https://cdn.kernel.org/pub/linux/kernel/$(LINUX_KERNEL_FAMILY)/linux-$(LINUX_KERNEL_VER).tar.xz

$(eval $(call download_tarbell_target,$(LINUX_KERNEL_DIR),$(LINUX_KERNEL_TARBALL),$(LINUX_KERNEL_URL)))


# Используем 2.26 для первоначального запуска, поскольку более поздние требуют libstdc++
GLIBC_MIN_SRC_VER ?= 2.26
GLIBC_MIN_SRC_TARBALL := glibc-$(GLIBC_MIN_SRC_VER).tar.xz
GLIBC_MIN_SRC_DIR := src/glibc-$(GLIBC_MIN_SRC_VER)
GLIBC_MIN_SRC_URL := https://ftp.gnu.org/gnu/glibc/glibc-$(GLIBC_MIN_SRC_VER).tar.xz

$(eval $(call download_tarbell_target,$(GLIBC_MIN_SRC_DIR),$(GLIBC_MIN_SRC_TARBALL),$(GLIBC_MIN_SRC_URL)))


# Используем 2.33 как ближайшую к целевой версии ядра
GLIBC_SRC_VER ?= 2.33
GLIBC_SRC_TARBALL := glibc-$(GLIBC_SRC_VER).tar.xz
GLIBC_SRC_DIR := src/glibc-$(GLIBC_SRC_VER)
GLIBC_SRC_URL := https://ftp.gnu.org/gnu/glibc/glibc-$(GLIBC_SRC_VER).tar.xz

$(eval $(call download_tarbell_target,$(GLIBC_SRC_DIR),$(GLIBC_SRC_TARBALL),$(GLIBC_SRC_URL)))


# https://ftp.gnu.org/gnu/make/make-4.3.tar.lz
# Нам нужен make-4.3 чтобы собирать старые версии glibc
# Более новые версии будут уходить в бесконечный цикл
MAKE_SRC_VER := 4.3
MAKE_SRC_TARBALL := make-$(MAKE_SRC_VER).tar.lz
MAKE_SRC_DIR := src/make-$(MAKE_SRC_VER)
MAKE_SRC_URL := https://ftp.gnu.org/gnu/make/make-$(MAKE_SRC_VER).tar.lz

$(eval $(call download_tarbell_target,$(MAKE_SRC_DIR),$(MAKE_SRC_TARBALL),$(MAKE_SRC_URL)))


ZLIB_DIR := $(realpath ../src/zlib)

ifeq ($(ZLIB_DIR),)
$(error ZLib not found, try 'make download' from toolchains)
endif

LLVM_DIR := $(realpath ../src/llvm-project)

ifeq ($(LLVM_DIR),)
$(error LLVM not found, try 'make download' from toolchains)
endif
