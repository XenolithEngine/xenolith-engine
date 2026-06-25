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

# Упрощённая (cross) сборка host-тулчейна для Linux/glibc — по аналогии с
# host-linux-musl/cross, host-macos/cross, host-windows-msvc/cross.
#
# Общая логика cross-хостов (см. runtime/toolchains/README.adoc): сначала
# собирается ОДИН нативный host билд-машины (make host), затем все ТАРГЕТЫ
# (make target-...), затем из таргетов (где нужно) — cross-хосты. В отличие от
# musl (libc собирается clang'ом на лету), glibc нельзя собрать без GCC, поэтому
# здесь ПЕРЕИСПОЛЬЗУЕТСЯ уже собранный glibc-sysroot целевого таргета
# (target-linux/glibc/sysroot-target-<triple>): в нём готовая glibc нужной
# версии + статическая libc++ + compiler-rt + заголовки. Сверху clang'ом
# билд-машины (hosts/<HOST_ID>) кросс-компилируются shared-рантаймы и финальный
# clang/lld/lldb — без трёхстадийного GCC-бутстрапа нативного host-linux-glibc.

GIT_TAG ?= $(shell git describe --tags --abbrev=0 2>/dev/null || echo dev)

THIS_FILE := $(lastword $(MAKEFILE_LIST))
LOCAL_ROOT := $(dir $(realpath $(THIS_FILE)))

include $(LOCAL_ROOT)../../common/utils/detect-platform.mk

SP_NJOBS ?= -j$(shell nproc 2>/dev/null || echo 8)
SP_LLVM_VER := 21

# Целевая арка glibc-хоста (которую собираем). x86_64 | aarch64 | riscv64.
SP_ARCH_HOST ?= x86_64
SP_ARCH := $(SP_ARCH_HOST)
SP_ARCH_CLANG := $(SP_ARCH_HOST)-unknown-linux-gnu

ifeq ($(SP_ARCH_HOST),x86_64)
SP_ARCH_LLVM := X86
else ifeq ($(SP_ARCH_HOST),aarch64)
SP_ARCH_LLVM := AArch64
else ifeq ($(SP_ARCH_HOST),riscv64)
SP_ARCH_LLVM := RISCV
else
$(error Unknown arch: $(SP_ARCH_HOST) (expected x86_64|aarch64|riscv64))
endif

# Готовый тулчейн билд-машины (его clang кросс-компилирует всё остальное).
HOST_ROOT := $(realpath $(LOCAL_ROOT)../../hosts/$(HOST_ID))
ifeq ($(HOST_ROOT),)
$(error host toolchain not found at hosts/$(HOST_ID) - run 'make host' first)
endif
HOST_CC := $(HOST_ROOT)/bin/clang
HOST_CXX := $(HOST_ROOT)/bin/clang++

# Готовый glibc-sysroot из соответствующего таргета (build targets first).
# Корневой Makefile тулчейнов делает host-cross-* зависимым от target-*.
BASE_SYSROOT := $(realpath $(LOCAL_ROOT)../../targets/$(SP_ARCH_CLANG))
ifeq ($(BASE_SYSROOT),)
$(error glibc target sysroot not found at \
    target-linux/glibc/sysroot-target-$(SP_ARCH_CLANG) - \
    build the target first: make target-$(SP_ARCH_CLANG))
endif

# Исходники из общего ../../src.
LLVM_DIR := $(realpath $(LOCAL_ROOT)../../src/llvm-project)
ZLIB_DIR := $(realpath $(LOCAL_ROOT)../../src/zlib)
LIBXML2_DIR := $(realpath $(LOCAL_ROOT)../../src/libxml2)
SPIRV_HEADERS_DIR := $(realpath $(LOCAL_ROOT)../../src/spirv-headers)
SPIRV_TOOLS_DIR := $(realpath $(LOCAL_ROOT)../../src/spirv-tools)
GLSLANG_DIR := $(realpath $(LOCAL_ROOT)../../src/glslang)

ifeq ($(LLVM_DIR),)
$(error LLVM not found, run 'make download' in runtime/toolchains)
endif

# LTO финального clang: по умолчанию Full (как у остальных хостов, кроме windows aarch64).
# Отключить: SP_LLVM_LTO= ; полегче: SP_LLVM_LTO=Thin.
SP_LLVM_LTO ?= Full
ifeq ($(SP_LLVM_LTO),)
LTO_FLAGS :=
else
LTO_FLAGS := -DLLVM_ENABLE_LTO=$(SP_LLVM_LTO) \
	-DLLVM_PARALLEL_LINK_JOBS=6 \
	-DCROSS_TOOLCHAIN_FLAGS_NATIVE=-DLLVM_ENABLE_LTO=OFF
endif

# GNU make для поставки в составе тулчейна.
MAKE_SRC_VER := 4.4.1
MAKE_SRC_TARBALL := make-$(MAKE_SRC_VER).tar.gz
MAKE_SRC_URL := https://ftp.gnu.org/gnu/make/$(MAKE_SRC_TARBALL)
MAKE_SRC_DIR := $(abspath src/make-$(MAKE_SRC_VER))

src/$(MAKE_SRC_TARBALL):
	mkdir -p src
	wget -O $@ $(MAKE_SRC_URL)

$(MAKE_SRC_DIR)/configure: src/$(MAKE_SRC_TARBALL)
	rm -rf $(MAKE_SRC_DIR)
	cd src; tar xf $(MAKE_SRC_TARBALL)
	touch $@

#
# Установочный sysroot собираемого тулчейна (shared-рантаймы + clang).
# glibc/compiler-rt/заголовки берутся из BASE_SYSROOT (read-only).
#
STAGING := $(abspath sysroot/$(SP_ARCH_CLANG))

HOST_AR := $(HOST_ROOT)/bin/llvm-ar
HOST_RANLIB := $(HOST_ROOT)/bin/llvm-ranlib

#
# Toolchain-файл для сборки РАНТАЙМОВ: clang билд-машины + --target + sysroot
# целевой glibc (BASE). BASE содержит статическую libc++.a -> обычный
# compiler-check проходит. Рантаймы ставятся во временный префикс.
#
RT_CMAKE := $(abspath $(STAGING)/rt.cmake)
RESOURCE_DIR := $(STAGING)/lib/clang
RESOURCE_IDIR := $(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include

$(RT_CMAKE):
	rm -rf $(STAGING)
	mkdir -p $(STAGING) $(STAGING)/include $(STAGING)/lib
	cp -r $(BASE_SYSROOT)/include_libc/* $(STAGING)/include
	cp -r $(BASE_SYSROOT)/lib/* $(STAGING)/lib
	cp -r $(BASE_SYSROOT)/usr/lib/libunwind.a $(STAGING)/lib

	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH_HOST))' >> $@
	@echo 'set(CMAKE_SYSROOT $(STAGING))' >> $@
	@echo 'set(CMAKE_C_COMPILER $(HOST_CC))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER $(HOST_CXX))' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -idirafter $(RESOURCE_IDIR) -fPIC -ffunction-sections -fdata-sections")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -idirafter $(RESOURCE_IDIR) -fPIC -ffunction-sections -fdata-sections")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -fPIC")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -lpthread -ldl")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "-resource-dir $(RESOURCE_DIR)")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@

#
# 2. shared-рантаймы (libc++/libc++abi/libunwind) -> временный префикс. Только
#    shared (STATIC=Off) и PIC: пойдут в libLLVM.so, а статика из BASE — не PIC.
#
RUNTIMES := $(STAGING)/lib/libc++.so.1.0

$(RUNTIMES): $(RT_CMAKE)
	@echo "Build shared runtimes (libc++/libc++abi/libunwind) for $(SP_ARCH_CLANG)"
	rm -rf build/runtimes
	cmake -G Ninja -S $(LLVM_DIR)/runtimes -B build/runtimes \
		-DCMAKE_TOOLCHAIN_FILE=$(RT_CMAKE) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER_WORKS=1 \
		-DCMAKE_CXX_COMPILER_WORKS=1 \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt;" \
		-DLLVM_TARGETS_TO_BUILD=$(SP_ARCH_LLVM) \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DLLVM_ENABLE_PIC=On \
		-DLIBCXX_ENABLE_STATIC=Off \
		-DLIBCXX_ENABLE_SHARED=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_ENABLE_STATIC=Off \
		-DLIBCXXABI_ENABLE_SHARED=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_STATIC=Off \
		-DLIBUNWIND_ENABLE_SHARED=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=ON \
		-DCOMPILER_RT_USE_BUILTINS_LIBRARY=ON \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_PREFIX=$(STAGING)
	cmake --build build/runtimes
	cmake --install build/runtimes
	rm $(STAGING)/lib/libunwind.a

#
# 3. zlib + libxml2 (нужны clang/lldb).
#
ZLIB := $(STAGING)/lib/libz.a
LIBXML2 := $(STAGING)/lib/libxml2.a

$(ZLIB): $(RUNTIMES)
	@echo "Build zlib"
	rm -rf build/zlib
	cmake -G Ninja -S $(ZLIB_DIR) -B build/zlib \
		-DCMAKE_TOOLCHAIN_FILE=$(RT_CMAKE) \
		-DCMAKE_BUILD_TYPE=Release \
		-DZLIB_BUILD_EXAMPLES=Off \
		-DZLIB_BUILD_SHARED=Off \
		-DZLIB_BUILD_STATIC=On \
		-DCMAKE_POSITION_INDEPENDENT_CODE=On \
		-DCMAKE_INSTALL_PREFIX=$(STAGING)
	cmake --build build/zlib
	cmake --install build/zlib

$(LIBXML2): $(ZLIB)
	@echo "Build libxml2"
	rm -rf build/libxml2
	cmake -G Ninja -S $(LIBXML2_DIR) -B build/libxml2 \
		-DCMAKE_TOOLCHAIN_FILE=$(RT_CMAKE) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=Off \
		-DLIBXML2_WITH_DEBUG=Off \
		-DLIBXML2_WITH_PROGRAMS=Off \
		-DLIBXML2_WITH_TESTS=Off \
		-DLIBXML2_WITH_PYTHON=Off \
		-DLIBXML2_WITH_ICONV=Off \
		-DLIBXML2_WITH_LZMA=Off \
		-DLIBXML2_WITH_ZLIB=Off \
		-DLIBXML2_WITH_TLS=On \
		-DCMAKE_POSITION_INDEPENDENT_CODE=On \
		-DCMAKE_INSTALL_PREFIX=$(STAGING)
	cmake --build build/libxml2
	cmake --install build/libxml2

#
# 4. финальный clang/lld/lldb + libLLVM.so (мульти-таргетный backend).
#
CLANG := $(STAGING)/bin/clang

#
# Toolchain-файл для сборки clang/зависимостей: sysroot = ЧИСТЫЙ STAGING.
# resource-dir + -isystem <extra> (zlib.h для lldb CTF) + -fPIC — в toolchain-файл
# (он перебивает per-build -DCMAKE_*_FLAGS_INIT).
#
BUILD_CMAKE := $(STAGING)/cross.cmake

$(BUILD_CMAKE):
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH_HOST))' >> $@
	@echo 'set(CMAKE_SYSROOT $(STAGING))' >> $@
	@echo 'set(CMAKE_C_COMPILER $(HOST_CC))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER $(HOST_CXX))' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET $(SP_ARCH_CLANG))' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -idirafter $(RESOURCE_IDIR) -fPIC -ffunction-sections -fdata-sections")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "-resource-dir $(RESOURCE_DIR)  -isystem $(STAGING)/include/c++/v1 -idirafter $(RESOURCE_IDIR) -fPIC -ffunction-sections -fdata-sections")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -fPIC")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -lpthread -ldl")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "-resource-dir $(RESOURCE_DIR) -lpthread -ldl")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@

$(CLANG): $(LIBXML2) $(BUILD_CMAKE)
	@echo "Build clang/lld/lldb ($(SP_ARCH_CLANG))"
	rm -rf build/llvm
	cmake -G Ninja -S $(LLVM_DIR)/llvm -B build/llvm \
		-DCMAKE_TOOLCHAIN_FILE=$(BUILD_CMAKE) \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
		-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_ENABLE_ZLIB=FORCE_ON \
		-DZLIB_LIBRARY=$(ZLIB) \
		-DZLIB_INCLUDE_DIR=$(STAGING)/include \
		-DLLVM_ENABLE_LIBXML2=FORCE_ON \
		-DLIBXML2_LIBRARY=$(LIBXML2) \
		-DLIBXML2_INCLUDE_DIR=$(STAGING)/include/libxml2 \
		-DLLVM_INCLUDE_BENCHMARKS=Off \
		-DLLVM_BUILD_BENCHMARKS=Off \
		$(LTO_FLAGS) \
		-DLLDB_ENABLE_PYTHON=Off \
		-DLLDB_ENABLE_LIBEDIT=Off \
		-DLLDB_ENABLE_CURSES=Off \
		-DLLDB_ENABLE_LZMA=Off \
		-DLLDB_ENABLE_LIBXML2=On \
		-DLLVM_ENABLE_LIBCXX=On \
		-DLLDB_INCLUDE_TESTS=Off \
		-DLLDB_NO_INSTALL_DEFAULT_RPATH=On \
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
		-DDEFAULT_SYSROOT=.. \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_EXE_LINKER_FLAGS="-resource-dir $(RESOURCE_DIR) -lc++ -lc++abi -lpthread -ldl -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-resource-dir $(RESOURCE_DIR) -lc++ -lc++abi -lpthread -ldl -Wl,--gc-sections" \
		-DCMAKE_INSTALL_PREFIX=$(STAGING)
	cmake --build build/llvm
	cmake --install build/llvm
	touch $(CLANG)

#
# 5. SPIR-V tools + glslang (нужны движку: HOST_SPIRV_LINK / HOST_GLSLANG).
#
SPIRV_BINS := spirv-as spirv-cfg spirv-diff spirv-dis spirv-link spirv-lint \
	spirv-objdump spirv-opt spirv-reduce spirv-val

$(STAGING)/bin/spirv-opt: $(CLANG)
	@echo "Build SPIR-V tools"
	rm -rf build/spirv-tools $(SPIRV_PREFIX)
	cmake -G Ninja -S $(SPIRV_TOOLS_DIR) -B build/spirv-tools \
		-DCMAKE_TOOLCHAIN_FILE=$(BUILD_CMAKE) \
		-DSPIRV_TOOLS_BUILD_STATIC=On \
		-DSPIRV_WERROR=Off \
		-DSPIRV-Headers_SOURCE_DIR=$(SPIRV_HEADERS_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXE_LINKER_FLAGS="-resource-dir $(RESOURCE_DIR) -lc++ -lc++abi -lpthread -ldl -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-resource-dir $(RESOURCE_DIR) -lc++ -lc++abi -lpthread -ldl -Wl,--gc-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DBUILD_SHARED_LIBS=Off \
		-DCMAKE_INSTALL_PREFIX=$(STAGING)
	cmake --build build/spirv-tools
	cmake --install build/spirv-tools
	touch $@

$(STAGING)/bin/glslang: $(STAGING)/bin/spirv-opt
	@echo "Build glslang"
	rm -rf build/glslang
	cmake -G Ninja -S $(GLSLANG_DIR) -B build/glslang \
		-DCMAKE_TOOLCHAIN_FILE=$(BUILD_CMAKE) \
		-DGLSLANG_TESTS=Off \
		-DENABLE_HLSL=Off \
		-DENABLE_OPT=On \
		-DALLOW_EXTERNAL_SPIRV_TOOLS=On \
		-DGLSLANG_ENABLE_INSTALL=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXE_LINKER_FLAGS="-resource-dir $(RESOURCE_DIR) -lc++ -lc++abi -lpthread -ldl -Wl,--gc-sections" \
		-DCMAKE_SHARED_LINKER_FLAGS="-resource-dir $(RESOURCE_DIR) -lc++ -lc++abi -lpthread -ldl -Wl,--gc-sections" \
		-DCMAKE_INSTALL_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DCMAKE_BUILD_RPATH='$$ORIGIN:$$ORIGIN/../lib' \
		-DBUILD_SHARED_LIBS=Off \
		-DCMAKE_INSTALL_PREFIX=$(STAGING)
	cmake --build build/glslang
	cmake --install build/glslang
	touch $@

#
# 6. host-овый GNU make в составе тулчейна.
#    Если целевая арка совпадает с аркой билд-машины — это «native» сборка
#    (бинарники запускаются), поэтому --build==--host, и autoconf/gnulib
#    проходят run-тесты и корректно детектят типы (иначе под кросс gnulib
#    подставляет `#define gid_t int` и ломает glibc sys/types.h).
#
ifeq ($(shell uname -m),$(SP_ARCH_HOST))
MAKE_HOST_FLAGS := --build=$(SP_ARCH_CLANG) --host=$(SP_ARCH_CLANG)
else
MAKE_HOST_FLAGS := --host=$(SP_ARCH_CLANG)
endif

$(STAGING)/bin/make: $(CLANG) $(MAKE_SRC_DIR)/configure
	@echo "Build make"
	rm -rf build/make
	mkdir -p build/make
	cd build/make; $(MAKE_SRC_DIR)/configure \
		--prefix=$(STAGING) --without-guile \
		$(MAKE_HOST_FLAGS) \
		CC="$(HOST_CC)" \
		CPP="$(HOST_CC) -E" \
		AR="$(HOST_AR)" RANLIB="$(HOST_RANLIB)" \
		CPPFLAGS="--sysroot=$(STAGING) --target=$(SP_ARCH_CLANG) -isystem $(RESOURCE_IDIR) -resource-dir $(RESOURCE_DIR)" \
		LDFLAGS="--sysroot=$(STAGING) --target=$(SP_ARCH_CLANG) -resource-dir $(RESOURCE_DIR)" \
		CFLAGS="-O2"
	$(MAKE) -C build/make $(SP_NJOBS)
	$(MAKE) -C build/make install

#
# Раскладка в hosts/<arch>-unknown-linux-gnu (strip + symlinks + host.mk).
#
OUT_SYSROOT ?= ../../hosts/$(SP_ARCH_CLANG)
STRIP := $(HOST_ROOT)/bin/llvm-strip

OUT_TOOLS := $(addprefix $(OUT_SYSROOT)/bin/,\
	clang-$(SP_LLVM_VER) \
	lld \
	lldb \
	lldb-argdumper \
	lldb-server \
	make \
	llvm-ar \
	llvm-cov \
	llvm-cxxfilt \
	llvm-dwp \
	llvm-ml \
	llvm-nm \
	llvm-objcopy \
	llvm-objdump \
	llvm-rc \
	llvm-readobj \
	llvm-size \
	llvm-strings \
	llvm-symbolizer \
	glslang \
	spirv-as \
	spirv-cfg \
	spirv-diff \
	spirv-dis \
	spirv-link \
	spirv-lint \
	spirv-objdump \
	spirv-opt \
	spirv-reduce \
	spirv-val)

OUT_LIBS := $(addprefix $(OUT_SYSROOT)/lib/,\
	libc++.so.1.0 \
	libc++abi.so.1.0 \
	libunwind.so.1.0 \
	libLTO.so.$(SP_LLVM_VER).1 \
	libLLVM.so.$(SP_LLVM_VER).1 \
	libRemarks.so.$(SP_LLVM_VER).1 \
	liblldb.so.$(SP_LLVM_VER).1.8)

OUT_INCLUDES := $(addprefix $(OUT_SYSROOT)/include/,\
	clang \
	clang-c \
	llvm \
	llvm-c \
	lldb)

# STAGING-файлы (shared libc++ из STAMP, libLLVM.so/бинарники из clang/glslang/make)
# создаются рецептами, а не как отдельные цели, поэтому OUT-правила зависят от
# маркеров сборки, а источник берут из $(STAGING)/.../$*.
ALL_BUILT := $(STAGING_STAMP) $(CLANG) $(STAGING)/bin/glslang $(STAGING)/bin/make

$(OUT_SYSROOT)/lib/clang: $(ALL_BUILT)
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -rf $@
	cp -rf $(STAGING)/lib/clang $@
	rm -rf $@/lib $@/include $@/$(SP_LLVM_VER)/lib 

$(OUT_SYSROOT)/lib/%: $(ALL_BUILT)
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	$(STRIP) $(STAGING)/lib/$* -o $@
	chmod +x $@

$(OUT_SYSROOT)/bin/%: $(ALL_BUILT)
	@mkdir -p $(OUT_SYSROOT)/bin
	@rm -f $@
	$(STRIP) $(STAGING)/bin/$* -o $@
	chmod +x $@

$(OUT_SYSROOT)/include/%: $(ALL_BUILT)
	rm -rf $@
	mkdir -p $@
	cp -fr $(STAGING)/include/$*/* $@

$(OUT_SYSROOT)/share/licenses: ../licenses
	rm -rf $@
	mkdir -p $@
	cp -fr $</* $@
	touch $@

$(OUT_SYSROOT)/host.mk: $(THIS_FILE)
	@echo 'Build $(OUT_SYSROOT)/host.mk'
	@echo 'HOST_ROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'HOST_BINDIR := $$(HOST_ROOT)/bin' >> $@
	@echo 'HOST_CC := $$(HOST_BINDIR)/cc' >> $@
	@echo 'HOST_CXX := $$(HOST_BINDIR)/c++' >> $@
	@echo 'HOST_AR := $$(HOST_BINDIR)/ar' >> $@
	@echo 'HOST_GLSLANG := $$(HOST_BINDIR)/glslang' >> $@
	@echo 'HOST_SPIRV_LINK := $$(HOST_BINDIR)/spirv-link' >> $@
	@echo 'HOST_GENERAL_CFLAGS := -idirafter $$(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include' >> $@
	@echo 'HOST_GENERAL_CXXFLAGS := -idirafter $$(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include' >> $@
	@echo 'HOST_GENERAL_LDFLAGS :=' >> $@
	@echo 'HOST_EXEC_CFLAGS :=' >> $@
	@echo 'HOST_EXEC_CXXFLAGS :=' >> $@
	@echo 'HOST_EXEC_LDFLAGS :=' >> $@
	@echo 'HOST_LIB_CFLAGS :=' >> $@
	@echo 'HOST_LIB_CXXFLAGS :=' >> $@
	@echo 'HOST_LIB_LDFLAGS :=' >> $@

$(OUT_SYSROOT)/release: $(OUT_LIBS) $(OUT_TOOLS) $(OUT_SYSROOT)/lib/clang $(OUT_SYSROOT)/share/licenses $(OUT_SYSROOT)/host.mk
	cd $(OUT_SYSROOT)/lib; ln -fs libc++abi.so.1.0 libc++abi.so
	cd $(OUT_SYSROOT)/lib; ln -fs libc++abi.so.1.0 libc++abi.so.1
	cd $(OUT_SYSROOT)/lib; ln -fs libc++.so.1.0 libc++.so
	cd $(OUT_SYSROOT)/lib; ln -fs libc++.so.1.0 libc++.so.1
	cd $(OUT_SYSROOT)/lib; ln -fs libunwind.so.1.0 libunwind.so
	cd $(OUT_SYSROOT)/lib; ln -fs libunwind.so.1.0 libunwind.so.1
	cd $(OUT_SYSROOT)/lib; ln -fs libLLVM.so.$(SP_LLVM_VER).1 libLLVM-$(SP_LLVM_VER).so
	cd $(OUT_SYSROOT)/lib; ln -fs libLLVM.so.$(SP_LLVM_VER).1 libLLVM.so
	cd $(OUT_SYSROOT)/lib; ln -fs libLTO.so.$(SP_LLVM_VER).1 libLTO.so
	cd $(OUT_SYSROOT)/lib; ln -fs libRemarks.so.$(SP_LLVM_VER).1 libRemarks.so
	cd $(OUT_SYSROOT)/lib; ln -fs liblldb.so.$(SP_LLVM_VER).1.8 liblldb.so.$(SP_LLVM_VER).1
	cd $(OUT_SYSROOT)/lib; ln -fs liblldb.so.$(SP_LLVM_VER).1.8 liblldb.so

	cd $(OUT_SYSROOT)/bin; ln -fs clang-$(SP_LLVM_VER) clang
	cd $(OUT_SYSROOT)/bin; ln -fs clang clang++
	cd $(OUT_SYSROOT)/bin; ln -fs clang clang-cl
	cd $(OUT_SYSROOT)/bin; ln -fs clang clang-cpp
	cd $(OUT_SYSROOT)/bin; ln -fs glslang glslangValidator
	cd $(OUT_SYSROOT)/bin; ln -fs lld ld64.lld
	cd $(OUT_SYSROOT)/bin; ln -fs lld ld.lld
	cd $(OUT_SYSROOT)/bin; ln -fs lld lld-link
	cd $(OUT_SYSROOT)/bin; ln -fs lld wasm-ld
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-symbolizer llvm-addr2line
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objcopy llvm-bitcode-strip
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar llvm-dlltool
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objcopy llvm-install-name-tool
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar llvm-lib
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ml llvm-ml64
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objdump llvm-otool
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar llvm-ranlib
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-readobj llvm-readelf
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objcopy llvm-strip
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-rc llvm-windres

	cd $(OUT_SYSROOT)/bin; ln -fs clang-$(SP_LLVM_VER) cc
	cd $(OUT_SYSROOT)/bin; ln -fs clang-$(SP_LLVM_VER) c++
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar ar

	echo "$(GIT_TAG)" > $@
	touch $@

#
# 7. xlmake — stappler build-tool. Собирается ДЛЯ целевой арки host'а (clang'ом
#    билд-машины против полного таргета) и ставится в bin тулчейна — по образцу
#    host-macos/cross и host-windows-msvc/cross. Требует собранного полного
#    таргета (toolchains/targets/<triple> с target.mk); правило ниже его триггерит.
#
ROOT_DIR := $(abspath $(LOCAL_ROOT)../..)
OUT_TARGET_SYSROOT := $(ROOT_DIR)/targets/$(SP_ARCH_CLANG)

$(OUT_TARGET_SYSROOT)/target.mk:
	$(MAKE) -C $(ROOT_DIR) target-$(SP_ARCH_CLANG) HOST_REQUIRED=0

$(OUT_SYSROOT)/bin/xlmake: $(OUT_TARGET_SYSROOT)/target.mk $(OUT_SYSROOT)/host.mk
	rm -rf build/xlmake
	$(MAKE) $(SP_NJOBS) -C $(ROOT_DIR)/../../utils/xlmake install \
		STAPPLER_HOST_FILE=$(HOST_ROOT)/host.mk \
		STAPPLER_TARGET_FILE=$(OUT_TARGET_SYSROOT)/target.mk \
		STAPPLER_TARGET=$(SP_ARCH_CLANG) \
		LOCAL_INSTALL_DIR=$(abspath $(OUT_SYSROOT)/bin) \
		LOCAL_OUTDIR=$(abspath build/xlmake) \
		RELEASE=1

download: $(MAKE_SRC_DIR)/configure
	@echo "cross host-linux-glibc reuses target glibc sysroot + LLVM/zlib/... from ../../src"

stage1: $(CLANG) $(STAGING)/bin/glslang $(STAGING)/bin/make

out: $(OUT_SYSROOT)/release $(OUT_SYSROOT)/bin/xlmake

all: out

clean:
	rm -rf build sysroot resource

.PHONY: all out download stage1 clean
