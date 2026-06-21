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

# Stage 1 — финальный, self-hosted.
#
# Компилятором stage0 собираем полностью самодостаточный набор в sysroot-out:
#   1. musl (заново, stage0-clang)
#   2. shared-рантаймы (libc++/libc++abi/libunwind/compiler-rt) — поставляются
#   3. финальный clang/lld (+ libLLVM.so) с мульти-таргетным backend-ом
#   4. glslang + spirv-tools
#
# Итоговые бинарники слинкованы с нашим libc++ (rpath $ORIGIN/../lib), а PT_INTERP
# указывает на стандартный /lib/ld-musl-$(SP_ARCH).so.1 — поэтому тулчейн работает
# на любом musl-Linux. lldb/zlib/libxml2/LTO в первой версии не собираются.

STAGEOUT_SYSROOT := sysroot-out
STAGEOUT_MUSL := $(STAGEOUT_SYSROOT)/lib/libc.a
STAGEOUT_LIBCXX := $(STAGEOUT_SYSROOT)/lib/libc++.so.1.0
STAGEOUT_CLANG_CC := $(STAGEOUT_SYSROOT)/bin/clang
STAGEOUT_CLANG_CXX := $(STAGEOUT_SYSROOT)/bin/clang++
STAGEOUT_TOOLCHAIN := $(STAGEOUT_SYSROOT)/clang.cmake

#
# 1. musl в финальный sysroot, собранная stage0-clang
#
$(STAGEOUT_MUSL): $(STAGE0_CLANG_CC) $(LINUX_HEADERS)
	@echo "Build STAGEOUT musl: $(STAGEOUT_MUSL)"
	rm -rf build/stageout-musl
	mkdir -p build/stageout-musl
	cd build/stageout-musl; $(MUSL_SRC_DIR)/configure \
		--prefix=$(abspath $(STAGEOUT_SYSROOT)) \
		--syslibdir=$(abspath $(STAGEOUT_SYSROOT))/lib \
		CC=$(abspath $(STAGE0_CLANG_CC))
	$(MAKE) -C build/stageout-musl $(SP_NJOBS)
	$(MAKE) -C build/stageout-musl install
	cp -rn $(LINUX_HEADERS_DIR)/include/* $(STAGEOUT_SYSROOT)/include/
	mkdir -p $(STAGEOUT_SYSROOT)/usr
	cd $(STAGEOUT_SYSROOT)/usr; ln -sf ../include include; ln -sf ../lib lib

# Сборка ведётся ПРОТИВ полного sysroot-stage0 (там есть musl + libunwind +
# libc++ + compiler-rt), поэтому compiler-check и feature-detection (например,
# отсутствие __cxa_thread_atexit_impl в musl) отрабатывают по-настоящему.
# Артефакты при этом устанавливаются в sysroot-out (CMAKE_INSTALL_PREFIX).
$(STAGEOUT_TOOLCHAIN): $(STAGEOUT_MUSL)
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSROOT $(abspath $(STAGE0_SYSROOT)))' >> $@
	@echo 'set(CMAKE_C_COMPILER $(abspath $(STAGE0_CLANG_CC)))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER $(abspath $(STAGE0_CLANG_CXX)))' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@

#
# 2. shared-рантаймы (поставляемые + для линковки финального clang)
#
$(STAGEOUT_LIBCXX): $(STAGEOUT_TOOLCHAIN)
	@echo "Build STAGEOUT runtimes: $(STAGEOUT_LIBCXX)"
	rm -rf build/stageout-runtimes
	cmake -G Ninja -S $(LLVM_DIR)/runtimes -B build/stageout-runtimes \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGEOUT_TOOLCHAIN)) \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;libunwind;libcxxabi;libcxx" \
		-DLLVM_TARGETS_TO_BUILD=$(SP_ARCH_LLVM) \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_ENABLE_PIC=On \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_SHARED=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_CRT=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_BUILD_ORC=OFF \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=On \
		-DCOMPILER_RT_USE_BUILTINS_LIBRARY=On \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=On \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT))
	cmake --build build/stageout-runtimes
	cmake --install build/stageout-runtimes

#
# 3. финальный clang/lld + libLLVM.so. Мульти-таргетный backend, чтобы
#    хост-компилятор мог кросс-компилировать движок под все цели.
#
$(STAGEOUT_CLANG_CC): $(STAGEOUT_LIBCXX)
	@echo "Build STAGEOUT clang: $(STAGEOUT_CLANG_CC)"
	rm -rf build/stageout-llvm
	cmake -G Ninja -S $(LLVM_DIR)/llvm -B build/stageout-llvm \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGEOUT_TOOLCHAIN)) \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;libunwind;libcxxabi;libcxx" \
		-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_ENABLE_ZLIB=Off \
		-DLLVM_ENABLE_LIBXML2=Off \
		-DLLVM_INCLUDE_BENCHMARKS=Off \
		-DLLVM_BUILD_BENCHMARKS=Off \
		-DLLVM_ENABLE_PIC=On \
		-DCMAKE_POSITION_INDEPENDENT_CODE=On \
		-DLLVM_BUILD_LLVM_DYLIB=On \
		-DLLVM_LINK_LLVM_DYLIB=On \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DCLANG_INCLUDE_TESTS=Off \
		-DCLANG_LINK_CLANG_DYLIB=Off \
		-DCLANG_ENABLE_ARCMT=Off \
		-DCLANG_ENABLE_STATIC_ANALYZER=Off \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXX_ENABLE_SHARED=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_SHARED=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=On \
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
		-DCOMPILER_RT_DEFAULT_TARGET_ARCH="$(SP_ARCH)" \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=On \
		-DCOMPILER_RT_USE_BUILTINS_LIBRARY=On \
		-DDEFAULT_SYSROOT=.. \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT))
	cmake --build build/stageout-llvm
	cmake --install build/stageout-llvm
	touch $(STAGEOUT_CLANG_CC)

#
# 4. SPIR-V tools и glslang (нужны движку: HOST_SPIRV_LINK / HOST_GLSLANG).
#    SPIRV-Headers берём НАПРЯМУЮ из исходников, а не из sysroot: иначе spirv-tools
#    добавляет -I<sysroot>/include, и C-заголовки musl попадают в путь поиска
#    раньше заголовков libc++ — ломается #include_next в libc++ (<cstdio> и т.п.)
#    и всплывает -Wundef на __STDC_VERSION__. ENABLE_OPT в glslang выключен в первой
#    версии (развязка от spirv-tools); движок использует spirv-opt отдельно.
#
$(STAGEOUT_SYSROOT)/bin/spirv-opt: $(STAGEOUT_CLANG_CC)
	@echo "Build SPIR-V tools"
	rm -rf build/stageout-spirv-tools
	cmake -G Ninja -S $(SPIRV_TOOLS_DIR) -B build/stageout-spirv-tools \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGEOUT_TOOLCHAIN)) \
		-DSPIRV_TOOLS_BUILD_STATIC=On \
		-DSPIRV_WERROR=Off \
		-DSPIRV-Headers_SOURCE_DIR=$(SPIRV_HEADERS_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DBUILD_SHARED_LIBS=Off \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT))
	cmake --build build/stageout-spirv-tools
	cmake --install build/stageout-spirv-tools
	touch $@

$(STAGEOUT_SYSROOT)/bin/glslang: $(STAGEOUT_SYSROOT)/bin/spirv-opt
	@echo "Build glslang"
	rm -rf build/stageout-glslang
	cmake -G Ninja -S $(GLSLANG_DIR) -B build/stageout-glslang \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGEOUT_TOOLCHAIN)) \
		-DGLSLANG_TESTS=Off \
		-DENABLE_HLSL=Off \
		-DENABLE_OPT=Off \
		-DGLSLANG_ENABLE_INSTALL=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_CXX_FLAGS_INIT="-ffunction-sections -fdata-sections" \
		-DCMAKE_EXE_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-lc++ -lc++abi -Wl,--gc-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DBUILD_SHARED_LIBS=Off \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGEOUT_SYSROOT))
	cmake --build build/stageout-glslang
	cmake --install build/stageout-glslang
	touch $@

stage1: $(STAGEOUT_MUSL) $(STAGEOUT_LIBCXX) $(STAGEOUT_CLANG_CC) \
	$(STAGEOUT_SYSROOT)/bin/spirv-opt $(STAGEOUT_SYSROOT)/bin/glslang

.PHONY: stage1
