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

# Stage 0 — бутстрап.
#
# Ключевой момент: системный clang (например, Alpine) по умолчанию использует
# GCC-рантайм (libgcc, crtbegin из /usr) и не несёт compiler-rt builtins, а его
# musl ожидает libssp_nonshared из /usr. Поэтому ГЛАВНЫЙ LLVM собираем системным
# clang ПРОТИВ СИСТЕМНОГО окружения (без подмены sysroot — gcc-crt и libssp
# доступны в /usr), а рантаймы (compiler-rt/libunwind/libc++abi/libc++) собирает
# уже свежесобранный (upstream) clang под НАШ musl через DEFAULT_SYSROOT: upstream
# clang не инжектит -lssp_nonshared и берёт crtbegin из compiler-rt.
#
# На выходе: clang/lld в sysroot-stage0, настроенный на наш musl
# (DEFAULT_SYSROOT + compiler-rt/libc++/lld/libunwind), пригодный для сборки
# самодостаточного stage1.

STAGE0_SYSROOT := sysroot-stage0
STAGE0_MUSL := $(STAGE0_SYSROOT)/lib/libc.a
STAGE0_CLANG_CC := $(STAGE0_SYSROOT)/bin/clang
STAGE0_CLANG_CXX := $(STAGE0_SYSROOT)/bin/clang++

#
# 1. musl (запиненная, out-of-tree) + kernel headers -> sysroot-stage0.
#    Ставит заголовки, libc.a/libc.so, crt*.o, загрузчик ld-musl-$(SP_ARCH).so.1.
#
$(STAGE0_MUSL): $(LINUX_HEADERS)
	@echo "Build STAGE0 musl: $(STAGE0_MUSL)"
	rm -rf build/stage0-musl
	mkdir -p build/stage0-musl
	cd build/stage0-musl; $(MUSL_SRC_DIR)/configure \
		--prefix=$(abspath $(STAGE0_SYSROOT)) \
		--syslibdir=$(abspath $(STAGE0_SYSROOT))/lib \
		CC=$(BOOTSTRAP_CC)
	$(MAKE) -C build/stage0-musl $(SP_NJOBS)
	$(MAKE) -C build/stage0-musl install
	cp -rn $(LINUX_HEADERS_DIR)/include/* $(STAGE0_SYSROOT)/include/
	mkdir -p $(STAGE0_SYSROOT)/usr
	cd $(STAGE0_SYSROOT)/usr; ln -sf ../include include; ln -sf ../lib lib

#
# 2. clang + lld + inline-рантаймы. Главный LLVM — системным clang против системы;
#    рантаймы — встроенным clang под наш musl (DEFAULT_SYSROOT).
#
$(STAGE0_CLANG_CC): $(STAGE0_MUSL)
	@echo "Build STAGE0 clang: $(STAGE0_CLANG_CC)"
	rm -rf build/stage0-llvm
	cmake -G Ninja -S $(LLVM_DIR)/llvm -B build/stage0-llvm \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=$(BOOTSTRAP_CC) \
		-DCMAKE_CXX_COMPILER=$(BOOTSTRAP_CXX) \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;libunwind;libcxxabi;libcxx" \
		-DLLVM_TARGETS_TO_BUILD=$(SP_ARCH_LLVM) \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_ENABLE_ZLIB=Off \
		-DLLVM_ENABLE_LIBXML2=Off \
		-DLLVM_INCLUDE_BENCHMARKS=Off \
		-DLLVM_BUILD_BENCHMARKS=Off \
		-DDEFAULT_SYSROOT=$(abspath $(STAGE0_SYSROOT)) \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLIBCXX_HAS_MUSL_LIBC=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
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
		-DLLVM_LOCAL_RPATH='$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))
	cmake --build build/stage0-llvm
	cmake --install build/stage0-llvm
	touch $(STAGE0_CLANG_CC)

stage0: $(STAGE0_MUSL) $(STAGE0_CLANG_CC)

.PHONY: stage0
