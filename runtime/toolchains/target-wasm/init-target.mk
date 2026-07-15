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

# Generate the per-arch wasm sysroot skeleton: the host symlink, a cmake
# toolchain file for building the runtimes freestanding against the sprt headers,
# and the app-facing target.mk descriptor.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

WASM_FEATURES := -matomics -mbulk-memory -mmutable-globals -msign-ext -mnontrapping-fptoint

WASM_C_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include

WASM_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include

# compiler-rt (C/ASM) stays freestanding/baremetal. The C++ runtimes (libc++abi)
# are compiled HOSTED — no -ffreestanding — so __STDC_HOSTED__==1 and the sprt STL
# exposes the C library under std:: (std::malloc/free/abort/..., needed by the
# demangler and fallback allocator). The runtime provides a real libc, so hosted
# is the correct mode here.
WASM_C_FLAGS := -nostdinc -ffreestanding $(WASM_FEATURES) $(WASM_C_INCLUDES) -D__SPRT_WASM
# -Wno-deprecated-declarations: sprt's <new> marks the plain global operator
# new/delete deprecated (it wants new(sprt::nothrow)/sprt::__delete), but the
# vendored libc++/libc++abi code legitimately uses them and builds with -Werror.
# No -fno-exceptions: libc++abi IS the exception runtime (__cxa_throw / personality
# / catch machinery), so it must be compiled with exceptions enabled.
WASM_CXX_FLAGS := -nostdinc -nostdinc++ $(WASM_FEATURES) $(WASM_CXX_INCLUDES) -D__SPRT_WASM -D__SPRT_USE_STL=0 -Wno-deprecated-declarations

$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo '# "Generic" = baremetal: LLVM sets LLVM_ON_UNIX=0/LLVM_ON_WIN32=0 for it' >> $@
	@echo '# (WASI is not recognised by HandleLLVMOptions), which pairs with the' >> $@
	@echo '# COMPILER_RT_BAREMETAL_BUILD / LIBUNWIND_IS_BAREMETAL / *_BAREMETAL flags.' >> $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR wasm32)' >> $@
	@echo '# On Generic the shared-lib suffix defaults to the static "\.a", so the' >> $@
	@echo '# always-defined (but EXCLUDE_FROM_ALL) unwind_shared/c++_shared targets' >> $@
	@echo '# collide with the static ones ("multiple rules generate libunwind.a").' >> $@
	@echo '# Give shared libs a distinct suffix; they are never actually built.' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_PREFIX "lib")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang++")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_AR "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ar")' >> $@
	@echo 'set(CMAKE_RANLIB "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ranlib")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_WORKS ON)' >> $@
	@echo '# wasm cannot link an executable during try_compile (no crt/libc yet).' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(WASM_C_FLAGS)")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(WASM_CXX_FLAGS)")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$(WASM_C_FLAGS)")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib $(WASM_FEATURES)")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "-nostdlib $(WASM_FEATURES)")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@
	rm -f $(TOOLCHAIN_OUTPUT_DIR)/host
	cd $(TOOLCHAIN_OUTPUT_DIR); ln -fs ../../../hosts/$(HOST_ID) host
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/21/include include

# Second cmake toolchain file, used to cross-build the third-party dependency libs
# (zlib/png/freetype/harfbuzz/...) — NOT the LLVM runtimes (those use toolchain.cmake
# above, which bakes the freestanding flags in because compiler_rt.mk drives cmake
# directly). The deps go through common/configure.mk, which computes the wasm flags
# and hands them in via -DSP_C_FLAGS/-DSP_CXX_FLAGS/... ; this file consumes them and
# tacks on -resource-dir + --target, exactly like target-windows/target-linux. Kept
# separate so the verified runtimes build is never perturbed.
TOOLCHAIN_LIB_CFLAGS := -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)

$(TOOLCHAIN_OUTPUT_DIR)/toolchain-libs.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo '# Generic = baremetal (LLVM_ON_UNIX/WIN32 = 0), matching the runtimes toolchain.' > $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR wasm32)' >> $@
	@echo '# On Generic the shared-lib suffix defaults to the static ".a"; give shared' >> $@
	@echo '# libs a distinct suffix so the (never-built) shared targets some deps always' >> $@
	@echo '# define do not collide with their static archives.' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_PREFIX "lib")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang++")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_AR "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ar")' >> $@
	@echo 'set(CMAKE_RANLIB "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ranlib")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_WORKS ON)' >> $@
	@echo '# wasm cannot link an executable during try_compile / feature probes (no crt).' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} $(TOOLCHAIN_LIB_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS} $(TOOLCHAIN_LIB_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$${SP_C_FLAGS} $(TOOLCHAIN_LIB_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS} $(TOOLCHAIN_LIB_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS} $(TOOLCHAIN_LIB_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH Off)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(PKG_CONFIG_PATH "$${CMAKE_CURRENT_LIST_DIR}/usr/lib/pkgconfig")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_PREFIX_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(CMAKE_INSTALL_PREFIX "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_INSTALL_LIBDIR "$${CMAKE_CURRENT_LIST_DIR}/usr/lib")' >> $@
	@echo 'set(CMAKE_INSTALL_INCLUDEDIR "$${CMAKE_CURRENT_LIST_DIR}/usr/include")' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@

# App-facing descriptor (mirrors runtime/toolchains/targets/wasm32-unknown-unknown/
# target.mk but points -resource-dir at the built compiler-rt in this sysroot).
$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := WASM' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)' >> $@
	@echo 'TARGET_WASM_FEATURES := $(WASM_FEATURES)' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -nostdinc -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_WASM_FEATURES)' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -nostdinc -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_WASM_FEATURES)' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_WASM_FEATURES) -nostdlib' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@

# simde (SIMD-everywhere) is a header-only dependency the geom SIMD headers need.
# Build+install it through its own cmake into this sysroot's usr/include/simde
# (header-only, so no target compilation — CMAKE_*_COMPILER_WORKS skips the probe),
# exactly like the other targets rather than raw-copying the headers.
SIMDE_CONFIGURE := \
	-DCMAKE_INSTALL_PREFIX="$(TOOLCHAIN_OUTPUT_DIR)" \
	-DCMAKE_INSTALL_LIBDIR="$(TOOLCHAIN_OUTPUT_DIR)/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="$(TOOLCHAIN_OUTPUT_DIR)/usr/include" \
	-DCMAKE_C_COMPILER_WORKS=1 \
	-DCMAKE_CXX_COMPILER_WORKS=1

$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(call rule_rm,simde)
	$(call rule_mkdir,simde)
	cd simde; cmake -G "Ninja" $(LIB_SRC_DIR)/simde $(SIMDE_CONFIGURE)
	cd simde; cmake --build .
	cd simde; cmake --install .
	$(call rule_rm,simde)
	$(call rule_touch,$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h)

all: $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake $(TOOLCHAIN_OUTPUT_DIR)/toolchain-libs.cmake \
	$(TOOLCHAIN_OUTPUT_DIR)/target.mk \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h

.PHONY: all
