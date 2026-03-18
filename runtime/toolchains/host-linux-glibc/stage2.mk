# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

# Здесь мы собираем минимальный glibc и финальную версию clang для использования при распространении

STAGE2_SYSROOT := sysroot-clang2
STAGEOUT_SYSROOT := sysroot-clang-out
STAGE2_MAKE := $(STAGE1_SYSROOT)/bin/make
STAGE2_GLIBC := $(STAGE2_SYSROOT)/lib/libc.so.6
STAGE2_ZLIB := $(STAGE2_SYSROOT)/lib/libz.a
STAGE2_LIBXML2 := $(STAGE2_SYSROOT)/lib/libxml2.a
STAGE2_LIBCXX := $(STAGE2_SYSROOT)/lib/libc++.a

STAGE2_CLANG_CC := $(STAGEOUT_SYSROOT)/bin/clang
STAGE2_CLANG_CXX := $(STAGEOUT_SYSROOT)/bin/clang++
STAGE2_CLANG_LLDB := $(STAGEOUT_SYSROOT)/bin/lldb
STAGEOUT_MAKE := $(STAGEOUT_SYSROOT)/bin/make
STAGEOUT_LIBCXX := $(STAGEOUT_SYSROOT)/lib/libc++.so.1.0
STAGEOUT_ZLIB := $(STAGEOUT_SYSROOT)/lib/libz.a

STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN := $(STAGE2_SYSROOT)/clang.stage1.cmake

$(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN): $(STAGE2_SYSROOT)/sysroot
	@echo "set(CMAKE_SYSTEM_NAME Linux)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE2_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
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

$(STAGE2_LIBXML2): $(ZLIB_DIR) $(STAGE1_CLANG_CC) $(STAGE2_LIBCXX)
	@echo "Build STAGE2_LIBXML2: $(STAGE2_LIBXML2)"
	rm -rf build/stage2-libxml
	mkdir -p build/stage2-libxml
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LIBXML2_DIR) -B build/stage2-libxml \
		-DBUILD_SHARED_LIBS=Off \
		-DLIBXML2_WITH_DEBUG=Off \
		-DLIBXML2_WITH_PROGRAMS=Off \
		-DLIBXML2_WITH_TESTS=Off \
		-DLIBXML2_WITH_TLS=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE2_SYSROOT))
	cmake --build build/stage2-libxml
	cmake --install build/stage2-libxml

STAGE2_ENABLE_SHARED := On

$(STAGE2_LIBCXX): $(STAGE2_ZLIB) $(STAGE1_CLANG_CC) $(STAGE1_CMAKE_CLANG_TOOLCHAIN)
	@echo "Build STAGE2_LIBCXX: $(STAGE2_LIBCXX)"
	rm -rf build/stage2-libcxx
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE1_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_DIR)/runtimes -B build/stage2-libcxx \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PIC=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=ON \
		-DCOMPILER_RT_USE_BUILTINS_LIBRARY=ON \
		-DCMAKE_INSTALL_PREFIX=$(STAGE2_SYSROOT) \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib'
	cmake --build build/stage2-libcxx
	cmake --install build/stage2-libcxx


$(STAGE2_CLANG_CC): $(STAGE2_LIBCXX) $(STAGE2_ZLIB) $(STAGE2_LIBXML2) $(STAGE2_GLIBC) $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)
	rm -rf build/stage2-llvm
	@echo Build STAGE2_CLANG_CC $(STAGE2_CLANG_CC)
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_DIR)/llvm -B build/stage2-llvm \
		-DLLVM_ENABLE_PROJECTS="lld;clang;lldb" \
		-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
		-DLLVM_ENABLE_PIC=On \
		-DLLVM_ENABLE_LIBXML2=FORCE_ON \
		-DLLDB_NO_INSTALL_DEFAULT_RPATH=On \
		-DLLVM_BUILD_LLVM_DYLIB=ON \
		-DLLVM_LINK_LLVM_DYLIB=ON \
		-DLLVM_ENABLE_LTO=Full \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
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
		-DLIBCXX_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
        -DCOMPILER_RT_DEFAULT_TARGET_ARCH="$(SP_ARCH)" \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=ON \
		-DCOMPILER_RT_USE_BUILTINS_LIBRARY=ON \
		-DLLDB_ENABLE_PYTHON=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT)) \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections -lpthread" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections -lpthread" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE \
		-DDEFAULT_SYSROOT=..
	cmake --build build/stage2-llvm
	cmake --install build/stage2-llvm
	touch $(STAGE2_CLANG_CC)

$(STAGEOUT_LIBCXX): $(STAGE2_ZLIB) $(STAGE1_CLANG_CC) $(STAGE1_CMAKE_CLANG_TOOLCHAIN)
	@echo "Build STAGEOUT_LIBCXX: $(STAGEOUT_LIBCXX)"
	rm -rf build/stage2-libcxx2
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE1_CMAKE_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(LLVM_DIR)/runtimes -B build/stage2-libcxx2 \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PIC=On \
		-DLLVM_ENABLE_LTO=Full \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=$(STAGE2_ENABLE_SHARED) \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=ON \
		-DCOMPILER_RT_USE_BUILTINS_LIBRARY=ON \
		-DCMAKE_INSTALL_PREFIX=$(STAGEOUT_SYSROOT) \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib'
	cmake --build build/stage2-libcxx2
	cmake --install build/stage2-libcxx2

$(STAGE2_SYSROOT)/include/vulkan/vulkan.h: $(STAGE2_CLANG_CC)
	rm -rf build/stage2-vulkan-headers
	@echo Build Vulkan Headers $(STAGE2_SYSROOT)/include/vulkan/vulkan.h
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(VULKAN_HEADERS_DIR) -B build/stage2-vulkan-headers \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE2_SYSROOT))
	cmake --build build/stage2-vulkan-headers
	cmake --install build/stage2-vulkan-headers
	touch $@

$(STAGE2_SYSROOT)/include/spirv/unified1/spirv.h: $(STAGE2_CLANG_CC)
	rm -rf build/stage2-spirv-headers
	@echo Build Vulkan Headers $(STAGE2_SYSROOT)/include/vulkan/vulkan.h
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(SPIRV_HEADERS_DIR) -B build/stage2-spirv-headers \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE2_SYSROOT))
	cmake --build build/stage2-spirv-headers
	cmake --install build/stage2-spirv-headers
	touch $@

$(STAGEOUT_SYSROOT)/bin/spirv-opt: $(STAGE2_SYSROOT)/include/spirv/unified1/spirv.h
	rm -rf build/stage2-spirv-tools
	@echo Build SPIR-V tools $(STAGE2_SYSROOT)/include/vulkan/vulkan.h
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(SPIRV_TOOLS_DIR) -B build/stage2-spirv-tools \
		-DSPIRV_TOOLS_BUILD_STATIC=Off \
		-DSPIRV-Headers_SOURCE_DIR=$(abspath $(STAGE2_SYSROOT)) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT)) \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections -flto" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections -flto" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DBUILD_SHARED_LIBS=On
	cmake --build build/stage2-spirv-tools
	cmake --install build/stage2-spirv-tools --prefix $(abspath $(STAGE2_SYSROOT))
	cmake --install build/stage2-spirv-tools
	touch $@

$(STAGEOUT_SYSROOT)/bin/glslang: $(STAGEOUT_SYSROOT)/bin/spirv-opt $(STAGE2_SYSROOT)/include/vulkan/vulkan.h
	rm -rf build/stage2-glslang
	@echo Build glslang compiler $(STAGE2_SYSROOT)/include/vulkan/vulkan.h
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN)) \
		-G Ninja -S $(GLSLANG_DIR) -B build/stage2-glslang \
		-DGLSLANG_TESTS=Off \
		-DGLSLANG_ENABLE_INSTALL=Off \
		-DENABLE_HLSL=Off \
		-DENABLE_OPT=On \
		-DALLOW_EXTERNAL_SPIRV_TOOLS=On \
		-DGLSLANG_ENABLE_INSTALL=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT)) \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -flto" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections -flto" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections -flto" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DBUILD_SHARED_LIBS=On
	cmake --build build/stage2-glslang
	cmake --install build/stage2-glslang
	touch $@

$(STAGEOUT_MAKE): $(STAGE1_SYSROOT)/sysroot $(STAGE0_CLANG_CC) $(MAKE_SRC_DIR)
	rm -rf build/stage1-make
	mkdir -p build/stage1-make
	cd build/stage1-make; $(abspath $(MAKE_SRC_DIR))/configure --prefix $(abspath $(STAGEOUT_SYSROOT)) \
		CC=$(abspath $(STAGE1_SYSROOT))/bin/clang \
		CFLAGS="--sysroot $(realpath $(STAGE2_SYSROOT)) -flto" \
		LDFLAGS="-flto"
	make -C build/stage1-make $(SP_NJOBS)
	make -C build/stage1-make install

stage2: $(STAGE2_GLIBC) $(STAGE2_CMAKE_STAGE1_CLANG_TOOLCHAIN) $(STAGE2_ZLIB) \
	$(STAGE2_LIBCXX) $(STAGE2_CLANG_CC) $(STAGEOUT_LIBCXX) \
	$(STAGE2_SYSROOT)/include/vulkan/vulkan.h $(STAGE2_SYSROOT)/include/spirv/unified1/spirv.h \
	$(STAGEOUT_SYSROOT)/bin/spirv-opt $(STAGEOUT_SYSROOT)/bin/glslang \
	$(STAGEOUT_MAKE)
