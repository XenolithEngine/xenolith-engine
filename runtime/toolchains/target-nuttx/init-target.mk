# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# (same MIT license as the rest of the toolchain trees)

# Generate the per-arch NuttX sysroot glue: a toolchain.cmake for building the
# LLVM runtimes against NuttX's libc, the app-facing target.mk descriptor, and
# the symlink to the shared host toolchain (clang/lld/llvm-ar).
#
# This is the NuttX-hosted counterpart of target-wasm/init-target.mk: instead
# of freestanding + sprt libc, we point clang at the NuttX headers/libs that
# import-export.mk just laid out under sysroot/usr. NuttX defines __NuttX__
# at build time, which our runtime detects (runtime/include/sprt/c/bits/__sprt_def.h).

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

OUT := $(TOOLCHAIN_OUTPUT_DIR)
SYSROOT := $(OUT)/sysroot
ARCH_FLAGS_FILE := $(OUT)/nuttx-arch-flags.mk
CONFIG_FILE := $(OUT)/nuttx-config.mk

# Pull the per-board flags extracted by import-export.mk.
-include $(ARCH_FLAGS_FILE)
-include $(CONFIG_FILE)

# Clang --target triple (no "nuttx" OS — LLVM does not know it). The arch flags
# (-mcpu/-march/-mfpu/-mabi for the specific board) ride on top of this triple.
NUTTX_TARGET := $(SP_ARCH_TARGET_CLANG)

# Build a single combined CFLAGS string for the toolchain file:
#   --target=<baremetal triple>  +  NuttX's per-board -mcpu/-march/... flags
#   -isystem <sysroot>/usr/include  so the NuttX libc/pthread headers resolve
#   -D__NuttX__                    the runtime's platform-detector predicate
NUTTX_CFLAGS := --target=$(NUTTX_TARGET) \
	$(NUTTX_ARCHCPUFLAGS) $(NUTTX_ARCHCFLAGS) \
	-isystem $(SYSROOT)/usr/include \
	-D__NuttX__

NUTTX_CXXFLAGS := --target=$(NUTTX_TARGET) \
	$(NUTTX_ARCHCPUFLAGS) $(NUTTX_ARCHCXXFLAGS) \
	-isystem $(SYSROOT)/usr/include \
	-D__NuttX__

# Linker flags for runtime try-compiles: point at the NuttX libs + startup
# objects and bring in libc/libm/libpthread so feature probes link cleanly.
NUTTX_LDFLAGS := --target=$(NUTTX_TARGET) \
	-L$(SYSROOT)/usr/lib \
	$(NUTTX_LDELFFLAGS) \
	-lc -lm

# --- toolchain.cmake ------------------------------------------------------
# CMAKE_SYSTEM_NAME Generic (baremetal): LLVM sets LLVM_ON_UNIX=0, which pairs
# with the COMPILER_RT_BAREMETAL_BUILD / *_BAREMETAL flags the runtimes need.
# NuttX's libc is a hosted libc, but it is reached via -isystem + -lc, not via
# CMAKE_SYSTEM_NAME=Linux (which would assume glibc + dynamic linker machinery
# NuttX does not have).
$(OUT)/toolchain.cmake: $(lastword $(MAKEFILE_LIST)) $(ARCH_FLAGS_FILE) $(CONFIG_FILE)
	@echo 'Build $@'
	@echo '# NuttX hosted-POSIX target. Generic (baremetal) drives LLVM_ON_UNIX=0,' > $@
	@echo '# which pairs with COMPILER_RT_BAREMETAL_BUILD / *_BAREMETAL in the runtimes.' >> $@
	@echo '# The libc is NuttX own libc, supplied via -isystem + -lc, not via the system.' >> $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
	@echo '# Static archives only on NuttX (flat build); keep the shared-lib suffix' >> $@
	@echo '# distinct so the never-built shared runtime targets do not collide.' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_PREFIX "lib")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang++")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_AR "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ar")' >> $@
	@echo 'set(CMAKE_RANLIB "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ranlib")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(NUTTX_TARGET)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(NUTTX_TARGET)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(NUTTX_TARGET)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_WORKS ON)' >> $@
	@echo '# No standalone executable until the NuttX image link; feature probes build archives.' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(NUTTX_CFLAGS)")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(NUTTX_CXXFLAGS)")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$(NUTTX_CFLAGS)")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(NUTTX_LDFLAGS)")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(NUTTX_LDFLAGS)")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR}/sysroot;$${CMAKE_CURRENT_LIST_DIR}/sysroot/usr")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@
	rm -f $(OUT)/host
	cd $(OUT); ln -fs ../../../hosts/$(HOST_ID) host
	# resource-dir symlink to the host clang resource dir: <stdarg.h>, <stddef.h>,
	# <arm_neon.h>, etc. live there. Before M2 there are no builtins; M2 will
	# drop libclang_rt.builtins.a under lib/clang/lib/<arch>/ (or replace the
	# symlink with a real directory carrying builtins + the host include/).
	rm -rf $(OUT)/lib
	mkdir -p $(OUT)/lib
	ln -fs ../../host/lib/clang/21 $(OUT)/lib/clang

# --- app-facing target.mk -------------------------------------------------
# The descriptor the rest of the make-system consumes (TARGET_SYSROOT /
# TARGET_SYSTEM=NuttX triggers make/os/nuttx.mk via apply-toolchain.mk).
# -resource-dir resolves the compiler-rt builtins once M2 ships them; before
# that it points at the host's (only builtins are looked up there).
$(OUT)/target.mk: $(lastword $(MAKEFILE_LIST)) $(ARCH_FLAGS_FILE) $(CONFIG_FILE)
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := NuttX' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(NUTTX_TARGET)' >> $@
	@echo '# NuttX per-board arch flags (extracted from the export Make.defs):' >> $@
	@echo 'TARGET_NUTTX_ARCHCPUFLAGS := $(NUTTX_ARCHCPUFLAGS)' >> $@
	@echo 'TARGET_NUTTX_ARCHCFLAGS := $(NUTTX_ARCHCFLAGS)' >> $@
	@echo 'TARGET_NUTTX_ARCHCXXFLAGS := $(NUTTX_ARCHCXXFLAGS)' >> $@
	@echo 'TARGET_NUTTX_CROSSDEV := $(NUTTX_CROSSDEV)' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := --target=$(NUTTX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_NUTTX_ARCHCPUFLAGS) $$(TARGET_NUTTX_ARCHCFLAGS) -D__NuttX__' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := --target=$(NUTTX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_NUTTX_ARCHCPUFLAGS) $$(TARGET_NUTTX_ARCHCXXFLAGS) -D__NuttX__' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := --target=$(NUTTX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang -L$$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@
	@echo '# NuttX libc lives under sysroot/usr/include; expose it via TARGET_INCLUDE_DIR_LIBC' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/usr/lib' >> $@

all: $(OUT)/toolchain.cmake $(OUT)/target.mk

.PHONY: all
