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

# Здесь мы собираем glibc, GCC, libstdc++, затем, с их помощью - clang
# Этот clang будет слинкован с libstdc++ и рантаймом от GCC, но сможет собрать
# отвязанный от GCC clang на stage1

STAGE0_SYSROOT := sysroot-gcc
STAGE0_GCC_CC := $(STAGE0_SYSROOT)/bin/gcc
STAGE0_GCC_CXX := $(STAGE0_SYSROOT)/bin/g++
STAGE0_GCC_MAKE := $(STAGE0_SYSROOT)/bin/make
STAGE0_GCC_GLIBC := $(STAGE0_SYSROOT)/lib/libc.so.6
STAGE0_ZLIB := $(STAGE0_SYSROOT)/lib/libz.a
STAGE0_LIBCXX := $(STAGE0_SYSROOT)/lib/libc++.a
STAGE0_LIBCRT := $(STAGE0_SYSROOT)/lib/linux/liborc_rt-$(SP_ARCH).a
STAGE0_CLANG_CC := $(STAGE0_SYSROOT)/bin/clang
STAGE0_CLANG_CXX := $(STAGE0_SYSROOT)/bin/clang++
STAGE0_CLANG_RTLIB := $(STAGE0_SYSROOT)/lib/clang/21/lib/$(SP_ARCH_CLANG)/libclang_rt.builtins.a

STAGE0_CMAKE_GCC_TOOLCHAIN := $(STAGE0_SYSROOT)/gcc.cmake
STAGE0_CMAKE_CLANG_TOOLCHAIN := $(STAGE0_SYSROOT)/clang.cmake

# Отдельно отметим синдром Туретта у GCC, под который необходимо подстроится, дублировав выходной путь с помощью
#
# mkdir -p $(STAGE0_SYSROOT)$(abspath $(STAGE0_SYSROOT))
# cd $(STAGE0_SYSROOT)$(abspath $(STAGE0_SYSROOT)); ln -s $(abspath $(STAGE0_SYSROOT))/lib lib
#
# Мы подставляем ссылку на реальное положение библиотек в локацию, которую ожидает GCC

$(STAGE0_SYSROOT)/sysroot: $(LINUX_HEADERS_DIR)
	mkdir -p $(STAGE0_SYSROOT)/etc
	mkdir -p $(STAGE0_SYSROOT)/usr
	mkdir -p $(STAGE0_SYSROOT)/lib
	mkdir -p $(STAGE0_SYSROOT)$(abspath $(STAGE0_SYSROOT))
	cp -rL $(LINUX_HEADERS_DIR)/include $(STAGE0_SYSROOT)/include
	cd $(STAGE0_SYSROOT); ln -s lib lib64
	cd $(STAGE0_SYSROOT)/usr; ln -s ../include include
	cd $(STAGE0_SYSROOT)/usr; ln -s ../lib lib
	cd $(STAGE0_SYSROOT)$(abspath $(STAGE0_SYSROOT)); ln -s $(abspath $(STAGE0_SYSROOT))/lib lib
	touch $(STAGE0_SYSROOT)/sysroot

$(STAGE0_GCC_MAKE): $(STAGE0_SYSROOT)/sysroot $(MAKE_SRC_DIR)
	rm -rf build/stage0-make
	mkdir -p build/stage0-make
	cd build/stage0-make; $(abspath $(MAKE_SRC_DIR))/configure --prefix $(abspath $(STAGE0_SYSROOT))
	make -C build/stage0-make $(SP_NJOBS)
	make -C build/stage0-make install

$(STAGE0_GCC_GLIBC): $(STAGE0_GCC_MAKE) $(GLIBC_MIN_SRC_DIR)
	rm -rf build/stage0-glibc
	mkdir -p build/stage0-glibc
	cd build/stage0-glibc; $(abspath $(GLIBC_MIN_SRC_DIR))/configure \
		--prefix $(abspath $(STAGE0_SYSROOT)) \
		--with-headers=$(abspath $(STAGE0_SYSROOT))/include \
		--disable-werror --without-selinux \
		--disable-timezone-tools \
		CXX=
	$(STAGE0_GCC_MAKE) -C build/stage0-glibc $(SP_NJOBS)
	$(STAGE0_GCC_MAKE) -C build/stage0-glibc install
	cp -rf replacements/$(GLIBC_MIN_SRC_DIR)/include/* $(STAGE0_SYSROOT)/include

STAGE0_GCC_CC_CFLAGS := \
	-I$(abspath $(GCC_SRC_DIR))/include \
	-I$(abspath $(STAGE0_SYSROOT))/include

$(STAGE0_GCC_CC): $(STAGE0_GCC_GLIBC)
	rm -rf build/stage0-gcc
	mkdir -p build/stage0-gcc
	cd build/stage0-gcc; $(abspath $(GCC_SRC_DIR))/configure \
		--prefix $(abspath $(STAGE0_SYSROOT)) \
		--libdir $(abspath $(STAGE0_SYSROOT))/lib \
		--disable-multilib \
		--disable-libsanitizer \
		--enable-languages=c,c++ \
		--with-sysroot=$(abspath $(STAGE0_SYSROOT)) \
		CFLAGS="$(STAGE0_GCC_CC_CFLAGS)" \
		CXXFLAGS="$(STAGE0_GCC_CC_CFLAGS)" \
		LDFLAGS=""
	make -C build/stage0-gcc $(SP_NJOBS)
	make -C build/stage0-gcc install

$(STAGE0_ZLIB): $(ZLIB_DIR) $(STAGE0_GCC_CC)
	@echo "Build STAGE0_ZLIB: $(STAGE0_ZLIB)"
	rm -rf build/stage0-zlib
	mkdir -p build/stage0-zlib
	cd build/stage0-zlib; CC=$(realpath $(STAGE0_GCC_CC)) CFLAGS="-fPIC --sysroot $(realpath $(STAGE0_SYSROOT))" \
		$(abspath $(ZLIB_VER))/configure --prefix=$(realpath $(STAGE0_SYSROOT)) --static --const
	cd build/stage0-zlib; make; make install
	
$(STAGE0_CMAKE_GCC_TOOLCHAIN): $(STAGE0_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE0_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(STAGE0_SYSROOT))/bin/gcc)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(STAGE0_SYSROOT))/bin/g++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT -Wl,--dynamic-linker=$(realpath $(STAGE0_SYSROOT))/lib/ld-linux-$(SP_ARCH_LD).so.2)" >> $@

$(STAGE0_CMAKE_CLANG_TOOLCHAIN): $(STAGE0_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE0_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(STAGE0_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(STAGE0_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT -Wl,--dynamic-linker=$(realpath $(STAGE0_SYSROOT))/lib/ld-linux-$(SP_ARCH_LD).so.2)" >> $@

$(STAGE0_LIBCXX): $(LLVM_VER) $(STAGE0_CMAKE_GCC_TOOLCHAIN) $(STAGE0_ZLIB) $(STAGE0_GCC_CC)
	@echo "Build STAGE0_LIBCXX $(STAGE0_LIBCXX)"
	rm -rf build/libcxx_gcc_runtime
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build/libcxx_gcc_runtime \
		-DCXX_SUPPORTS_NOSTDLIBXX_FLAG=Off \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DCMAKE_INSTALL_PREFIX=$(realpath $(STAGE0_SYSROOT))
	cmake --build build/libcxx_gcc_runtime
	cmake --install build/libcxx_gcc_runtime

$(STAGE0_LIBCRT): $(LLVM_VER) $(STAGE0_LIBCXX)
	@echo "Build STAGE0_LIBCRT $(STAGE0_LIBCRT)"
	rm -rf build/libcxx_gcc_crt
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build/libcxx_gcc_crt \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(STAGE0_SYSROOT)) \
		-DCMAKE_INSTALL_PREFIX=$(realpath $(STAGE0_SYSROOT))
	cmake --build build/libcxx_gcc_crt
	cmake --install build/libcxx_gcc_crt

$(STAGE0_CLANG_CC): $(LLVM_VER) $(STAGE0_LIBCRT)
	@echo "Build STAGE0_CLANG_CC $(STAGE0_CLANG_CC)"
	rm -rf build_llvm_stage0
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build/llvm_stage0 \
		-DDEFAULT_SYSROOT=$(realpath $(STAGE0_SYSROOT)) \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLLVM_LOCAL_RPATH='$(realpath $(STAGE0_SYSROOT))/lib;$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH="$(realpath $(STAGE0_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_BUILD_RPATH="$(realpath $(STAGE0_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_INSTALL_PREFIX=$(STAGE0_SYSROOT)
	cmake --build build/llvm_stage0
	cmake --install build/llvm_stage0

$(STAGE0_CLANG_RTLIB): $(STAGE0_LIBCRT)
	mkdir -p $(dir $(STAGE0_CLANG_RTLIB))
	cp $(STAGE0_SYSROOT)/lib/linux/clang_rt.crtbegin-$(SP_ARCH).o $(dir $(STAGE0_CLANG_RTLIB))/clang_rt.crtbegin.o
	cp $(STAGE0_SYSROOT)/lib/linux/clang_rt.crtend-$(SP_ARCH).o $(dir $(STAGE0_CLANG_RTLIB))/clang_rt.crtend.o
	cp $(STAGE0_SYSROOT)/lib/linux/libclang_rt.profile-$(SP_ARCH).a $(dir $(STAGE0_CLANG_RTLIB))/libclang_rt.profile.a
	cp $(STAGE0_SYSROOT)/lib/linux/liborc_rt-$(SP_ARCH).a $(dir $(STAGE0_CLANG_RTLIB))/liborc_rt.a
	cp $(STAGE0_SYSROOT)/lib/linux/libclang_rt.builtins-$(SP_ARCH).a $(dir $(STAGE0_CLANG_RTLIB))/libclang_rt.builtins.a

stage0: $(STAGE0_GCC_GLIBC) $(STAGE0_GCC_CC) $(STAGE0_CLANG_CC) $(STAGE0_CMAKE_CLANG_TOOLCHAIN) $(STAGE0_CLANG_RTLIB)
