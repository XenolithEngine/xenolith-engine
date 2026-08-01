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

# Кросс-libc + кросс-тулчейн для target <arch>-unknown-linux-musl.
#
# Аналог target-linux/glibc, но кардинально проще: host-clang (из hosts/<id>)
# мульти-таргетный, поэтому musl и рантаймы кросс-компилируются напрямую — без
# binutils и кросс-GCC. Производит промежуточный sysroot (OUT_SYSROOT) с musl,
# kernel headers, compiler-rt (resource dir), toolchain.cmake/target.mk/target.ini
# и симлинком host -> hosts/<id>. Сторонние библиотеки + libc++ доустанавливает
# родительский target-linux-musl/Makefile.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

SP_NJOBS ?= -j$(shell nproc 2>/dev/null || echo 8)
SP_ARCH_TARGET ?= x86_64
SP_ARCH_TARGET_CLANG := $(SP_ARCH_TARGET)-unknown-linux-musl
SP_LLVM_VER := 21

ifeq ($(SP_ARCH_TARGET),x86_64)
SP_ARCH_KERNEL := x86
SP_ARCH_LLVM := X86
else ifeq ($(SP_ARCH_TARGET),aarch64)
SP_ARCH_KERNEL := arm64
SP_ARCH_LLVM := AArch64
else ifeq ($(SP_ARCH_TARGET),riscv64)
SP_ARCH_KERNEL := riscv
SP_ARCH_LLVM := RISCV
else
$(error Unknown target arch: $(SP_ARCH_TARGET))
endif

SP_LDSO_NAME := ld-musl-$(SP_ARCH_TARGET).so.1

SP_ARCH_HOST := $(shell uname -m)

# Пинённая musl (сабмодуль runtime/musl-libc) и общий LLVM-исходник.
MUSL_SRC_DIR := $(realpath ../../../musl-libc)
LLVM_DIR := $(realpath ../../src/llvm-project)

ifeq ($(MUSL_SRC_DIR),)
$(error musl sources not found (git submodule update --init runtime/musl-libc))
endif
ifeq ($(LLVM_DIR),)
$(error LLVM not found, run 'make download' in runtime/toolchains)
endif

# host-тулчейн (мульти-таргетный clang). Передаётся родителем как OUT_HOST.
OUT_HOST ?= $(abspath ../../hosts/$(SP_ARCH_HOST)-unknown-linux-musl)
SP_HOST_CLANG := $(abspath $(OUT_HOST)/bin/clang)

OUT_SYSROOT ?= $(SP_ARCH_TARGET_CLANG)
ABS_SYSROOT := $(abspath $(OUT_SYSROOT))
RESOURCE_DIR := $(ABS_SYSROOT)/lib/clang

# Кросс-компилятор: host-clang + --target + resource-dir target-а.
TARGET_CC := $(SP_HOST_CLANG) --target=$(SP_ARCH_TARGET_CLANG) -resource-dir $(RESOURCE_DIR)

# Архиватор: arch-агностичный llvm-ar/llvm-ranlib host-а (musl по --host
# ожидал бы x86_64-unknown-linux-musl-ar, которого нет).
HOST_AR := $(abspath $(OUT_HOST)/bin/llvm-ar)
HOST_RANLIB := $(abspath $(OUT_HOST)/bin/llvm-ranlib)

#
# Linux kernel headers (пинённые, как у host/glibc — наименьшая LTS)
#
LINUX_KERNEL_VER ?= 5.10.258
LINUX_KERNEL_TARBALL := linux-$(LINUX_KERNEL_VER).tar.xz
LINUX_KERNEL_URL := https://cdn.kernel.org/pub/linux/kernel/v5.x/$(LINUX_KERNEL_TARBALL)
LINUX_KERNEL_SRC := src/linux-$(LINUX_KERNEL_VER)
LINUX_HEADERS_DIR := $(abspath src/linux-headers-$(SP_ARCH_TARGET))
LINUX_HEADERS := $(LINUX_HEADERS_DIR)/include/linux/futex.h

src/$(LINUX_KERNEL_TARBALL):
	mkdir -p src
	wget -O $@ $(LINUX_KERNEL_URL)

$(LINUX_HEADERS): src/$(LINUX_KERNEL_TARBALL)
	rm -rf $(LINUX_KERNEL_SRC) $(LINUX_HEADERS_DIR)
	cd src; tar xf $(LINUX_KERNEL_TARBALL)
	$(MAKE) -C $(LINUX_KERNEL_SRC) headers_install ARCH=$(SP_ARCH_KERNEL) INSTALL_HDR_PATH=$(LINUX_HEADERS_DIR)
	$(MAKE) -C $(LINUX_KERNEL_SRC) clean

#
# 1. musl-заголовки + kernel headers -> OUT_SYSROOT/include (libc-заголовки).
#    install-headers компилятора не требует.
#
MUSL_HEADERS_STAMP := $(ABS_SYSROOT)/include/stdlib.h

$(MUSL_HEADERS_STAMP): $(LINUX_HEADERS)
	@echo "Install musl + kernel headers -> $(OUT_SYSROOT)/include"
	mkdir -p $(ABS_SYSROOT)
	rm -rf build/musl-headers
	mkdir -p build/musl-headers
	cd build/musl-headers; $(MUSL_SRC_DIR)/configure \
		--prefix=$(ABS_SYSROOT) \
		--syslibdir=$(ABS_SYSROOT)/lib \
		--host=$(SP_ARCH_TARGET_CLANG) \
		CC="$(SP_HOST_CLANG) --target=$(SP_ARCH_TARGET_CLANG)"
	$(MAKE) -C build/musl-headers install-headers
	cp -rn $(LINUX_HEADERS_DIR)/include/* $(ABS_SYSROOT)/include/
	# Резолв-дир: симлинк host + заголовки clang (emmintrin.h и пр.) сразу,
	# чтобы они были доступны всем последующим сборкам (compiler-rt/рантаймы/либы).
	rm -f $(ABS_SYSROOT)/host
	ln -sfn $(abspath $(OUT_HOST)) $(ABS_SYSROOT)/host
	mkdir -p $(RESOURCE_DIR)
	ln -sfn ../../host/lib/clang/$(SP_LLVM_VER)/include $(RESOURCE_DIR)/include

#
# 2. compiler-rt builtins (+ crt) для target -> resource-dir.
#    Собирается host-clang под musl; libc уже представлен заголовками.
#
TARGET_TOOLCHAIN_CMAKE := $(ABS_SYSROOT)/compiler-rt.cmake
TARGET_BUILTINS := $(RESOURCE_DIR)/lib/linux/libclang_rt.builtins-$(SP_ARCH_TARGET).a

$(TARGET_TOOLCHAIN_CMAKE): $(MUSL_HEADERS_STAMP)
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH_TARGET))' >> $@
	@echo 'set(CMAKE_SYSROOT $(ABS_SYSROOT))' >> $@
	@echo 'set(CMAKE_C_COMPILER $(SP_HOST_CLANG))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER $(abspath $(OUT_HOST)/bin/clang++))' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_TARGET_CLANG))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_TARGET_CLANG))' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET $(SP_ARCH_TARGET_CLANG))' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "-resource-dir $(RESOURCE_DIR)")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "-resource-dir $(RESOURCE_DIR)")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "-resource-dir $(RESOURCE_DIR)")' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@

# Ставим во временный префикс и копируем ТОЛЬКО lib/linux в resource-dir, чтобы
# compiler-rt не создал реальный каталог lib/clang/include и не сломал симлинк
# на заголовки host-clang (emmintrin.h и пр.).
$(TARGET_BUILTINS): $(TARGET_TOOLCHAIN_CMAKE)
	@echo "Build target compiler-rt builtins ($(SP_ARCH_TARGET_CLANG))"
	rm -rf build/compiler-rt build/compiler-rt-install
	cmake -G Ninja -S $(LLVM_DIR)/runtimes -B build/compiler-rt \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_TOOLCHAIN_CMAKE) \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt" \
		-DLLVM_TARGETS_TO_BUILD=$(SP_ARCH_LLVM) \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_CRT=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_BUILD_ORC=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=On \
		-DCMAKE_INSTALL_PREFIX=$(abspath build/compiler-rt-install)
	cmake --build build/compiler-rt
	cmake --install build/compiler-rt
	mkdir -p $(RESOURCE_DIR)/lib
	rm -rf $(RESOURCE_DIR)/lib/linux
	cp -rf build/compiler-rt-install/lib/linux $(RESOURCE_DIR)/lib/linux

#
# 3. musl libc (full) -> OUT_SYSROOT/lib (libc.a/.so, crt*.o, ld-musl-*.so.1).
#    Линкуется host-clang с compiler-rt builtins из resource-dir.
#
TARGET_MUSL := $(ABS_SYSROOT)/lib/libc.a

$(TARGET_MUSL): $(TARGET_BUILTINS)
	@echo "Build target musl ($(SP_ARCH_TARGET_CLANG))"
	rm -rf build/musl
	mkdir -p build/musl
	cd build/musl; $(MUSL_SRC_DIR)/configure \
		--prefix=$(ABS_SYSROOT) \
		--syslibdir=$(ABS_SYSROOT)/lib \
		--host=$(SP_ARCH_TARGET_CLANG) \
		CC="$(TARGET_CC)" \
		AR="$(HOST_AR)" \
		RANLIB="$(HOST_RANLIB)" \
		LIBCC="$(RESOURCE_DIR)/lib/linux/libclang_rt.builtins-$(SP_ARCH_TARGET).a"
	$(MAKE) -C build/musl $(SP_NJOBS)
	$(MAKE) -C build/musl install

#
# 4. Рантаймы LLVM (libunwind + libc++abi + libc++) для target -> usr/lib.
#    Делают sysroot самодостаточным: compiler-check сторонних библиотек линкует
#    -lunwind (host-clang дефолтит --unwindlib=libunwind), поэтому libunwind.a
#    обязан присутствовать ДО их сборки (аналог glibc/, кладущего libunwind.a).
#
#    Это КАНОНИЧЕСКИЙ сборщик target libc++/libc++abi для musl: rule libc++.mk в
#    target-linux/Makefile пропускается, т.к. usr/lib/libc++.a уже создан здесь.
#    Поэтому musl-специфичные флаги libc++.mk (LIBCXX_HAS_MUSL_LIBC,
#    LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF) ОБЯЗАНЫ дублироваться тут.
#    Без OFF libc++abi оставляет жёсткую ссылку на __cxa_thread_atexit_impl,
#    которого нет в musl libc.a -> ld.lld: undefined symbol на линковке.
#
TARGET_RUNTIMES := $(ABS_SYSROOT)/usr/lib/libc++.a

$(TARGET_RUNTIMES): $(TARGET_MUSL)
	@echo "Build target runtimes (libc++/libc++abi/libunwind) for $(SP_ARCH_TARGET_CLANG)"
	rm -rf build/runtimes
	cmake -G Ninja -S $(LLVM_DIR)/runtimes -B build/runtimes \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_TOOLCHAIN_CMAKE) \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
		-DLLVM_TARGETS_TO_BUILD=$(SP_ARCH_LLVM) \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DLLVM_ENABLE_PIC=On \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXX_INSTALL_LIBRARY_DIR=usr/lib \
		-DLIBCXX_INSTALL_INCLUDE_DIR=usr/include/c++/v1 \
		-DLIBCXX_INSTALL_INCLUDE_TARGET_DIR=usr/include/c++/v1 \
		-DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_SHARED=Off \
		-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=Off \
		-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
		-DLIBUNWIND_INSTALL_INCLUDE_DIR=usr/include \
		-DCMAKE_INSTALL_PREFIX=$(ABS_SYSROOT)
	cmake --build build/runtimes
	cmake --install build/runtimes

#
# Toolchain-файлы для сборки сторонних библиотек (libc-агностичная машинерия
# target-linux/common ожидает именно такой layout).
#
$(ABS_SYSROOT)/toolchain.cmake: $(LOCAL_MAKEFILE)
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR "$(SP_ARCH_TARGET)")' >> $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)" CACHE STRING "" FORCE)' >> $@
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

$(ABS_SYSROOT)/target.mk: $(LOCAL_MAKEFILE)
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Linux' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH_TARGET)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/include_libc' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang -lc++abi' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@

$(ABS_SYSROOT)/target.ini: $(LOCAL_MAKEFILE)
	@echo 'Build $@'
	@echo '[host_machine]' > $@
	@echo "system = 'linux'" >> $@
	@echo "cpu_family = '$(SP_ARCH_TARGET)'" >> $@
	@echo "cpu = '$(SP_ARCH_TARGET)'" >> $@
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
	@echo "c_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@
	@echo "c_link_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@
	@echo "cpp_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@
	@echo "cpp_link_args = ['-resource-dir','@DIRNAME@/lib/clang','--target=$(SP_ARCH_TARGET_CLANG)','--sysroot=@DIRNAME@']" >> $@

$(ABS_SYSROOT)/sysroot: $(TARGET_MUSL) $(TARGET_BUILTINS) $(TARGET_RUNTIMES) \
		$(ABS_SYSROOT)/toolchain.cmake $(ABS_SYSROOT)/target.mk $(ABS_SYSROOT)/target.ini
	# host-симлинк и resource-include уже созданы на шаге заголовков; обновим host
	# на случай изменившегося OUT_HOST.
	rm -f $(ABS_SYSROOT)/host
	ln -sfn $(abspath $(OUT_HOST)) $(ABS_SYSROOT)/host
	touch $@

download: $(LINUX_HEADERS)

out: $(ABS_SYSROOT)/sysroot

all: out

.PHONY: all out download
