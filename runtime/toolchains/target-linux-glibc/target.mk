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

TARGET_SYSROOT := sysroot-target-$(SP_ARCH_TARGET_CLANG)
TARGET_AS := $(TARGET_SYSROOT)/bin/$(SP_ARCH_TARGET_CLANG)-as
TARGET_CC := $(TARGET_SYSROOT)/bin/$(SP_ARCH_TARGET_CLANG)-gcc
TARGET_CXX := $(TARGET_SYSROOT)/bin/$(SP_ARCH_TARGET_CLANG)-g++
TARGET_GLIBC := $(TARGET_SYSROOT)/lib/libc.so.6

TARGET_LIBCXX := $(TARGET_SYSROOT)/lib/libc++.a
TARGET_LIBCRT := $(TARGET_SYSROOT)/lib/linux/liborc_rt-$(SP_ARCH_TARGET).a

TARGET_CMAKE_GCC_TOOLCHAIN := $(TARGET_SYSROOT)/gcc.cmake

$(TARGET_SYSROOT)/sysroot: $(LINUX_TARGET_HEADERS_DIR)
	mkdir -p $(TARGET_SYSROOT)/etc
	mkdir -p $(TARGET_SYSROOT)/usr
	mkdir -p $(TARGET_SYSROOT)/lib
	cp -rL $(LINUX_TARGET_HEADERS_DIR)/include $(TARGET_SYSROOT)/include
	cd $(TARGET_SYSROOT); ln -s lib lib64
	cd $(TARGET_SYSROOT)/usr; ln -s ../include include
	cd $(TARGET_SYSROOT)/usr; ln -s ../lib lib
	touch $(TARGET_SYSROOT)/sysroot

$(TARGET_AS): $(HOST_GCC_CC) $(BINUTILS_SRC_DIR) $(TARGET_SYSROOT)/sysroot
	rm -rf build/target-binutils
	mkdir -p build/target-binutils
	cd build/target-binutils; $(abspath $(BINUTILS_SRC_DIR))/configure \
		--target=$(SP_ARCH_TARGET_CLANG) --prefix $(abspath $(TARGET_SYSROOT)) --disable-nls --disable-werror \
		 CC=$(abspath $(HOST_GCC_CC)) \
		 CXX=$(abspath $(HOST_GCC_CXX))
	make -C build/target-binutils $(SP_NJOBS)
	make -C build/target-binutils install

$(TARGET_GLIBC): $(TARGET_AS) $(HOST_GCC_CC) $(TARGET_SYSROOT)/sysroot
	rm -rf build/target-gcc
	mkdir -p build/target-gcc
	cd build/target-gcc; PATH=$(abspath $(TARGET_SYSROOT)/bin):$$PATH $(abspath $(GCC_SRC_DIR))/configure \
		--prefix $(abspath $(TARGET_SYSROOT)) \
		--libdir $(abspath $(TARGET_SYSROOT))/lib \
		--disable-nls \
		--disable-multilib \
		--disable-libsanitizer \
		--disable-threads \
		--disable-shared \
		--enable-languages=c \
		--without-headers \
		--target=$(SP_ARCH_TARGET_CLANG) \
		CFLAGS="-O3 -fPIC" \
		CPPLAGS="-O3 -fPIC" \
		CXXFLAGS="-O3 -fPIC" \
		CC=$(abspath $(HOST_GCC_CC)) \
		CXX=$(abspath $(HOST_GCC_CXX))
	make -C build/target-gcc all-gcc $(SP_NJOBS)
	make -C build/target-gcc all-target-libgcc $(SP_NJOBS)
	make -C build/target-gcc install-gcc
	make -C build/target-gcc install-target-libgcc

	rm -rf build/target-glibc
	mkdir -p build/target-glibc
	cd build/target-glibc; PATH=$(abspath $(TARGET_SYSROOT)/bin):$$PATH $(abspath $(GLIBC_SRC_DIR))/configure \
		--prefix $(abspath $(TARGET_SYSROOT)) \
		--with-headers=$(abspath $(TARGET_SYSROOT))/include \
		--with-sysroot=$(abspath $(TARGET_SYSROOT)) \
		--with-binutils=$(abspath $(TARGET_SYSROOT))/bin \
		--disable-werror --without-selinux \
		--disable-timezone-tools \
		--disable-multilib \
		--disable-sanity-checks \
		--disable-static-nss \
		--disable-crypt \
		--disable-nss-crypt \
		--disable-nss-crypt \
		--disable-build-nscd \
		--disable-nscd \
		--disable-mathvec \
		--enable-kernel=$(LINUX_KERNEL_VER) \
		--host=$(SP_ARCH_TARGET_CLANG) \
		--target=$(SP_ARCH_TARGET_CLANG) \
		CFLAGS="-Os -fPIC" \
		CPPLAGS="-Os -fPIC" \
		CXXFLAGS="-Os -fPIC" \
		CC="$(abspath $(TARGET_CC)) -fPIC" \
		CXX="$(abspath $(TARGET_CC)) -fPIC"
	$(HOST_GCC_MAKE) -C build/target-glibc $(SP_NJOBS)
	$(HOST_GCC_MAKE) -C build/target-glibc install
	sed  -i 's#$(abspath $(TARGET_SYSROOT))/lib/##g' $(TARGET_SYSROOT)/lib/libc.so

$(TARGET_CXX): $(TARGET_GLIBC)
	@echo "Build TARGET_CXX $(TARGET_CXX)"
	rm -rf build/target-g++
	mkdir -p build/target-g++
	cd build/target-g++; PATH=$(abspath $(TARGET_SYSROOT)/bin):$$PATH $(abspath $(GCC_SRC_DIR))/configure \
		--prefix $(abspath $(TARGET_SYSROOT)) \
		--libdir $(abspath $(TARGET_SYSROOT))/lib \
		--disable-nls \
		--disable-multilib \
		--disable-libsanitizer \
		--enable-languages=c,c++ \
		--with-sysroot=$(abspath $(TARGET_SYSROOT)) \
		--with-build-sysroot=$(abspath $(TARGET_SYSROOT)) \
		--target=$(SP_ARCH_TARGET_CLANG) \
		CFLAGS="-O3 -fPIC" \
		CPPLAGS="-O3 -fPIC" \
		CXXFLAGS="-O3 -fPIC" \
		CC=$(abspath $(HOST_GCC_CC)) \
		CXX=$(abspath $(HOST_GCC_CXX))
	make -C build/target-g++ all $(SP_NJOBS)
	make -C build/target-g++ install

$(TARGET_CMAKE_GCC_TOOLCHAIN): $(TARGET_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(TARGET_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(abspath $(TARGET_CC)))" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(abspath $(TARGET_CXX)))" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@

$(TARGET_LIBCXX): $(TARGET_SYSROOT)/sysroot $(TARGET_CXX) $(TARGET_CMAKE_GCC_TOOLCHAIN)
	@echo "Build TARGET_LIBCXX $(TARGET_LIBCXX)"
	rm -rf build/libcxx_gcc_libcxx
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_DIR)/runtimes -B build/libcxx_gcc_libcxx \
		-DCXX_SUPPORTS_NOSTDLIBXX_FLAG=Off \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;" \
		-DLLVM_HOST_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_SHARED=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(realpath $(TARGET_SYSROOT))
	cmake --build build/libcxx_gcc_libcxx
	cmake --install build/libcxx_gcc_libcxx

$(TARGET_LIBCRT): $(TARGET_LIBCXX)
	@echo "Build TARGET_LIBCRT $(TARGET_LIBCRT)"
	rm -rf build/libcxx_gcc_crt
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_DIR)/runtimes -B build/libcxx_gcc_crt \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;" \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_HOST_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(TARGET_SYSROOT)) \
		-DCOMPILER_RT_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DCMAKE_INSTALL_PREFIX=$(realpath $(TARGET_SYSROOT))
	cmake --build build/libcxx_gcc_crt
	cmake --install build/libcxx_gcc_crt

$(TARGET_SYSROOT)/lib/clang: $(TARGET_LIBCRT)
	rm -rf build/libcxx_gcc_clang
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_CMAKE_GCC_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_DIR)/llvm -B build/libcxx_gcc_clang \
		-DLLVM_ENABLE_PROJECTS="clang" \
		-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
		-DLLVM_HOST_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DLLVM_TARGET_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(realpath $(TARGET_SYSROOT))
	cd build/libcxx_gcc_clang; ninja install-clang-resource-headers

target: $(TARGET_AS) $(TARGET_CC) $(TARGET_GLIBC) $(TARGET_CXX) $(TARGET_LIBCXX) $(TARGET_LIBCRT) $(TARGET_SYSROOT)/lib/clang
