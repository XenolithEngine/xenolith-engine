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
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/21/include include

$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Windows' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang -D_WIN32_WINNT=0x0A00 -fexceptions -nostdinc' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang -D_WIN32_WINNT=0x0A00 -fexceptions -nostdinc' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang -D_WIN32_WINNT=0x0A00 -fexceptions -nostdlib' >> $@
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
	-DPKG_CONFIG_PATH="$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/pkgconfig"

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

$(TOOLCHAIN_OUTPUT_DIR)/lib/%.lib : $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows/def/%.def
	$(call rule_rm,$@)
	$(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-lib /def:$< /out:$@ /machine:$(SP_ARCH_WIN)

# Merge import libs with SPRT for dependencies build
$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/sprt.lib: $(RUNTIME_IMPORT_LIBS) \
		$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h \
		$(TOOLCHAIN_OUTPUT_DIR)/target.mk
	$(call rule_rm,$@)
	$(MAKE) -j8 -C $(SP_RUNTIME_ROOT) \
		STAPPLER_HOST_FILE=$(TOOLCHAIN_OUTPUT_DIR)/host/host.mk \
		STAPPLER_TARGET_FILE=$(TOOLCHAIN_OUTPUT_DIR)/target.mk \
		STAPPLER_TARGET=$(SP_ARCH_TARGET_CLANG) RELEASE=1
	$(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-lib /out:$@ \
			$(RUNTIME_IMPORT_LIBS) \
			$(SP_RUNTIME_ROOT)/stappler-build/$(SP_ARCH_TARGET_CLANG)/release/cc/sprt.lib \
			/machine:$(SP_ARCH_WIN)

$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/import.lib: $(RUNTIME_IMPORT_LIBS)
	$(call rule_rm,$@)
	$(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-lib /out:$@ \
			$(RUNTIME_IMPORT_LIBS) \
			/machine:$(SP_ARCH_WIN)

all: $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	$(TOOLCHAIN_OUTPUT_DIR)/target.mk \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/include/simde/simde-arch.h \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/sprt.lib \
	$(TOOLCHAIN_OUTPUT_DIR)/usr/lib/import.lib

.PHONY: all
.DEFAULT_GOAL := all
