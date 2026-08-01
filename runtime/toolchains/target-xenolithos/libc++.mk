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

# Разделяемые libc++ / libc++abi / libunwind — рантайм C++ ОБРАЗА устройства.
#
# Отдельная сборка, а не ключ в target-linux/libc++.mk: у обычных Linux-таргетов
# рантайм C++ даёт операционная система, движок линкуется статически, и .so там
# не нужны вовсе. Их набор остаётся ровно таким, каким был.
#
# Здесь операционной системы нет: рантайм несёт сам образ, и в одном процессе
# живут приложение, лоадер Vulkan и драйвер GPU (mesa) — три независимо
# собранных образа. Каждому нужен C++-ABI, и он обязан быть ОДИН на процесс:
# __cxa_guard_*, хранилища new/terminate-хендлеров, operator new/delete и пара
# __dynamic_cast + vtable'ы __cxxabiv1::*_type_info — это состояние процесса, а
# не библиотеки. Разделяемая libc++abi.so.1 в rootfs решает это ровно так же,
# как точечный экспорт ABI из исполняемого файла (make/os/linux.mk) решает это
# на десктопном Linux, где libc++abi статическая.
#
# СТАТИЧЕСКАЯ ПОЛОВИНА СЮДА НЕ ВХОДИТ. libc++.a / libc++abi.a / libunwind.a
# кладёт общий набор (target-linux/libc++.mk) шагом раньше — на libc++abi.a
# рассчитывает `-l:libc++abi.a` из runtime/runtime.mk, и трогать её нельзя.
# Поэтому здесь ENABLE_STATIC выключены у всех трёх: сборка ставит только .so,
# симлинки SONAME и линкер-скрипт libc++.so. Заголовки те же самые и
# перезаписываются идентичными (в готовый таргет usr/include/c++ всё равно не
# уезжает — см. install-target.mk).
#
# Про разматыватель. LIBCXXABI_ENABLE_STATIC_UNWINDER здесь НЕ задаётся, и это
# существенно: он делает LIBCXXABI_STATICALLY_LINK_UNWINDER_IN_SHARED_LIBRARY
# доступным со значением ON по умолчанию, а тогда libc++abi ВКОМПИЛИРОВЫВАЕТ
# разматыватель прямо в libc++abi.so.1. Рядом в rootfs лежит libunwind.so.1, на
# которую ссылается шим libgcc_s.so.1 — через него glibc делает pthread_exit,
# pthread_cancel и backtrace(). Две копии разматывателя с раздельным состоянием
# в одном процессе — тот самый cross-talk, о котором предупреждает документация
# llvm-libgcc. Без этого ключа libc++abi.so.1 получает DT_NEEDED на
# libunwind.so.1, и копия остаётся одна.
#
# Про исключения. Здесь EXCEPTIONS=ON, в отличие от статической половины, и это
# намеренное расхождение. Статическую линкует движок, который собран
# -fno-exceptions и не бросает вообще; а эта — рантайм C++ ОБРАЗА, и её
# потребители другие: лоадер Vulkan, vulkaninfo и драйвер GPU (mesa) — обычный
# сторонний C++, который исключения использует. С OFF внеконтурные бросающие
# хелперы внутри самой библиотеки (std::stoi и родня) звали бы abort вместо
# throw, и `catch` в драйвере до обработчика бы не дошёл.
#
# Одного ключа cmake недостаточно: SP_OPT рантайма несёт -fno-exceptions, и он
# приезжает в CMAKE_CXX_FLAGS_INIT через configure.mk, где перебивает всё, что
# добавляет cxx_add_exception_flags. Поэтому SP_OPT для этой сборки подменяется
# в target-xenolithos/Makefile (libcxx_vars) — без этого флаг был бы пустышкой.
#
# На заголовки ключ не влияет: __config_site у обеих сборок побайтово одинаков,
# LIBCXX_ENABLE_EXCEPTIONS в него не попадает. То есть ABI не расходится, и
# статическая половина остаётся ровно такой, какой её ждёт движок.

.DEFAULT_GOAL := all

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

LIBNAME = llvm-project

include ../common/configure.mk

LIBCXX_SHARED_LIB := $(SP_INSTALL_PREFIX)/usr/lib/libc++.so.1

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLVM_ENABLE_PIC=On \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_HOST_TRIPLE="$(SP_TARGET)" \
	-DLLVM_DEFAULT_TARGET_TRIPLE="$(SP_TARGET)" \
	-DLIBCXX_ENABLE_EXCEPTIONS=ON \
	-DLIBCXX_HAS_ATOMIC_LIB=Off \
	-DLIBCXX_ENABLE_SHARED=On \
	-DLIBCXX_ENABLE_STATIC=Off \
	-DLIBCXX_USE_COMPILER_RT=On \
	-DLIBCXX_CXX_ABI=libcxxabi \
	-DLIBCXX_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_ENABLE_EXCEPTIONS=ON \
	-DLIBCXXABI_USE_LLVM_UNWINDER=On \
	-DLIBCXXABI_USE_COMPILER_RT=On \
	-DLIBCXXABI_ENABLE_SHARED=On \
	-DLIBCXXABI_ENABLE_STATIC=Off \
	-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBUNWIND_USE_COMPILER_RT=On \
	-DLIBUNWIND_ENABLE_SHARED=On \
	-DLIBUNWIND_ENABLE_STATIC=Off \
	-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
	-DCMAKE_BUILD_TYPE=Release

# Предпосылка — сам этот файл: полная пересборка libc++ занимает минуты, и
# гонять её на каждый `make` таргета незачем. Меняется конфигурация — меняется
# файл, и цель устаревает.
$(LIBCXX_SHARED_LIB): $(LOCAL_MAKEFILE)
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" -S $(LIB_SRC_DIR)/$(LIBNAME)/runtimes $(CONFIGURE)
	cd $(LIBNAME); cmake --build . --config Release --target install-cxx
	cd $(LIBNAME); cmake --build . --config Release --target install-cxxabi
	cd $(LIBNAME); cmake --build . --config Release --target install-unwind
	$(call rule_rm,$(LIBNAME))

all: $(LIBCXX_SHARED_LIB)

.PHONY: all
