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

# Здесь мы собираем glibc и независимый clang, который будет собирать зависимости и конечный clang для тулчейна

STAGE1_SYSROOT := sysroot-clang1
STAGE1_MAKE := $(STAGE1_SYSROOT)/bin/make
STAGE1_GLIBC := $(STAGE1_SYSROOT)/lib/libc.so.6
STAGE1_ZLIB := $(STAGE1_SYSROOT)/lib/libz.a
STAGE1_LIBCXX := $(STAGE1_SYSROOT)/lib/libc++.a
STAGE1_LIBCRT := $(STAGE1_SYSROOT)/lib/linux/liborc_rt-$(SP_ARCH).a
STAGE1_CLANG_CC := $(STAGE1_SYSROOT)/bin/clang
STAGE1_CLANG_CXX := $(STAGE1_SYSROOT)/bin/clang++

STAGE1_CMAKE_STAGE0_CLANG_TOOLCHAIN := $(STAGE1_SYSROOT)/clang.stage0.cmake
STAGE1_CMAKE_CLANG_TOOLCHAIN := $(STAGE1_SYSROOT)/clang.cmake

$(STAGE1_CMAKE_STAGE0_CLANG_TOOLCHAIN): $(STAGE1_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE1_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(STAGE0_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(STAGE0_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT -Wl,--dynamic-linker=$(realpath $(STAGE1_SYSROOT))/lib/ld-linux-$(SP_ARCH_LD).so.2)" >> $@

$(STAGE1_CMAKE_CLANG_TOOLCHAIN): $(STAGE1_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE1_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(STAGE1_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(STAGE1_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@

$(STAGE1_SYSROOT)/sysroot: $(LINUX_HEADERS_DIR)
	mkdir -p $(STAGE1_SYSROOT)/etc
	mkdir -p $(STAGE1_SYSROOT)/lib
	cp -rL $(LINUX_HEADERS_DIR)/include $(STAGE1_SYSROOT)/include
	touch $(STAGE1_SYSROOT)/sysroot

$(STAGE1_MAKE): $(STAGE1_SYSROOT)/sysroot $(STAGE0_CLANG_CC) $(MAKE_SRC_DIR)
	rm -rf build/stage1-make
	mkdir -p build/stage1-make
	cd build/stage1-make; $(abspath $(MAKE_SRC_DIR))/configure --prefix $(abspath $(STAGE1_SYSROOT)) CC=$(abspath $(STAGE0_CLANG_CC))
	make -C build/stage1-make $(SP_NJOBS)
	make -C build/stage1-make install

$(STAGE1_GLIBC): $(GLIBC_SRC_DIR) $(STAGE1_MAKE)
	rm -rf build/stage1-glibc
	mkdir -p build/stage1-glibc
	cd build/stage1-glibc; $(abspath $(GLIBC_SRC_DIR))/configure \
		--prefix $(abspath $(STAGE1_SYSROOT)) \
		--disable-werror --without-selinux \
		--with-headers=$(abspath $(STAGE1_SYSROOT))/include \
		 CC=$(abspath $(STAGE0_GCC_CC)) \
		 CXX=$(abspath $(STAGE0_GCC_CXX))
	$(STAGE1_MAKE) -C build/stage1-glibc $(SP_NJOBS)
	$(STAGE1_MAKE) -C build/stage1-glibc install

$(STAGE1_ZLIB): $(ZLIB_DIR) $(STAGE0_CLANG_CC)
	@echo "Build STAGE0_ZLIB: $(STAGE0_ZLIB)"
	rm -rf build/stage1-zlib
	mkdir -p build/stage1-zlib
	cd build/stage1-zlib; CC=$(realpath $(STAGE0_CLANG_CC)) CFLAGS="-fPIC --sysroot $(realpath $(STAGE0_SYSROOT))" \
		$(abspath $(ZLIB_DIR))/configure --prefix=$(realpath $(STAGE1_SYSROOT)) --static --const
	cd build/stage1-zlib; make; make install

$(STAGE1_LIBCXX): $(STAGE1_ZLIB) $(STAGE0_CLANG_CC) $(STAGE1_CMAKE_STAGE0_CLANG_TOOLCHAIN)
	@echo "Build MUSL_GCC_LIBCXX"
	rm -rf build/stage1-libcxx
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build/stage1-libcxx \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCMAKE_BUILD_TYPE=Release \
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
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(STAGE1_SYSROOT)) \
		-DCMAKE_INSTALL_PREFIX=$(STAGE1_SYSROOT)
	cmake --build build/stage1-libcxx
	cmake --install build/stage1-libcxx

$(STAGE1_CLANG_CC): $(STAGE1_LIBCXX) $(STAGE1_ZLIB) $(STAGE1_CMAKE_STAGE0_CLANG_TOOLCHAIN)
	mkdir -p $(STAGE1_SYSROOT)$(abspath $(STAGE1_SYSROOT))
	cd $(STAGE1_SYSROOT)$(abspath $(STAGE1_SYSROOT)); ln -f -s $(abspath $(STAGE1_SYSROOT))/lib lib
	rm -rf build/stage1-llvm
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE1_CMAKE_STAGE0_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build/stage1-llvm \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
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
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(STAGE1_SYSROOT)) \
		-DLLVM_LOCAL_RPATH='$(realpath $(STAGE1_SYSROOT))/lib;$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH="$(realpath $(STAGE1_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_BUILD_RPATH="$(realpath $(STAGE1_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_INSTALL_PREFIX=$(STAGE1_SYSROOT)
	cmake --build build/stage1-llvm
	cmake --install build/stage1-llvm

stage1: $(STAGE1_MAKE) $(STAGE1_GLIBC) $(STAGE1_ZLIB) $(STAGE1_LIBCXX) \
	$(STAGE1_CMAKE_STAGE0_CLANG_TOOLCHAIN) $(STAGE1_CLANG_CC)
