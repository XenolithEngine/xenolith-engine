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

# Здесь мы собираем минимальный glibc и финальную версию clang для использования при распространении

# Нам нужен флаг -D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE чтобы libc++ корректно работало со Stappler Runtime

STAGE2_SYSROOT := sysroot-clang2
STAGEOUT_SYSROOT := sysroot-clang-out
STAGE2_MAKE := $(STAGE1_SYSROOT)/bin/make
STAGE2_GLIBC := $(STAGE2_SYSROOT)/lib/libc.so.6
STAGE2_ZLIB := $(STAGE2_SYSROOT)/lib/libz.a
STAGE2_LIBCXX := $(STAGE2_SYSROOT)/lib/libc++.a

STAGE2_CLANG_CC := $(STAGEOUT_SYSROOT)/bin/clang
STAGE2_CLANG_CXX := $(STAGEOUT_SYSROOT)/bin/clang++
STAGEOUT_GLIBC := $(STAGEOUT_SYSROOT)/lib/libc.so.6
STAGEOUT_ZLIB := $(STAGEOUT_SYSROOT)/lib/libz.a

STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN := $(STAGE2_SYSROOT)/clang.stage1.cmake

$(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN): $(STAGE2_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE2_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(realpath $(STAGE1_SYSROOT))/bin/clang)" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(realpath $(STAGE1_SYSROOT))/bin/clang++)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@

$(STAGE2_SYSROOT)/sysroot: $(LINUX_HEADERS_DIR)
	mkdir -p $(STAGE2_SYSROOT)/etc
	mkdir -p $(STAGE2_SYSROOT)/lib
	mkdir -p $(STAGE2_SYSROOT)/include
	cp -rL $(LINUX_HEADERS_DIR)/include/* $(STAGE2_SYSROOT)/include
	touch $(STAGE2_SYSROOT)/sysroot

$(STAGE2_GLIBC): $(STAGE2_SYSROOT)/sysroot $(GLIBC_SRC_DIR) $(STAGE1_MAKE)
	rm -rf build/stage2-glibc
	mkdir -p build/stage2-glibc
	cd build/stage2-glibc; $(abspath $(GLIBC_SRC_DIR))/configure \
		--prefix $(abspath $(STAGE2_SYSROOT)) \
		--disable-werror \
		--without-selinux \
		--disable-profile \
		--disable-timezone-tools \
		--enable-tunables=no \
		--disable-crypt \
		--disable-nscd \
		--disable-build-nscd \
		--disable-nss-crypt \
		--with-headers=$(abspath $(STAGE2_SYSROOT))/include \
		 CC=$(abspath $(STAGE0_GCC_CC)) \
		 CXX=$(abspath $(STAGE0_GCC_CXX)) \
		 CFLAGS="-Os"
	$(STAGE1_MAKE) -C build/stage2-glibc $(SP_NJOBS)
	$(STAGE1_MAKE) -C build/stage2-glibc install
	sed  -i 's#$(abspath $(STAGE2_SYSROOT))/lib/##g' $(STAGE2_SYSROOT)/lib/libc.so
	sed  -i 's#$(abspath $(STAGE2_SYSROOT))/lib/##g' $(STAGE2_SYSROOT)/lib/libm.a
	sed  -i 's#$(abspath $(STAGE2_SYSROOT))/lib/##g' $(STAGE2_SYSROOT)/lib/libm.so

$(STAGE2_ZLIB): $(ZLIB_DIR) $(STAGE1_CLANG_CC)
	@echo "Build STAGE2_ZLIB: $(STAGE2_ZLIB)"
	rm -rf build/stage2-zlib
	mkdir -p build/stage2-zlib
	cd build/stage2-zlib; CC=$(realpath $(STAGE1_CLANG_CC)) \
		CFLAGS="-fPIC --sysroot $(realpath $(STAGE1_SYSROOT))" \
		$(abspath $(ZLIB_DIR))/configure --prefix=$(realpath $(STAGE2_SYSROOT)) --static --const
	cd build/stage2-zlib; make; make install

$(STAGE2_LIBCXX): $(STAGE2_ZLIB) $(STAGE1_CLANG_CC) $(STAGE1_CMAKE_CLANG_TOOLCHAIN)
	@echo "Build STAGE2_LIBCXX: $(STAGE2_LIBCXX)"
	rm -rf build/stage2-libcxx
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE1_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/runtimes -B build/stage2-libcxx \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PIC=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_SHARED=Off \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=Off \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(STAGE2_SYSROOT)) \
		-DCMAKE_INSTALL_PREFIX=$(STAGE2_SYSROOT) \
		-DBUILD_SHARED_LIBS=OFF \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE \
		-DCMAKE_C_FLAGS_INIT="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DCMAKE_CXX_FLAGS_INIT="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DCMAKE_C_FLAGS="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DCMAKE_CXX_FLAGS="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE"
	cmake --build build/stage2-libcxx
	cmake --install build/stage2-libcxx


#	mkdir -p $(STAGE1_SYSROOT)$(abspath $(STAGE1_SYSROOT))
#	cd $(STAGE1_SYSROOT)$(abspath $(STAGE1_SYSROOT)); ln -f -s $(abspath $(STAGE1_SYSROOT))/lib lib

$(STAGE2_CLANG_CC): $(STAGE1_LIBCXX) $(STAGE1_ZLIB) $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)
	rm -rf build/stage2-llvm
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_VER)/llvm -B build/stage2-llvm \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PIC=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DCLANG_INCLUDE_TESTS=Off \
		-DCLANG_LINK_CLANG_DYLIB=Off \
		-DCLANG_ENABLE_ARCMT=Off \
		-DCLANG_ENABLE_STATIC_ANALYZER=Off \
		-DLIBCLANG_BUILD_STATIC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXX_ENABLE_SHARED=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_SHARED=Off \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=Off \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
        -DCOMPILER_RT_DEFAULT_TARGET_ARCH="$(SP_ARCH)" \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(abspath $(STAGEOUT_SYSROOT)) \
		-DLLVM_LOCAL_RPATH='$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH="$$ORIGIN/../lib" \
		-DCMAKE_BUILD_RPATH="$$ORIGIN/../lib" \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT)) \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi" \
		-DBUILD_SHARED_LIBS=OFF \
		-DCMAKE_C_FLAGS_INIT="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DCMAKE_CXX_FLAGS_INIT="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DCMAKE_C_FLAGS="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DCMAKE_CXX_FLAGS="-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" \
		-DDEFAULT_SYSROOT=..
	cmake --build build/stage2-llvm
	cmake --install build/stage2-llvm

$(STAGEOUT_GLIBC): $(STAGE2_SYSROOT)/sysroot $(GLIBC_SRC_DIR) $(STAGE1_MAKE)
	mkdir -p $(STAGEOUT_SYSROOT)/lib
	mkdir -p $(STAGEOUT_SYSROOT)/include_libc
	cp -frL $(LINUX_HEADERS_DIR)/include/* $(STAGEOUT_SYSROOT)/include_libc
	rm -rf build/stageout-glibc
	mkdir -p build/stageout-glibc
	cd build/stageout-glibc; $(abspath $(GLIBC_SRC_DIR))/configure \
		--prefix $(abspath $(STAGEOUT_SYSROOT)) \
		--includedir=$(abspath $(STAGEOUT_SYSROOT))/include_libc \
		--disable-werror \
		--without-selinux \
		--disable-profile \
		--disable-timezone-tools \
		--enable-tunables=no \
		--disable-crypt \
		--disable-nscd \
		--disable-build-nscd \
		--disable-nss-crypt \
		--with-headers=$(abspath $(STAGEOUT_SYSROOT))/include_libc \
		 CC=$(abspath $(STAGE0_GCC_CC)) \
		 CXX=$(abspath $(STAGE0_GCC_CXX)) \
		 CFLAGS="-Os"
	$(STAGE1_MAKE) -C build/stageout-glibc $(SP_NJOBS)
	$(STAGE1_MAKE) -C build/stageout-glibc install
	sed  -i 's#$(abspath $(STAGEOUT_SYSROOT))/lib/##g' $(STAGEOUT_SYSROOT)/lib/libc.so
	sed  -i 's#$(abspath $(STAGEOUT_SYSROOT))/lib/##g' $(STAGEOUT_SYSROOT)/lib/libm.a
	sed  -i 's#$(abspath $(STAGEOUT_SYSROOT))/lib/##g' $(STAGEOUT_SYSROOT)/lib/libm.so


stage2: $(STAGE2_GLIBC) $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN) $(STAGE2_ZLIB) \
	$(STAGE2_LIBCXX) $(STAGE2_CLANG_CC) $(STAGEOUT_GLIBC)
