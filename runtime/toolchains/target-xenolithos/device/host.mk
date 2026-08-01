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

# Host-бутстрап (make 4.3 + glibc 2.26 + gcc 15.2) — ОБЩИЙ с target-linux/glibc.
#
# Этот gcc нужен только чтобы собрать target-binutils и минимальный target-gcc,
# которым собирается glibc устройства. Он не зависит ни от целевой арки, ни от
# версии целевой glibc, ни от вендора в триплете — то есть бутстрап target-linux
# подходит здесь буквально без изменений, а стоит он ~1.8 ГБ и несколько часов.
#
# Правила НАМЕРЕННО объявлены без предпосылок: make выполнит их, только если
# файла физически нет. Повесить сюда зависимости от исходников значило бы, что
# любое касание src/ в соседнем каталоге запускает многочасовую пересборку
# бутстрапа — при том, что его содержимое от этих исходников не изменилось.

HOST_SYSROOT   := $(GNU_TOOLCHAIN_DIR)/sysroot-host
HOST_GCC_CC    := $(HOST_SYSROOT)/bin/gcc
HOST_GCC_CXX   := $(HOST_SYSROOT)/bin/g++
HOST_GCC_MAKE  := $(HOST_SYSROOT)/bin/make

# g++ и make ставятся тем же бутстрапом и используются только как команды в
# рецептах, поэтому отдельных целей для них не заводим.
$(HOST_GCC_CC):
	$(MAKE) -C $(GNU_TOOLCHAIN_DIR) host SP_ARCH_TARGET=$(SP_ARCH_HOST)

host: $(HOST_GCC_CC)

.PHONY: host
