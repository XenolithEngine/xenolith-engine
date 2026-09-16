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

THIS_FILE := $(lastword $(MAKEFILE_LIST))

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/find-recursive.mk
include $(dir $(THIS_FILE))../common/utils/names.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk
include $(dir $(THIS_FILE))../common/utils/llvm-version.mk

TOOLCHAIN_CFLAGS :=  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)

$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake: $(lastword $(MAKEFILE_LIST))
	@echo 'set(CMAKE_SYSTEM_NAME Windows)' > $@
	@echo 'set(CMAKE_C_SIMULATE_ID MSVC)' >> $@
	@echo 'set(CMAKE_CXX_SIMULATE_ID MSVC)' >> $@
	@echo 'set(CMAKE_MSVC_RUNTIME_LIBRARY "Sprt")' >> $@
	@echo 'set(CMAKE_C_COMPILE_OPTIONS_MSVC_RUNTIME_LIBRARY_Sprt -Xclang --dependent-lib=sprt)' >> $@
	@echo 'set(CMAKE_CXX_COMPILE_OPTIONS_MSVC_RUNTIME_LIBRARY_Sprt -Xclang --dependent-lib=sprt -std=gnu++20)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)")' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS} $(TOOLCHAIN_CFLAGS)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
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
	@echo 'set(CMAKE_C_STANDARD_LIBRARIES "-L$${CMAKE_CURRENT_LIST_DIR}/usr/lib -lsprt" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_STANDARD_LIBRARIES "-L$${CMAKE_CURRENT_LIST_DIR}/usr/lib -lsprt" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@
	rm -f $(TOOLCHAIN_OUTPUT_DIR)/host
	cd $(TOOLCHAIN_OUTPUT_DIR); ln -fs ../../../hosts/$(HOST_ID) host
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/$(SP_LLVM_VER)/include include

# -fexceptions is required on Windows even though the SDK never throws: the freestanding libc
# implements longjmp on top of SEH unwinding (see libc_impl/src/windows/except.cc), and the
# destructors of the frames being unwound are run from the MSVC C++ EH cleanup funclets. Both
# those funclets and the FuncInfo tables that index them are emitted ONLY for translation units
# compiled with C++ exception support.
#
# Do NOT add -fno-cxx-exceptions here. It looks like the natural counterpart of the
# -fno-exceptions used on every other platform, but on Windows it strips exactly the tables and
# funclets that stack unwinding and destructor calls depend on. "No C++ exceptions" is a rule
# for the code in this SDK, not something the compiler can enforce here without taking the
# unwind machinery down with it.
TOOLCHAIN_TARGET_FLAGS := -D_WIN32_WINNT=0x0A00 -fexceptions -fms-compatibility-version=19.40

$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Windows' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang $(TOOLCHAIN_TARGET_FLAGS) -nostdinc' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang $(TOOLCHAIN_TARGET_FLAGS) -nostdinc' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang $(TOOLCHAIN_TARGET_FLAGS) -nostdlib' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@

#
# We need SIMDE to build sprt static library
#
SIMDE_CONFIGURE := \
	-DCMAKE_INSTALL_PREFIX="${TOOLCHAIN_OUTPUT_DIR}" \
	-DCMAKE_INSTALL_LIBDIR="${TOOLCHAIN_OUTPUT_DIR}/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="${TOOLCHAIN_OUTPUT_DIR}/usr/include" \
	-DPKG_CONFIG_PATH="$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/pkgconfig" \
	-DCMAKE_C_COMPILER_WORKS=1 \
	-DCMAKE_CXX_COMPILER_WORKS=1

$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(call rule_rm,simde)
	$(call rule_mkdir,simde)
	cd simde; cmake -G "Ninja" $(LIB_SRC_DIR)/simde $(SIMDE_CONFIGURE);
	cd simde; cmake --build .
	cd simde; cmake --install .
	$(call rule_rm,simde)
	$(call rule_touch,$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h)

RUNTIME_IMPORT_DEFS := $(wildcard $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows/def/*.def)
RUNTIME_IMPORT_LIBS := $(addprefix $(TOOLCHAIN_OUTPUT_DIR)/lib/,$(notdir $(RUNTIME_IMPORT_DEFS:.def=.lib)))

# llvm-lib is reached through the `host` symlink, which the toolchain.cmake rule creates as
# a side effect. `all` lists the two as siblings, so with -j they race, and from an empty
# intermediate directory this rule usually wins: every import lib then fails with
# "host/bin/llvm-lib: No such file or directory". Order-only, because the symlink is
# recreated on every toolchain.cmake rebuild and would otherwise redo all of them.
$(TOOLCHAIN_OUTPUT_DIR)/lib/%.lib : $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows/def/%.def \
		| $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake
	$(call rule_rm,$@)
	$(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-lib /def:$< /out:$@ /machine:$(SP_ARCH_WIN)

# By default usr/lib/sprt.lib is the static runtime archive merged with the Win32 import
# libraries, and every image built against this sysroot carries its own copy of the runtime.
# That is what a target sysroot is expected to hand out: the result is self-contained, and
# the archive keeps the whole libc surface linkable by plain name, which link-time feature
# probes (check_function_exists and its autotools equivalents) depend on.
#
# SPRT_TOOLCHAIN_SHARED=1 opts into the other arrangement: the runtime is built as sprt.dll,
# sprt.lib becomes its import library (already carrying the executable-side startup stub as
# an archive member, see <root>/runtime/Makefile), and the DLL is staged into usr/bin so it
# can travel with whatever links against it. Consumers must then be compiled with
# -DSPRT_SHARED_RUNTIME, and only the annotated surface plus SPRT_ABI_EXPORTS is reachable -
# the header-inline libc umbrellas resolve to __sprt_* imports and their plain names are no
# longer symbols at all.
SPRT_TOOLCHAIN_SHARED ?= 0

# Keyed on the sysroot directory, so the static and the "+dll" variant never share it: the
# tree is rule_rm'd before and after every run, and a `make -j target-windows
# target-windows-dll` would otherwise have one variant delete the other's build in flight.
SPRT_RUNTIME_BUILD_DIR := $(abspath build/$(notdir $(TOOLCHAIN_OUTPUT_DIR)))
SPRT_RUNTIME_OUT := $(SPRT_RUNTIME_BUILD_DIR)/$(SP_ARCH_TARGET_CLANG)/release/cc

# Merge import libs with SPRT for dependencies build.
#
# import.lib is a real prerequisite, not just a sibling in `all`: the runtime module links
# -limport (runtime.mk), so building sprt here fails outright if it is missing.
$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/sprt.lib: $(RUNTIME_IMPORT_LIBS) \
		$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/import.lib \
		$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h \
		$(TOOLCHAIN_OUTPUT_DIR)/target.mk
	$(call rule_rm,$@)
	$(call rule_rm,$(SPRT_RUNTIME_BUILD_DIR))
	$(MAKE) -j8 -C $(SP_RUNTIME_ROOT) \
		STAPPLER_HOST_FILE=$(TOOLCHAIN_OUTPUT_DIR)/host/host.mk \
		STAPPLER_TARGET_FILE=$(TOOLCHAIN_OUTPUT_DIR)/target.mk \
		STAPPLER_TARGET=$(SP_ARCH_TARGET_CLANG) \
		LOCAL_OUTDIR=$(SPRT_RUNTIME_BUILD_DIR) \
		SPRT_SHARED=$(SPRT_TOOLCHAIN_SHARED) \
		RELEASE=1
	$(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-lib /out:$@ \
			$(RUNTIME_IMPORT_LIBS) \
			$(SPRT_RUNTIME_OUT)/sprt.lib \
			/machine:$(SP_ARCH_WIN)
# Every image linked against the library above now imports sprt.dll, so the DLL has to
# travel with the toolchain. Stage it in the intermediate sysroot; the cross Makefile
# picks it up from there when it assembles the released bin/ directory.
ifeq ($(SPRT_TOOLCHAIN_SHARED),1)
	$(call rule_mkdir,$(TOOLCHAIN_OUTPUT_DIR)/usr/bin)
	$(call rule_cp,$(SPRT_RUNTIME_OUT)/sprt.dll,$(TOOLCHAIN_OUTPUT_DIR)/usr/bin)
endif
	$(call rule_rm,$(SPRT_RUNTIME_BUILD_DIR))

$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/import.lib: $(RUNTIME_IMPORT_LIBS)
	$(call rule_rm,$@)
	$(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-lib /out:$@ \
			$(RUNTIME_IMPORT_LIBS) \
			/machine:$(SP_ARCH_WIN)

all: $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	$(TOOLCHAIN_OUTPUT_DIR)/target.mk \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/sprt.lib \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/import.lib \
	$(RUNTIME_IMPORT_LIBS_USR)

# Just the part compiler-rt needs before it can be cross-built: the toolchain file (whose
# recipe also plants the `host` symlink) and the Win32 import libraries it links against.
# sprt cannot be in this phase - linking sprt.dll pulls in clang_rt.builtins, so it has to
# come after compiler-rt, which is what the arch recipes in Makefile sequence.
toolchain: $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	$(TOOLCHAIN_OUTPUT_DIR)/target.mk \
	$(RUNTIME_IMPORT_LIBS) \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/import.lib

.PHONY: all toolchain
.DEFAULT_GOAL := all
