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

# Рантаймы поверх glibc: compiler-rt -> libc++ -> zlib + expat -> libxcrypt.
#
# libdrm здесь НЕТ: она входит в общий набор сторонних библиотек Linux-таргетов
# (common/libdrm.mk) и приезжает вторым шагом; разделяемые libc++/libc++abi/
# libunwind для образа — третьим (../libc++.mk). См. target-xenolithos/Makefile.
#
# Собираются clang'ом движка (hosts/<host-triple>) как кросс-компилятором против
# sysroot'а, который поднял target.mk. GCC здесь уже не участвует — его роль
# закончилась вместе с glibc.
#
# Здесь же генерируются clang.cmake и meson-cross.ini: их переиспользуют
# vulkan.mk, gpu.mk и userland.mk.

HOST_CLANG      := $(ENGINE_HOST_DIR)/bin/clang
HOST_CLANGXX    := $(ENGINE_HOST_DIR)/bin/clang++
HOST_LLVM_AR    := $(ENGINE_HOST_DIR)/bin/llvm-ar
HOST_LLVM_STRIP := $(ENGINE_HOST_DIR)/bin/llvm-strip
HOST_LLVM_READELF := $(ENGINE_HOST_DIR)/bin/llvm-readelf

# Каталог ресурсов clang движка: оттуда берутся builtin-заголовки. Версия
# определяется по факту, чтобы не ломаться на обновлении LLVM.
HOST_CLANG_RESOURCE := $(firstword $(wildcard $(ENGINE_HOST_DIR)/lib/clang/*))

TARGET_RESOURCE_DIR := $(abspath $(TARGET_SYSROOT))/lib/clang
SYSROOT_ABS         := $(abspath $(TARGET_SYSROOT))

# Командные строки кросс-компилятора для пакетов на autotools и kbuild — у них
# нет понятия toolchain-файла (cmake/meson-пакеты берут то же самое из
# clang.cmake / meson-cross.ini). Переиспользуются в userland.mk.
#
# -stdlib=libc++ не опционален: в sysroot лежат заголовки libc++ в include/c++/v1
# и нет libstdc++ вообще, а clang под Linux по умолчанию берёт libstdc++.
CROSS_CC  = $(HOST_CLANG) --target=$(SP_TARGET_TRIPLE) --sysroot=$(SYSROOT_ABS) \
            -resource-dir $(TARGET_RESOURCE_DIR) -fuse-ld=lld
CROSS_CXX = $(HOST_CLANGXX) --target=$(SP_TARGET_TRIPLE) --sysroot=$(SYSROOT_ABS) \
            -resource-dir $(TARGET_RESOURCE_DIR) -stdlib=libc++ -fuse-ld=lld

# ── проверка окружения ────────────────────────────────────────────────────────
# Всё, что ниже, требует clang движка плюс cmake/ninja/meson/python3. Без этой
# проверки отказ вылезет только ПОСЛЕ многочасового бутстрапа gcc+glibc, потому
# что до этой точки они нигде не используются. Подключается как order-only:
# выполняется всегда, но никогда не помечает актуальную цель устаревшей.
HOST_TOOLS := cmake ninja meson pkg-config python3 patch wget

check-engine:
	@missing=""; for t in $(HOST_TOOLS); do \
		command -v $$t >/dev/null 2>&1 || missing="$$missing $$t"; done; \
	if [ -n "$$missing" ]; then \
		echo "[check] missing host tools:$$missing" >&2; exit 1; fi
	@if [ ! -x "$(HOST_CLANG)" ]; then \
		echo "[check] host clang not found: $(HOST_CLANG)" >&2; \
		echo "[check] build it first: make -C $(TOOLCHAINS_ROOT) host" >&2; \
		exit 1; fi
	@if [ ! -d "$(HOST_CLANG_RESOURCE)/include" ]; then \
		echo "[check] clang resource dir not found under $(ENGINE_HOST_DIR)/lib/clang" >&2; \
		exit 1; fi

.PHONY: check-engine

# ── cmake toolchain для кросс-сборки clang'ом ─────────────────────────────────
TARGET_CMAKE_TOOLCHAIN := $(abspath $(MAKE_ROOT)$(TBUILD)/clang.cmake)

$(TARGET_CMAKE_TOOLCHAIN): $(TARGET_GLIBC) | check-engine
	@mkdir -p $(TBUILD)
	@echo 'set(CMAKE_SYSTEM_NAME Linux)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH_TARGET))' >> $@
	@echo 'set(CMAKE_C_COMPILER "$(abspath $(HOST_CLANG))")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$(abspath $(HOST_CLANGXX))")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_TARGET_TRIPLE)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_TARGET_TRIPLE)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$(abspath $(HOST_CLANG))")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_TARGET_TRIPLE)")' >> $@
	@echo 'set(CMAKE_SYSROOT "$(SYSROOT_ABS)")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$(SYSROOT_ABS)")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@

# ── compiler-rt (builtins) ────────────────────────────────────────────────────
# Обязан идти до libc++: libc++/libc++abi/libunwind линкуются с
# USE_COMPILER_RT=ON и ждут builtins в resource-dir.
#
# После install собирается resource-dir внутри целевого sysroot: builtin-
# заголовки симлинкуются из host-тулчейна, lib/linux — на установленный
# compiler-rt.
TARGET_COMPILERRT := $(TARGET_SYSROOT)/lib/linux/libclang_rt.builtins-$(SP_ARCH_TARGET).a

$(TARGET_COMPILERRT): $(TARGET_CMAKE_TOOLCHAIN) | $(LLVM_SRC_DIR)
	rm -rf $(TBUILD)/compiler-rt
	cmake -DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-G Ninja -S $(LLVM_SRC_DIR)/runtimes -B $(TBUILD)/compiler-rt \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;" \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_HOST_TRIPLE=$(SP_TARGET_TRIPLE) \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_CRT=On \
		-DCOMPILER_RT_BUILD_PROFILE=On \
		-DCOMPILER_RT_BUILD_ORC=Off \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_TRIPLE=$(SP_TARGET_TRIPLE) \
		-DCMAKE_INSTALL_PREFIX=$(SYSROOT_ABS)
	cmake --build $(TBUILD)/compiler-rt $(SP_NJOBS)
	cmake --install $(TBUILD)/compiler-rt
	mkdir -p $(TARGET_RESOURCE_DIR)/lib
	ln -sfn $(SYSROOT_ABS)/lib/linux $(TARGET_RESOURCE_DIR)/lib/linux
	ln -sfn $(abspath $(HOST_CLANG_RESOURCE))/include $(TARGET_RESOURCE_DIR)/include

# ── libc++ / libc++abi / libunwind ────────────────────────────────────────────
# Статические + разделяемые. -resource-dir указывает clang'у на только что
# собранный compiler-rt, поэтому USE_COMPILER_RT=ON разрешает builtins.
#
# Это СБОРОЧНЫЙ рантайм sysroot'а: против него линкуются vulkaninfo и шим
# libgcc_s. В образ едет не он, а сборка из ../libc++.mk (третий шаг сборки
# таргета) — SONAME и ABI те же.
#
# STATICALLY_LINK_UNWINDER_IN_SHARED_LIBRARY=Off по той же причине, что и там:
# иначе libc++abi.so.1 несёт разматыватель ВНУТРИ, и рядом с libunwind.so.1, на
# которую ссылается шим libgcc_s.so.1, в процессе оказывается вторая копия
# разматывателя с раздельным состоянием.
TARGET_LIBCXX := $(TARGET_SYSROOT)/lib/libc++.a

$(TARGET_LIBCXX): $(TARGET_COMPILERRT) | $(LLVM_SRC_DIR)
	rm -rf $(TBUILD)/libcxx
	cmake -DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-G Ninja -S $(LLVM_SRC_DIR)/runtimes -B $(TBUILD)/libcxx \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLLVM_HOST_TRIPLE=$(SP_TARGET_TRIPLE) \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_CXX_ABI=libcxxabi \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_STATICALLY_LINK_UNWINDER_IN_SHARED_LIBRARY=Off \
		-DCMAKE_INSTALL_PREFIX=$(SYSROOT_ABS) \
		-DCMAKE_C_FLAGS_INIT="-resource-dir $(TARGET_RESOURCE_DIR) -fuse-ld=lld" \
		-DCMAKE_CXX_FLAGS_INIT="-resource-dir $(TARGET_RESOURCE_DIR) -fuse-ld=lld"
	cmake --build $(TBUILD)/libcxx $(SP_NJOBS)
	cmake --install $(TBUILD)/libcxx

# ── libgcc_s.so.1 (шим для рантайма glibc) ────────────────────────────────────
# glibc грузит libgcc_s.so.1 через dlopen и достаёт из неё шесть символов
# (misc/unwind-link.c): _Unwind_{Backtrace,ForcedUnwind,GetCFA,GetIP,Resume} и
# __gcc_personality_v0. Без неё падают pthread_exit, pthread_cancel, backtrace()
# и любое исключение C++, разматывающееся через кадр libc с -fexceptions.
#
# Отключить это на стороне glibc нельзя: конфигурационного ключа нет, а имя
# библиотеки генерируется из shlib-versions (libgcc_s=1) в LIBGCC_S_SO, где
# настраивается только цифра версии. Прямой путь без dlopen есть лишь в
# нешаренной сборке libc.a, а рантайм устройства — на разделяемой glibc.
#
# Пять из шести символов даёт LLVM-ная libunwind, поэтому вместо второго прохода
# gcc собирается шим на ~5 КБ: единственный __gcc_personality_v0 плюс DT_NEEDED
# на уже собранную libunwind.so.1 (dlsym по хендлу ищет и в зависимостях).
# Копировать libunwind внутрь через --whole-archive НЕЛЬЗЯ: в процессе окажутся
# две копии разматывателя с раздельным состоянием — ровно то cross-talk'ом
# кончающееся сегфолтом, о котором предупреждает документация llvm-libgcc.
#
# Готовый gcc_personality_v0.c.o из libclang_rt.builtins.a не подходит: он
# GLOBAL HIDDEN (compiler-rt собран с -fvisibility=hidden) и в динамическую
# таблицу не попадает, поэтому файл пересобирается с -fvisibility=default. Сам
# архив builtins всё равно нужен вторым аргументом — из него тянется
# __compilerrt_abort_impl, без которого dlopen падает, а glibc наружу показывает
# всё то же "libgcc_s.so.1 must be installed".
#
# В sysroot НЕ ставится: там линковка идёт на compiler-rt, и наличие libgcc_s
# рядом дало бы clang'у шанс подобрать её вместо builtins. Место шима — только
# rootfs устройства, куда его забирает mkrootfs.sh.
TARGET_LIBGCC_S := $(abspath $(MAKE_ROOT)$(TBUILD))/libgcc_s/libgcc_s.so.1

GCC_PERSONALITY_SRC := $(LLVM_SRC_DIR)/compiler-rt/lib/builtins/gcc_personality_v0.c

$(TARGET_LIBGCC_S): $(TARGET_LIBCXX) $(TARGET_COMPILERRT)
	@mkdir -p $(dir $@)
	$(CROSS_CC) -fPIC -fvisibility=default -O2 -c $(GCC_PERSONALITY_SRC) \
		-o $(dir $@)gcc_personality_v0.o
	$(CROSS_CC) -shared -nostdlib -Wl,-soname,libgcc_s.so.1 -o $@ \
		$(dir $@)gcc_personality_v0.o $(TARGET_COMPILERRT) \
		-L$(SYSROOT_ABS)/lib -lunwind -lc

# ── zlib (cmake, статическая) ─────────────────────────────────────────────────
# Нужна mesa ещё до того, как заработает общая сборка сторонних библиотек
# движка (та идёт поверх готового sysroot).
TARGET_ZLIB := $(TARGET_SYSROOT)/usr/lib/libz.a

$(TARGET_ZLIB): $(TARGET_CMAKE_TOOLCHAIN) | $(ZLIB_SRC_DIR)
	rm -rf $(TBUILD)/zlib
	mkdir -p $(TBUILD)/zlib
	cd $(TBUILD)/zlib; cmake -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(SYSROOT_ABS)/usr \
		-DZLIB_BUILD_EXAMPLES=OFF \
		-DZLIB_BUILD_TESTING=OFF \
		-DZLIB_BUILD_SHARED=OFF \
		-DZLIB_BUILD_STATIC=ON \
		$(ZLIB_SRC_DIR)
	cmake --build $(TBUILD)/zlib $(SP_NJOBS)
	cmake --install $(TBUILD)/zlib

# ── expat (cmake, статическая) ────────────────────────────────────────────────
TARGET_EXPAT := $(TARGET_SYSROOT)/usr/lib/libexpat.a

$(TARGET_EXPAT): $(TARGET_CMAKE_TOOLCHAIN) | $(EXPAT_SRC_DIR)
	rm -rf $(TBUILD)/expat
	mkdir -p $(TBUILD)/expat
	cd $(TBUILD)/expat; cmake -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(SYSROOT_ABS)/usr \
		-DEXPAT_BUILD_TESTS=OFF \
		-DEXPAT_BUILD_EXAMPLES=OFF \
		-DEXPAT_BUILD_TOOLS=OFF \
		-DBUILD_SHARED_LIBS=OFF \
		$(EXPAT_SRC_DIR)
	cmake --build $(TBUILD)/expat $(SP_NJOBS)
	cmake --install $(TBUILD)/expat

# ── libxcrypt (autotools, разделяемая) ────────────────────────────────────────
# Даёт crypt(3). glibc 2.39 больше не собирает libcrypt, а отладочный SSH-путь
# (root против /etc/shadow) требует от dropbear проверки SHA-512-хешей.
#
# --enable-obsolete-api сохраняет glibc-совместимый SONAME libcrypt.so.1; без
# него libxcrypt ставит libcrypt.so.2 и разводит ABI с дистрибутивом, на который
# этот sysroot равняется.
TARGET_LIBXCRYPT := $(TARGET_SYSROOT)/usr/lib/libcrypt.so.1

$(TARGET_LIBXCRYPT): $(TARGET_GLIBC) $(LIBXCRYPT_SRC_DIR) | check-engine
	rm -rf $(TBUILD)/libxcrypt
	mkdir -p $(TBUILD)/libxcrypt
	cd $(TBUILD)/libxcrypt; PATH=$(TARGET_PATH):$$PATH \
		$(abspath $(LIBXCRYPT_SRC_DIR))/configure \
		--host=$(SP_TARGET_TRIPLE) \
		--prefix=/usr --libdir=/usr/lib \
		--disable-static --enable-shared \
		--enable-obsolete-api \
		CC="$(CROSS_CC)"
	PATH=$(TARGET_PATH):$$PATH make -C $(TBUILD)/libxcrypt $(SP_NJOBS)
	PATH=$(TARGET_PATH):$$PATH make -C $(TBUILD)/libxcrypt install DESTDIR=$(SYSROOT_ABS)

libs: $(TARGET_COMPILERRT) $(TARGET_LIBCXX) $(TARGET_LIBGCC_S) \
      $(TARGET_ZLIB) $(TARGET_EXPAT) $(TARGET_LIBXCRYPT)

.PHONY: libs
