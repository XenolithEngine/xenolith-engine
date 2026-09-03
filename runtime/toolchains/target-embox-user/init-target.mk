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
# Generate the Embox EL0 sysroot skeleton: the host symlink, cmake toolchain
# files for cross-building the LLVM runtimes and the third-party deps against the
# sprt headers, the app-facing target.mk descriptor, and the app linker script.
#
# Nothing is imported. Unlike target-embox, which needs an `embox export` package
# for the kernel's libc and per-board arch flags, this target's sysroot is made
# entirely of our own headers and libraries (decision D6) -- which is also why it
# has no import-sysroot.mk and why one link works on every board.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk
include $(dir $(THIS_FILE))../common/utils/llvm-version.mk

# One binary for every board. -march=armv8-a with NO -mtune: QEMU virt is a
# Cortex-A53 and the Pi 4 an A72, and the whole point of the fixed image base
# (D3) is that the same app.elf runs on both. Nothing here may be board-specific.
#
# No -mstrict-align either, unlike the kernel's own flags: that is a property of
# how Embox maps its own memory early in boot, and EL0 pages are ordinary
# cacheable Normal memory where unaligned access works.
EMBOX_USER_ARCH_FLAGS := -march=armv8-a

EMBOX_USER_C_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include

EMBOX_USER_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include

# compiler-rt (C/ASM) stays freestanding. The C++ runtimes (libc++abi) compile
# HOSTED -- no -ffreestanding -- so __STDC_HOSTED__ is 1 and the sprt STL exposes
# the C library under std:: (std::malloc/abort/..., which the demangler and the
# fallback allocator need). We do provide a real libc, so hosted is correct.
# Same split as target-wasm.
EMBOX_USER_C_FLAGS := -nostdinc -ffreestanding $(EMBOX_USER_ARCH_FLAGS) \
	$(EMBOX_USER_C_INCLUDES) -D__EMBOX_USER__
# -Wno-deprecated-declarations: sprt's <new> deprecates the plain global
# operator new/delete, and the vendored libc++abi legitimately uses them while
# building with -Werror. No -fno-exceptions: libc++abi IS the exception runtime.
EMBOX_USER_CXX_FLAGS := -nostdinc -nostdinc++ $(EMBOX_USER_ARCH_FLAGS) \
	$(EMBOX_USER_CXX_INCLUDES) -D__EMBOX_USER__ -D__SPRT_USE_STL=0 \
	-Wno-deprecated-declarations

$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo '# Embox EL0, freestanding. "Generic" = baremetal: LLVM sets' > $@
	@echo '# LLVM_ON_UNIX=0, which pairs with COMPILER_RT_BAREMETAL_BUILD and the' >> $@
	@echo '# *_BAREMETAL flags in compiler_rt.mk. The libc is ours, reached through' >> $@
	@echo '# -isystem on the runtime include trees, not through a sysroot.' >> $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
	@echo '# On Generic the shared-lib suffix defaults to ".a", so the' >> $@
	@echo '# always-defined (but EXCLUDE_FROM_ALL) unwind_shared/cxxabi_shared' >> $@
	@echo '# targets collide with the static ones. Give them a distinct suffix;' >> $@
	@echo '# they are never actually built.' >> $@
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
	@echo '# The libc being built is not linkable yet during the probes, and the' >> $@
	@echo '# entry point comes from libc_impl which is not in this build.' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(EMBOX_USER_C_FLAGS)")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(EMBOX_USER_CXX_FLAGS)")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$(EMBOX_USER_C_FLAGS)")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "-nostdlib")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@
	rm -f $(TOOLCHAIN_OUTPUT_DIR)/host
	cd $(TOOLCHAIN_OUTPUT_DIR); ln -fs ../../../hosts/$(HOST_ID) host
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/$(SP_LLVM_VER)/include include

# Second toolchain file for the third-party deps (zlib/png/freetype/...). They go
# through common/configure.mk, which fills SP_C_FLAGS per dep; baking the
# runtimes' -Werror flags into their build (as toolchain.cmake does) would fail
# on upstream warnings. Mirrors target-wasm.
$(TOOLCHAIN_OUTPUT_DIR)/toolchain-libs.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' > $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
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
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS}" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS}" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$${SP_C_FLAGS}" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS}" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS}" CACHE STRING "" FORCE)' >> $@
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

# App-facing descriptor. TARGET_SYSTEM selects make/os/embox-user.mk, which owns
# the freestanding flags and the fixed-address link; what is left here is where
# this sysroot put things.
$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := EmboxUser' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)' >> $@
	@echo '# -D__EMBOX_USER__ is what makes the runtime resolve to this platform' >> $@
	@echo '# (include/sprt/c/bits/__sprt_def.h). Unlike wasm, where the compiler' >> $@
	@echo '# predefines __wasm__, LLVM knows nothing about Embox -- so nothing but' >> $@
	@echo '# this define distinguishes the target, and without it every cross' >> $@
	@echo '# header fails to resolve.' >> $@
	@echo '# -nostdinc lives here rather than in the OS preset, matching wasm and' >> $@
	@echo '# Windows: it says where this targets headers are, which is a property' >> $@
	@echo '# of the sysroot. -ffreestanding is deliberately absent -- see the note' >> $@
	@echo '# in make/os/embox-user.mk.' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := --target=$(SP_ARCH_TARGET_CLANG) -nostdinc -resource-dir $$(TARGET_SYSROOT)/lib/clang $(EMBOX_USER_ARCH_FLAGS) -D__EMBOX_USER__' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := --target=$(SP_ARCH_TARGET_CLANG) -nostdinc -resource-dir $$(TARGET_SYSROOT)/lib/clang $(EMBOX_USER_ARCH_FLAGS) -D__EMBOX_USER__' >> $@
	@echo '# Assembly needs the same target identity as C: without it a .S file' >> $@
	@echo '# sees only --target=, so anything it conditions on the platform is' >> $@
	@echo '# silently compiled for nobody (musl-adapters/string/musl_string_aarch64.S).' >> $@
	@echo 'TARGET_GENERAL_SFLAGS := --target=$(SP_ARCH_TARGET_CLANG) -resource-dir $$(TARGET_SYSROOT)/lib/clang $(EMBOX_USER_ARCH_FLAGS) -D__EMBOX_USER__' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := --target=$(SP_ARCH_TARGET_CLANG) -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@

# The app linker script goes in the sysroot because that is where the app's link
# line looks for it (make/os/embox-user.mk: -T $(TARGET_SYSROOT)/share/...).
$(TOOLCHAIN_OUTPUT_DIR)/share/app-$(SP_ARCH).lds: $(MAKE_ROOT)app-$(SP_ARCH).lds
	@mkdir -p $(dir $@)
	cp -f $< $@

# simde (SIMD-everywhere), header-only, needed by the geom SIMD headers. Built
# and installed through its own cmake like every other target rather than
# raw-copied, so its own install layout stays authoritative.
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
	$(TOOLCHAIN_OUTPUT_DIR)/share/app-$(SP_ARCH).lds \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h

.PHONY: all
