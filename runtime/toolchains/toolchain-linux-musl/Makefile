# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# Сборка разделена на 3 этапа
# На первом этапе используется musl-cross-make для создания GCC-тулчейна,
# на базе которого создаётся гибридный GCC/Clang тулчейн
#
# На втором этапе на базе гибридного тулчейна собирается промежуточный
# clang-тулчейн.
#
# На третьем этапе собирается финальный clang-тулчейн

MUSL_GCC_SYSROOT ?= toolchain-gcc
MUSL_CLANG_STAGE1_SYSROOT ?= toolchain-clang-stage1
MUSL_ARCH ?= x86_64
MUSL_ARCH_GCC ?= $(MUSL_ARCH)-linux-musl
MUSL_ARCH_CLANG ?= $(MUSL_ARCH)-unknown-linux-musl

MUSL_CROSS_MAKE_SRC = https://github.com/richfelker/musl-cross-make.git
MUSL_CROSS_MAKE ?= musl-cross-make
MUSL_SRC := $(MUSL_CROSS_MAKE)/musl-1.2.5.orig

LLVM_MAJOR := 21
LLVM_V := $(LLVM_MAJOR).1.4

MUSL_GCC_V := 15.1.0
MUSL_GCC_CC := $(MUSL_GCC_SYSROOT)/bin/$(MUSL_ARCH_GCC)-gcc
MUSL_GCC_CXX := $(MUSL_GCC_SYSROOT)/bin/$(MUSL_ARCH_GCC)-g++
MUSL_GCC_LIBCXX := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib/libc++.a
MUSL_GCC_LIBCRT := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib/linux/liborc_rt-$(MUSL_ARCH).a
MUSL_GCC_CLANG := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/bin/clang-check

MUSL_GCC_CRTLIB := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib/clang/$(LLVM_MAJOR)/lib/x86_64-unknown-linux-musl

MUSL_GCC_LDD := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/bin/ldd
MUSL_GCC_ZLIB := $(MUSL_GCC_SYSROOT)/lib/libz.a
MUSL_GCC_ZLIB_ARCH := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib/libz.a

MUSL_GCC_CMAKE_GCC_TOOLCHAIN := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)-gcc.cmake
MUSL_GCC_CMAKE_CLANG_TOOLCHAIN := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)-clang.cmake
MUSL_GCC_LD_PATH := $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/etc/ld-musl-$(MUSL_ARCH).path

MUSL_GCC_RPATH := $(MUSL_GCC_SYSROOT)/lib;$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib

MUSL_CLANG_SYSROOT ?= toolchain-clang0
MUSL_CLANG_CC := $(MUSL_CLANG_SYSROOT)/bin/clang
MUSL_CLANG_CXX := $(MUSL_CLANG_SYSROOT)/bin/clang++
MUSL_CLANG_LIBC := $(MUSL_CLANG_SYSROOT)/lib/libc.so
MUSL_CLANG_LIBCXX := $(MUSL_CLANG_SYSROOT)/lib/libc++.a
MUSL_CLANG_CLANG := $(MUSL_CLANG_SYSROOT)/bin/clang-check
MUSL_CLANG_ZLIB := $(MUSL_CLANG_SYSROOT)/lib/libz.a
MUSL_CLANG_CMAKE_CLANG_TOOLCHAIN := $(MUSL_CLANG_SYSROOT)/$(MUSL_ARCH_CLANG)-clang.cmake

MUSL_FINAL_SYSROOT ?= toolchain-final
MUSL_FINAL_CC := $(MUSL_FINAL_SYSROOT)/bin/clang
MUSL_FINAL_CXX := $(MUSL_FINAL_SYSROOT)/bin/clang++
MUSL_FINAL_LIBC := $(MUSL_FINAL_SYSROOT)/lib/libc.so
MUSL_FINAL_CLANG := $(MUSL_FINAL_SYSROOT)/bin/clang-check
MUSL_FINAL_ZLIB := $(MUSL_FINAL_SYSROOT)/lib/libz.a
MUSL_FINAL_CMAKE_CLANG_TOOLCHAIN := $(MUSL_FINAL_SYSROOT)/$(MUSL_ARCH_CLANG)-clang.cmake

STATIC_SYSROOT ?= toolchain-static
STATIC_CC := $(STATIC_SYSROOT)/bin/clang
STATIC_CXX := $(STATIC_SYSROOT)/bin/clang++
STATIC_LIBC := $(STATIC_SYSROOT)/lib/libc.a
STATIC_CLANG := $(STATIC_SYSROOT)/bin/clang-$(LLVM_MAJOR)
STATIC_ZLIB := $(STATIC_SYSROOT)/lib/libz.a
STATIC_CMAKE_CLANG_TOOLCHAIN := $(STATIC_SYSROOT)/$(MUSL_ARCH_CLANG)-clang.cmake

STATIC_SYSROOT_MIN ?= toolchain-static-min

OUTPUT ?= $(abspath ../$(MUSL_ARCH_CLANG))

INCLUDE_SOURCE ?= /usr/include

LLVM_MAJOR := 21
LLVM_V := $(LLVM_MAJOR).1.4
LLVM_VER := llvm-project-$(LLVM_V).src
LLVM_TARBALL := $(addsuffix .tar.xz,$(LLVM_VER))
LLVM_URL := https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_V)/llvm-project-$(LLVM_V).src.tar.xz

### ВАЖНО
# MUSL_CROSS_MAKE использует сервера, которые могут блокировать запросы на загрузку из России
# Используйте VPN или загрузките пакеты вручную

$(MUSL_CROSS_MAKE):
	git clone $(MUSL_CROSS_MAKE_SRC) $(MUSL_CROSS_MAKE)
	sed  -i 's/GCC_VER = 9.4.0/GCC_VER = $(MUSL_GCC_V)/g' $(MUSL_CROSS_MAKE)/Makefile
	cd $(MUSL_CROSS_MAKE); make TARGET=$(MUSL_ARCH_GCC)

$(LLVM_TARBALL):
	wget -O $(LLVM_TARBALL) $(LLVM_URL)

$(LLVM_VER): $(LLVM_TARBALL)
	tar -xf $(LLVM_TARBALL)
	touch $(LLVM_VER)

ZLIB_VER := zlib-1.3.1
ZLIB_TARBALL := $(addsuffix .tar.gz,$(ZLIB_VER))
ZLIB_URL := https://www.zlib.net/$(ZLIB_TARBALL)

$(ZLIB_TARBALL):
	wget -O $(ZLIB_TARBALL) $(ZLIB_URL)

$(ZLIB_VER): $(ZLIB_TARBALL)
	tar -xf $(ZLIB_TARBALL)
	touch $(ZLIB_VER)

$(MUSL_GCC_CC): $(MUSL_GCC_SYSROOT)/sysroot
	$(MAKE) -C $(MUSL_CROSS_MAKE) TARGET=$(MUSL_ARCH_GCC) OUTPUT=$(realpath $(MUSL_GCC_SYSROOT)) install
	cd $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/bin; ln -s ../lib/libc.so ldd
	cd $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib; rm ld-musl-$(MUSL_ARCH).so.1; ln -s libc.so ld-musl-$(MUSL_ARCH).so.1

$(MUSL_GCC_LD_PATH): $(MUSL_GCC_SYSROOT)/sysroot
	@mkdir -p $(dir $(MUSL_GCC_LD_PATH))
	@echo "$(realpath $(MUSL_GCC_SYSROOT))/lib" > $@
	@echo "$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/lib" >> $@

$(MUSL_GCC_CMAKE_GCC_TOOLCHAIN): $(MUSL_GCC_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(MUSL_GCC_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(MUSL_ARCH_GCC))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(MUSL_ARCH_GCC))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(MUSL_GCC_SYSROOT))/bin/$(MUSL_ARCH_GCC)-gcc)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(MUSL_GCC_SYSROOT))/bin/$(MUSL_ARCH_GCC)-g++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@

$(MUSL_GCC_CMAKE_CLANG_TOOLCHAIN): $(MUSL_GCC_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(MUSL_ARCH_GCC))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(MUSL_ARCH_GCC))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@

$(MUSL_GCC_SYSROOT)/sysroot:
	mkdir -p $(MUSL_GCC_SYSROOT) \
		$(MUSL_GCC_SYSROOT)/lib \
		$(MUSL_GCC_SYSROOT)/libexec \
		$(MUSL_GCC_SYSROOT)/include \
		$(MUSL_GCC_SYSROOT)/share \
		$(MUSL_GCC_SYSROOT)/share/cmake-toolchains \
		$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC) \
		$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib \
		$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include
	cd $(MUSL_GCC_SYSROOT); ln -s $(MUSL_ARCH_GCC) $(MUSL_ARCH_CLANG)
	cd $(MUSL_GCC_SYSROOT)/lib; ln -s ../$(MUSL_ARCH_GCC)/lib $(MUSL_ARCH_GCC)
	cd $(MUSL_GCC_SYSROOT)/lib; ln -s ../$(MUSL_ARCH_GCC)/lib $(MUSL_ARCH_CLANG)
	cd $(MUSL_GCC_SYSROOT)/include; ln -s ../$(MUSL_ARCH_GCC)/include $(MUSL_ARCH_GCC)
	cd $(MUSL_GCC_SYSROOT)/include; ln -s ../$(MUSL_ARCH_GCC)/include $(MUSL_ARCH_CLANG)
	touch $(MUSL_GCC_SYSROOT)/sysroot

$(MUSL_GCC_ZLIB): $(ZLIB_VER) $(MUSL_GCC_CC)
	@echo "Build MUSL_GCC_ZLIB: $(MUSL_GCC_ZLIB)"
	mkdir -p zlib-build
	cd zlib-build; CC=$(realpath $(MUSL_GCC_CC)) CFLAGS="-fPIC" ../$(ZLIB_VER)/configure --prefix=$(realpath $(MUSL_GCC_SYSROOT)) --static --const
	cd zlib-build; make; make install
	rm -rf zlib-build

$(MUSL_GCC_ZLIB_ARCH): $(MUSL_GCC_ZLIB)
	cp $(MUSL_GCC_SYSROOT)/include/zlib.h $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/zlib.h
	cp $(MUSL_GCC_SYSROOT)/include/zconf.h $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/zconf.h
	cp $(MUSL_GCC_ZLIB) $(MUSL_GCC_ZLIB_ARCH)

$(MUSL_GCC_LIBCXX): $(MUSL_GCC_CC) $(MUSL_GCC_ZLIB) $(LLVM_VER)
	@echo "Build MUSL_GCC_LIBCXX"
	rm -rf build_libcxx_gcc_runtime
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(MUSL_GCC_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build_libcxx_gcc_runtime \
		-DCXX_SUPPORTS_NOSTDLIBXX_FLAG=Off \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)
	cmake --build build_libcxx_gcc_runtime
	cmake --install build_libcxx_gcc_runtime

$(MUSL_GCC_LIBCRT): $(MUSL_GCC_LIBCXX) $(LLVM_VER)
	@echo "Build MUSL_GCC_LIBCXX"
	rm -rf build_libcxx_gcc_crt
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(MUSL_GCC_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build_libcxx_gcc_crt \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC) \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)
	cmake --build build_libcxx_gcc_crt
	cmake --install build_libcxx_gcc_crt

$(MUSL_GCC_CRTLIB): $(MUSL_GCC_LIBCRT)
	mkdir -p $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib/clang/$(LLVM_MAJOR)/lib/x86_64-unknown-linux-musl
	cd $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib/clang/$(LLVM_MAJOR)/lib/x86_64-unknown-linux-musl; ln -f -s ../../../../linux/libclang_rt.builtins-$(MUSL_ARCH).a libclang_rt.builtins.a
	cp -f $(MUSL_GCC_SYSROOT)/lib/gcc/$(MUSL_ARCH_GCC)/$(MUSL_GCC_V)/*.o $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/lib

$(MUSL_GCC_CLANG): $(MUSL_GCC_LIBCRT) $(LLVM_VER) $(MUSL_GCC_CRTLIB)
	@echo "Build MUSL_GCC_CLANG: $(MUSL_GCC_CLANG)"
	rm -rf build_llvm_stage0
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(MUSL_GCC_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build_llvm_stage0 \
		-DDEFAULT_SYSROOT=$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC) \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLLVM_LOCAL_RPATH='$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/lib;$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH="$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/lib;$$ORIGIN/../lib" \
		-DCMAKE_BUILD_RPATH="$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/lib;$$ORIGIN/../lib" \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)
	cmake --build build_llvm_stage0
	cmake --install build_llvm_stage0

$(MUSL_CLANG_SYSROOT)/sysroot:
	mkdir -p $(MUSL_CLANG_SYSROOT) \
		$(MUSL_CLANG_SYSROOT)/bin \
		$(MUSL_CLANG_SYSROOT)/lib \
		$(MUSL_CLANG_SYSROOT)/libexec \
		$(MUSL_CLANG_SYSROOT)/include \
		$(MUSL_CLANG_SYSROOT)/share \
		$(MUSL_CLANG_SYSROOT)/share/cmake-toolchains \
		$(MUSL_CLANG_SYSROOT)/$(MUSL_ARCH_CLANG) \
		$(MUSL_CLANG_SYSROOT)/$(MUSL_ARCH_CLANG)/lib \
		$(MUSL_CLANG_SYSROOT)/$(MUSL_ARCH_CLANG)/include
	cd $(MUSL_CLANG_SYSROOT)/lib; ln -s ../$(MUSL_ARCH_CLANG)/lib $(MUSL_ARCH_CLANG)
	cd $(MUSL_CLANG_SYSROOT)/include; ln -s ../$(MUSL_ARCH_CLANG)/include $(MUSL_ARCH_CLANG)
	touch $(MUSL_CLANG_SYSROOT)/sysroot

$(MUSL_CLANG_SYSROOT)/include/linux/futex.h: replacements/linux/futex.h
	mkdir -p $(MUSL_CLANG_SYSROOT)/include/linux
	cp $< $@

$(MUSL_CLANG_SYSROOT)/include/asm/prctl.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/prctl.h
	mkdir -p $(MUSL_CLANG_SYSROOT)/include/asm
	cp $< $@

$(MUSL_CLANG_LIBC): $(MUSL_CLANG_SYSROOT)/sysroot $(MUSL_GCC_CLANG)
	rm -rf build_musl
	mkdir build_musl
	cd build_musl; $(realpath $(MUSL_SRC))/configure \
		CC=$(realpath $(MUSL_GCC_SYSROOT))/$(MUSL_ARCH_GCC)/bin/clang \
		CFLAGS="--target=$(MUSL_ARCH_CLANG)" \
		--prefix=$(realpath $(MUSL_CLANG_SYSROOT)) \
		--syslibdir=$(realpath $(MUSL_CLANG_SYSROOT))/lib
	cd build_musl; make -j12; make install;

$(MUSL_CLANG_LIBCXX): $(MUSL_CLANG_LIBC)
	@echo "Build MUSL_GCC_LIBCXX"
	rm -rf build_libcxx_clang_runtime
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(MUSL_GCC_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build_libcxx_clang_runtime \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(MUSL_CLANG_SYSROOT)) \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(MUSL_CLANG_SYSROOT)
	cmake --build build_libcxx_clang_runtime
	cmake --install build_libcxx_clang_runtime

$(MUSL_CLANG_ZLIB): $(ZLIB_VER) $(MUSL_CLANG_CLANG)
	@echo "Build MUSL_CLANG_ZLIB: $(MUSL_CLANG_ZLIB)"
	mkdir -p zlib-build
	cd zlib-build; CC=$(realpath $(MUSL_CLANG_CC)) CFLAGS="-fPIC" ../$(ZLIB_VER)/configure --prefix=$(realpath $(MUSL_CLANG_SYSROOT)) --static --const
	cd zlib-build; make; make install
	rm -rf zlib-build

$(MUSL_CLANG_CLANG): $(MUSL_CLANG_LIBCXX) $(MUSL_GCC_ZLIB_ARCH)
	rm -rf build_llvm_stage1
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(MUSL_GCC_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build_llvm_stage1 \
		-DDEFAULT_SYSROOT=$(realpath $(MUSL_CLANG_SYSROOT)) \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(MUSL_CLANG_SYSROOT)) \
		-DLLVM_LOCAL_RPATH='$(realpath $(MUSL_CLANG_SYSROOT))/lib;$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH="$(realpath $(MUSL_CLANG_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_BUILD_RPATH="$(realpath $(MUSL_CLANG_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(MUSL_CLANG_SYSROOT)
	cmake --build build_llvm_stage1
	cmake --install build_llvm_stage1

$(MUSL_CLANG_CMAKE_CLANG_TOOLCHAIN): $(MUSL_CLANG_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(MUSL_CLANG_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(MUSL_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(MUSL_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(MUSL_CLANG_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(MUSL_CLANG_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@


$(MUSL_FINAL_SYSROOT)/sysroot:
	mkdir -p $(MUSL_FINAL_SYSROOT) \
		$(MUSL_FINAL_SYSROOT)/bin \
		$(MUSL_FINAL_SYSROOT)/lib \
		$(MUSL_FINAL_SYSROOT)/include \
		$(MUSL_FINAL_SYSROOT)/share \
		$(MUSL_FINAL_SYSROOT)/lib/$(MUSL_ARCH_CLANG) \
		$(MUSL_FINAL_SYSROOT)/include/$(MUSL_ARCH_CLANG) \
	touch $(MUSL_FINAL_SYSROOT)/sysroot

$(MUSL_FINAL_SYSROOT)/include/linux/futex.h: replacements/linux/futex.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/prctl.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/prctl.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/byteorder.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/byteorder.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/bitsperlong.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/bitsperlong.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/types.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/types.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/ioctl.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/ioctl.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/posix_types.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/posix_types.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/swab.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/swab.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/unistd.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/unistd.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/unistd_64.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/unistd_64.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm/posix_types_64.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm/posix_types_64.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm-generic/int-ll64.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm-generic/int-ll64.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm-generic
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm-generic/bitsperlong.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm-generic/bitsperlong.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm-generic
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm-generic/types.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm-generic/types.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm-generic
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm-generic/ioctl.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm-generic/ioctl.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/asm-generic/posix_types.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/asm-generic/posix_types.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/asm-generic
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/perf_event.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/perf_event.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/ioctl.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/ioctl.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/types.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/types.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/posix_types.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/posix_types.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/stddef.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/stddef.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/elf.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/elf.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/elf-em.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/elf-em.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/unistd.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/unistd.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/mman.h: replacements/linux/mman.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/swab.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/swab.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@

$(MUSL_FINAL_SYSROOT)/include/linux/version.h: replacements/linux/version.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux
	cp $< $@
	
$(MUSL_FINAL_SYSROOT)/include/linux/byteorder/little_endian.h: $(MUSL_GCC_SYSROOT)/$(MUSL_ARCH_GCC)/include/linux/byteorder/little_endian.h
	mkdir -p $(MUSL_FINAL_SYSROOT)/include/linux/byteorder
	cp $< $@

$(MUSL_FINAL_LIBC): $(MUSL_FINAL_SYSROOT)/sysroot $(MUSL_CLANG_CLANG)
	rm -rf build_musl
	mkdir build_musl
	cd build_musl; $(realpath $(MUSL_SRC))/configure \
		CC=$(realpath $(MUSL_CLANG_SYSROOT))/bin/clang \
		CFLAGS="--target=$(MUSL_ARCH_CLANG)" \
		--prefix=$(realpath $(MUSL_FINAL_SYSROOT)) \
		--syslibdir=$(realpath $(MUSL_FINAL_SYSROOT))/lib
	cd build_musl; make -j12; make install;

MUSL_FINAL_LIBDIR := $(abspath $(MUSL_FINAL_SYSROOT))/lib
MUSL_FINAL_RPATH := $(MUSL_FINAL_LIBDIR);$(MUSL_FINAL_LIBDIR)/$(MUSL_ARCH_CLANG);$$ORIGIN/../lib;$$ORIGIN/../lib/$(MUSL_ARCH_CLANG);

$(MUSL_FINAL_CLANG): $(MUSL_FINAL_LIBC) $(MUSL_FINAL_SYSROOT)/include/linux/futex.h $(MUSL_CLANG_CMAKE_CLANG_TOOLCHAIN)
	rm -rf build_llvm_stage2
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(MUSL_CLANG_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build_llvm_stage2 \
		-DDEFAULT_SYSROOT=$(realpath $(MUSL_FINAL_SYSROOT)) \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(abspath $(MUSL_FINAL_SYSROOT)) \
		-DLLVM_LOCAL_RPATH='$(MUSL_FINAL_RPATH)' \
		-DCMAKE_INSTALL_RPATH="$(MUSL_FINAL_RPATH)" \
		-DCMAKE_BUILD_RPATH="$(MUSL_FINAL_RPATH)" \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(MUSL_FINAL_SYSROOT)
	cmake --build build_llvm_stage2
	cmake --install build_llvm_stage2

$(MUSL_FINAL_ZLIB): $(ZLIB_VER) $(MUSL_FINAL_CLANG)
	@echo "Build MUSL_FINAL_ZLIB: $(MUSL_FINAL_ZLIB)"
	mkdir -p zlib-build
	cd zlib-build; CC=$(realpath $(MUSL_FINAL_CC)) CFLAGS="-fPIC" ../$(ZLIB_VER)/configure --prefix=$(realpath $(MUSL_FINAL_SYSROOT)) --static --const
	cd zlib-build; make; make install
	rm -rf zlib-build

$(MUSL_FINAL_CMAKE_CLANG_TOOLCHAIN): $(MUSL_FINAL_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(MUSL_FINAL_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(MUSL_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(MUSL_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(MUSL_FINAL_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(MUSL_FINAL_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@


# =================
# Static toolchain
# =================

$(STATIC_SYSROOT)/sysroot:
	mkdir -p $(STATIC_SYSROOT) \
		$(STATIC_SYSROOT)/bin \
		$(STATIC_SYSROOT)/lib \
		$(STATIC_SYSROOT)/include \
		$(STATIC_SYSROOT)/share \
		$(STATIC_SYSROOT)/lib/$(MUSL_ARCH_CLANG) \
		$(STATIC_SYSROOT)/include/$(MUSL_ARCH_CLANG) \
	touch $(STATIC_SYSROOT)/sysroot

$(STATIC_LIBC): $(STATIC_SYSROOT)/sysroot $(MUSL_FINAL_CLANG)
	rm -rf build_musl
	mkdir build_musl
	cd build_musl; $(realpath $(MUSL_SRC))/configure \
		CC=$(realpath $(MUSL_FINAL_SYSROOT))/bin/clang \
		CFLAGS="--target=$(MUSL_ARCH_CLANG)" \
		--prefix=$(realpath $(STATIC_SYSROOT)) \
		--syslibdir=$(realpath $(STATIC_SYSROOT))/lib
	cd build_musl; make -j12; make install;

$(STATIC_CLANG): $(STATIC_LIBC)
	rm -rf build_llvm_static
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(MUSL_FINAL_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build_llvm_static \
		-DCMAKE_BUILD_TYPE=Release \
		-DDEFAULT_SYSROOT=.. \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INCLUDE_TESTS=Off \
		-DLLVM_BUILD_STATIC=On \
		-DLLVM_TABLEGEN=$(realpath $(MUSL_FINAL_SYSROOT))/bin/llvm-tblgen \
		-DCLANG_TABLEGEN=$(realpath $(MUSL_FINAL_SYSROOT))/bin/clang-tblgen \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DCLANG_INCLUDE_TESTS=Off \
		-DCLANG_LINK_CLANG_DYLIB=Off \
		-DCLANG_ENABLE_ARCMT=Off \
		-DCLANG_ENABLE_STATIC_ANALYZER=Off \
		-DLIBCLANG_BUILD_STATIC=On \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(abspath $(STATIC_SYSROOT)) \
		-DLLVM_LOCAL_RPATH='$ORIGIN/../lib;$ORIGIN/../lib/$(MUSL_ARCH_CLANG)' \
		-DCMAKE_INSTALL_RPATH='$ORIGIN/../lib;$ORIGIN/../lib/$(MUSL_ARCH_CLANG)' \
		-DCMAKE_BUILD_RPATH='$ORIGIN/../lib;$ORIGIN/../lib/$(MUSL_ARCH_CLANG)' \
		-DLLVM_HOST_TRIPLE=$(MUSL_ARCH_CLANG) \
		-DCMAKE_EXE_LINKER_FLAGS="-static -lc++ -lc++abi" \
		-DCMAKE_INSTALL_PREFIX=$(STATIC_SYSROOT)
	cmake --build build_llvm_static
	cmake --install build_llvm_static

$(STATIC_ZLIB): $(ZLIB_VER) $(STATIC_CLANG)
	@echo "Build STATIC_ZLIB: $(STATIC_ZLIB)"
	mkdir -p zlib-build
	cd zlib-build; CC=$(realpath $(STATIC_CC)) CFLAGS="-fPIC" ../$(ZLIB_VER)/configure --prefix=$(realpath $(STATIC_SYSROOT)) --static --const
	cd zlib-build; make; make install
	rm -rf zlib-build

$(STATIC_CMAKE_CLANG_TOOLCHAIN): $(STATIC_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STATIC_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(MUSL_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(MUSL_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(STATIC_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(STATIC_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@

$(STATIC_SYSROOT_MIN)/sysroot: $(STATIC_ZLIB) $(STATIC_CLANG)
	rm -rf $(STATIC_SYSROOT_MIN)
	cp -r -f $(STATIC_SYSROOT) $(STATIC_SYSROOT_MIN)
	cd $(STATIC_SYSROOT_MIN)/bin; rm -f \
		amdgpu-arch clang-format clang-installapi clang-linker-wrapper clang-nvlink-wrapper \
		clang-offload-bundler clang-offload-packager clang-refactor clang-repl clang-scan-deps \
		clang-sycl-linker  diagtool git-clang-format hmaptool llvm-cov llvm-cxxfilt llvm-dwp \
		llvm-mca llvm-ml llvm-nm llvm-objdump llvm-pdbutil llvm-profdata llvm-profgen \
		llvm-readobj llvm-size llvm-strings llvm-symbolizer nvptx-arch wasm-ld
	cd $(STATIC_SYSROOT_MIN)/lib; rm -f \
		libclang.so libclang.so.* libLTO.so libLTO.so.* libRemarks.so libRemarks.so.*
	rm -rf $(STATIC_SYSROOT_MIN)/share/clang
	mv $(STATIC_SYSROOT_MIN)/lib/$(MUSL_ARCH_CLANG)/* $(STATIC_SYSROOT_MIN)/lib
	rm -rf $(STATIC_SYSROOT_MIN)/lib/$(MUSL_ARCH_CLANG)

$(STATIC_SYSROOT_MIN)/include/linux/mman.h: replacements/linux/mman.h
	mkdir -p $(STATIC_SYSROOT_MIN)/include/linux
	cp $< $@

$(STATIC_SYSROOT_MIN)/include/linux/version.h: replacements/linux/version.h
	mkdir -p $(STATIC_SYSROOT_MIN)/include/linux
	cp $< $@

$(STATIC_SYSROOT_MIN)/include/linux/if_alg.h: replacements/linux/if_alg.h
	mkdir -p $(STATIC_SYSROOT_MIN)/include/linux
	cp $< $@

$(STATIC_SYSROOT_MIN)/include/linux/aio_abi.h: replacements/linux/aio_abi.h
	mkdir -p $(STATIC_SYSROOT_MIN)/include/linux
	cp $< $@

$(STATIC_SYSROOT_MIN)/include/bits/fdhelper.h: fdhelper/$(MUSL_ARCH)/fdhelper.h
	mkdir -p $(STATIC_SYSROOT_MIN)/include/bits
	cp $< $@

gcc-toolchain-files: $(MUSL_GCC_CMAKE_GCC_TOOLCHAIN) $(MUSL_GCC_CMAKE_CLANG_TOOLCHAIN)

$(MUSL_GCC_SYSROOT).tar.xz: $(MUSL_GCC_SYSROOT) gcc-toolchain-files $(MUSL_GCC_CC)
	tar -cJf $@ $(MUSL_GCC_SYSROOT)

stage0: gcc-toolchain-files $(MUSL_GCC_CC) $(MUSL_GCC_LIBCXX) $(MUSL_GCC_LIBCRT) $(MUSL_GCC_CLANG)

stage1-files: \
	$(MUSL_CLANG_SYSROOT)/include/linux/futex.h \
	$(MUSL_CLANG_SYSROOT)/include/asm/prctl.h

stage1: $(MUSL_CLANG_SYSROOT)/sysroot $(MUSL_CLANG_LIBC) $(MUSL_CLANG_LIBCXX) $(MUSL_CLANG_CLANG) $(MUSL_CLANG_ZLIB) \
	stage1-files

stage2-files: \
	$(MUSL_FINAL_SYSROOT)/include/linux/futex.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/prctl.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/byteorder.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/bitsperlong.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/types.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/ioctl.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/posix_types.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/posix_types_64.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/swab.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/unistd.h \
	$(MUSL_FINAL_SYSROOT)/include/asm/unistd_64.h \
	$(MUSL_FINAL_SYSROOT)/include/asm-generic/types.h \
	$(MUSL_FINAL_SYSROOT)/include/asm-generic/ioctl.h \
	$(MUSL_FINAL_SYSROOT)/include/asm-generic/int-ll64.h \
	$(MUSL_FINAL_SYSROOT)/include/asm-generic/bitsperlong.h \
	$(MUSL_FINAL_SYSROOT)/include/asm-generic/posix_types.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/perf_event.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/types.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/posix_types.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/stddef.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/ioctl.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/elf.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/elf-em.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/unistd.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/mman.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/swab.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/version.h \
	$(MUSL_FINAL_SYSROOT)/include/linux/byteorder/little_endian.h

stage2: $(MUSL_FINAL_SYSROOT)/sysroot $(MUSL_FINAL_LIBC) $(MUSL_FINAL_CLANG) stage2-files \
	$(MUSL_FINAL_CMAKE_CLANG_TOOLCHAIN) $(MUSL_FINAL_ZLIB)

static-toolchain: stage2-files $(STATIC_LIBC) $(STATIC_CLANG) $(STATIC_SYSROOT_MIN)/sysroot

output-toolchain: static-toolchain \
		$(STATIC_SYSROOT_MIN)/include/linux/mman.h \
		$(STATIC_SYSROOT_MIN)/include/linux/version.h \
		$(STATIC_SYSROOT_MIN)/include/linux/if_alg.h \
		$(STATIC_SYSROOT_MIN)/include/linux/aio_abi.h \
		$(STATIC_SYSROOT_MIN)/include/bits/fdhelper.h
	cp -f -r $(STATIC_SYSROOT_MIN) $(OUTPUT)
	cd $(OUTPUT)/bin; ln -f -s clang cc
	cd $(OUTPUT)/bin; ln -f -s clang++ c++
	cd $(OUTPUT)/bin; ln -f -s clang++ cxx
	cd $(OUTPUT)/bin; ln -f -s clang-cpp cpp
	cd $(OUTPUT)/bin; ln -f -s llvm-ar ar
	cd $(OUTPUT)/bin; ln -f -s llvm-ranlib ranlib
	cd $(OUTPUT)/include/linux; ln -s ../limits.h limits.h
	touch $(OUTPUT)/include/linux/types.h
	cp -rf ../licenses $(OUTPUT)/share/licenses

output-toolchain-file: output-toolchain
	@echo 'TOOLCHAIN_SYSROOT := $$(abspath $$(dir $$(lastword $$(MAKEFILE_LIST))))' > $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_TARGET := $(MUSL_ARCH_CLANG)' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_LIBDIR := $$(TOOLCHAIN_SYSROOT)/lib' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_INCLUDEDIR := $$(TOOLCHAIN_SYSROOT)/include' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_CC := $$(TOOLCHAIN_SYSROOT)/bin/clang' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_CXX := $$(TOOLCHAIN_SYSROOT)/bin/clang++' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_AR := $$(TOOLCHAIN_SYSROOT)/bin/llvm-ar' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_GLSLANG := $$(TOOLCHAIN_SYSROOT)/bin/glslang' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_SPIRV_LINK := $$(TOOLCHAIN_SYSROOT)/bin/spirv-link' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_STATIC := 1' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_REQUIRES_STAPPLER_ABI := 1' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_GENERAL_CXXFLAGS := -stdlib=libc++' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_LIB_LDFLAGS :=' >> $(OUTPUT)/toolchain.mk
	@echo 'TOOLCHAIN_EXEC_LDFLAGS := -static -lc -lc++ -lc++abi' >> $(OUTPUT)/toolchain.mk
	@echo '$$(info Available toolchain: $$(TOOLCHAIN_TARGET): $$(TOOLCHAIN_SYSROOT))' >> $(OUTPUT)/toolchain.mk

output: output-toolchain output-toolchain-file

download: $(MUSL_CROSS_MAKE) $(LLVM_VER)

all: output output-toolchain-file

archive: $(MUSL_GCC_SYSROOT).tar.xz

.PHONY: stage0 all gcc-toolchain-files archive musl-gcc-crt-link stage1-files stage1 stage2-files stage2 static-toolchain output download
