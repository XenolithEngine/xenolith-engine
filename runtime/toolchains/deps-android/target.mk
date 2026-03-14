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

ifdef ANDROID_NDK_ROOT
NDK ?= $(ANDROID_NDK_ROOT)
else
NDK ?= $(HOME)/Android/Ndk
endif

THIS_FILE := $(lastword $(MAKEFILE_LIST))

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/find-recursive.mk

NDK_INCLUDES = $(NDK)/toolchains/llvm/prebuilt/$(HOST_ANDROID)/sysroot/usr/include
NDK_LIBDIR = $(NDK)/toolchains/llvm/prebuilt/$(HOST_ANDROID)/sysroot/usr/lib/$(ANDROID_TARGET)/$(ANDROID_PLATFORM_LEVEL)

TARGET_INCLUDES = $(TOOLCHAIN_OUTPUT_DIR)/include
TARGET_LIBDIR = $(TOOLCHAIN_OUTPUT_DIR)/lib

ANDROID_HEADERS := $(filter-out $(NDK_INCLUDES)/zlib.h $(NDK_INCLUDES)/zconf.h, $(wildcard $(NDK_INCLUDES)/*.h))

ANDROID_HEADERS += $(wildcard $(addsuffix /*.h, \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/aaudio) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/amidi) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/android) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/arpa) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/asm-generic) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/bits) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/camera) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/drm) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/linux) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/media) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/misc) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/mtd) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/net) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/netinet) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/netpacket) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/OMXAL) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/rdma) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/scsi) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/SLES) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/sound) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/sys) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/unicode) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/video) \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/xen) \
))

ANDROID_ARCH_HEADERS := $(wildcard $(addsuffix /*.h, \
	$(call sp_find_dirs_recursive,$(NDK_INCLUDES)/$(ANDROID_TARGET)) \
))

ANDROID_CRT := $(addprefix $(NDK_LIBDIR)/, \
	crtbegin_dynamic.o \
	crtbegin_so.o \
	crtbegin_static.o \
	crtend_android.o \
	crtend_so.o \
)

ANDROID_LIBS := $(addprefix $(NDK_LIBDIR)/, \
	libandroid.so \
	libcamera2ndk.so \
	libc.so \
	libdl.so \
	libjnigraphics.so \
	liblog.so \
	libmediandk.so \
	libm.so \
	libOpenMAXAL.so \
	libOpenSLES.so \
)

TARGET_LIBCXX := $(TOOLCHAIN_OUTPUT_DIR)/usr/lib/libc++.a

TARGET_HEADERS := $(patsubst $(NDK_INCLUDES)/%,$(TARGET_INCLUDES)/%,$(ANDROID_HEADERS))

TARGET_ARCH_HEADERS := $(patsubst $(NDK_INCLUDES)/$(ANDROID_TARGET)/%,$(TARGET_INCLUDES)/%,$(ANDROID_ARCH_HEADERS))

TARGET_CRT := $(patsubst $(NDK_LIBDIR)/%,$(TARGET_LIBDIR)/%,$(ANDROID_CRT))

TARGET_LIBS := $(patsubst $(NDK_LIBDIR)/%,$(TARGET_LIBDIR)/%,$(ANDROID_LIBS))

# TARGET_IFSS := $(patsubst $(NDK_LIBDIR)/%.so,$(TARGET_LIBDIR)/%.ifs,$(ANDROID_LIBS))

$(info NDK_LIBDIR $(NDK_LIBDIR))

$(TARGET_INCLUDES)/%.h : $(NDK_INCLUDES)/%.h
	@mkdir -p $(dir $@)
	cp -f $< $@

$(TARGET_INCLUDES)/%.h : $(NDK_INCLUDES)/$(ANDROID_TARGET)/%.h
	@mkdir -p $(dir $@)
	cp -f $< $@

$(TARGET_LIBDIR)/%.o : $(NDK_LIBDIR)/%.o
	@mkdir -p $(dir $@)
	cp -f $< $@

$(TARGET_LIBDIR)/%.so : $(NDK_LIBDIR)/%.so
	@mkdir -p $(dir $@)
	$(dir $(THIS_FILE))../hosts/$(HOST_ID)/bin/llvm-ifs $< --output-elf=$@

$(TOOLCHAIN_OUTPUT_DIR)/usr/include/android/api-level.h: $(TARGET_INCLUDES)/android/api-level.h
	@mkdir -p $(dir $@)
	cp -f $< $@

TOOLCHAIN_CFLAGS :=  -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)

$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake: $(lastword $(MAKEFILE_LIST)) $(TOOLCHAIN_OUTPUT_DIR)/usr/include/android/api-level.h
	@echo 'set(CMAKE_SYSTEM_NAME Android)' > $@
	@echo 'set(CMAKE_ANDROID_ARCH "$(ANDROID_ARCH)")' >> $@
	@echo 'set(CMAKE_ANDROID_ARCH_ABI "$(ANDROID_ARCH_ABI)")' >> $@
	@echo 'set(CMAKE_ANDROID_API "$(ANDROID_PLATFORM_LEVEL)")' >> $@
	@echo 'set(CMAKE_ANDROID_STANDALONE_TOOLCHAIN "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_SYSROOT "$${CMAKE_CURRENT_LIST_DIR}/sysroot")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)")' >> $@
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
	@echo 'set(ANDROID_PLATFORM_LEVEL "$(ANDROID_PLATFORM_LEVEL)")' >> $@
	rm -f $(TOOLCHAIN_OUTPUT_DIR)/sysroot
	cd $(TOOLCHAIN_OUTPUT_DIR); ln -fs . sysroot
	rm -f $(TOOLCHAIN_OUTPUT_DIR)/host
	cd $(TOOLCHAIN_OUTPUT_DIR); ln -fs ../../hosts/$(HOST_ID) host
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang
	cd $(TOOLCHAIN_OUTPUT_DIR)/lib/clang; ln -fs ../../host/lib/clang/21/include include

$(TOOLCHAIN_OUTPUT_DIR)/target.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Android' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH_TARGET)' >> $@
	@echo 'TARGET_NAME := $(SP_ARCH_TARGET_CLANG)$(ANDROID_PLATFORM_LEVEL)' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/include_libc' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/lib' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := -resource-dir $$(TARGET_SYSROOT)/lib/clang -lc++ -lc++abi' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@

LIBNAME = llvm-project

ifeq ($(SP_RES_ARCH),i686)
ANDROID_UNWIND_LIB := $(NDK)/toolchains/llvm/prebuilt/$(HOST_ANDROID)/lib/clang/21/lib/linux/i386/libunwind.a
else
ANDROID_UNWIND_LIB := $(NDK)/toolchains/llvm/prebuilt/$(HOST_ANDROID)/lib/clang/21/lib/linux/$(SP_RES_ARCH)/libunwind.a
endif

ANDROID_RT_BUILTINS_LIB := $(NDK)/toolchains/llvm/prebuilt/$(HOST_ANDROID)/lib/clang/21/lib/linux/libclang_rt.builtins-$(SP_RES_ARCH)-android.a

TMP_UNWIND_LIB := $(TOOLCHAIN_OUTPUT_DIR)/lib/libunwind.a
TMP_RT_BUILTINS_LIB := $(TOOLCHAIN_OUTPUT_DIR)/lib/clang/lib/linux/libclang_rt.builtins-$(SP_RES_ARCH)-android.a

$(TARGET_LIBCXX): $(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake
	@echo "Build TARGET_LIBCXX $(TARGET_LIBCXX)"
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang/lib/linux
	cp -f $(ANDROID_UNWIND_LIB) $(TMP_UNWIND_LIB)
	cp -f $(ANDROID_RT_BUILTINS_LIB) $(TMP_RT_BUILTINS_LIB)
	rm -rf $(LIBNAME)
	mkdir -p $(LIBNAME)
	cd $(LIBNAME); cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
		-G "Ninja" -S $(dir $(THIS_FILE))../../src/$(LIBNAME)/runtimes \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_ENABLE_PIC=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXX_ENABLE_SHARED=Off \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXX_INSTALL_LIBRARY_DIR=usr/lib \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
		-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
		-DLIBCXXABI_ENABLE_SHARED=Off \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DLIBUNWIND_ENABLE_SHARED=Off \
		-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=ON \
		-DLLVM_HOST_TRIPLE="$(SP_ARCH_TARGET_CLANG)" \
		-DLLVM_DEFAULT_TARGET_TRIPLE="$(SP_ARCH_TARGET_CLANG)" \
		-DCOMPILER_RT_DEFAULT_TARGET_TRIPLE=$(SP_ARCH_TARGET_CLANG) \
		-DCMAKE_INSTALL_PREFIX="${TOOLCHAIN_OUTPUT_DIR}" \
		-DCMAKE_INSTALL_BINDIR=$(abspath $(dir $(THIS_FILE))$(LIBNAME)/bin) \
		-DCMAKE_INSTALL_DATAROOTDIR=$(abspath $(dir $(THIS_FILE))$(LIBNAME)/share) \
		-DCMAKE_INSTALL_LIBDIR="${TOOLCHAIN_OUTPUT_DIR}/usr/lib" \
		-DCMAKE_INSTALL_INCLUDEDIR="${TOOLCHAIN_OUTPUT_DIR}/usr/include"

	cd $(LIBNAME); cmake  --build . --config Release --target install-cxx
	cd $(LIBNAME); cmake  --build . --config Release --target install-cxxabi
	cd $(LIBNAME); cmake  --build . --config Release --target install-unwind
	cd $(LIBNAME); cmake  --build . --config Release --target install
	rm -rf $(LIBNAME)
	rm ${TMP_UNWIND_LIB}
	mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/lib/clang/lib
	rm -rf $(TOOLCHAIN_OUTPUT_DIR)/lib/clang/lib/linux
	mv -f $(TOOLCHAIN_OUTPUT_DIR)/lib/linux $(TOOLCHAIN_OUTPUT_DIR)/lib/clang/lib
	rm -rf $(TOOLCHAIN_OUTPUT_DIR)/lib/linux

TARGET_CPU_FEATURES := $(addprefix $(TOOLCHAIN_OUTPUT_DIR)/sources/android/cpufeatures/, \
		cpu-features.c \
		cpu-features.h \
	)

$(TOOLCHAIN_OUTPUT_DIR)/AndroidVersion.txt:
	@mkdir -p $(dir $@)
	@echo 'r29' > $@

$(TOOLCHAIN_OUTPUT_DIR)/sources/android/cpufeatures/cpu-features.c: $(NDK)/sources/android/cpufeatures/cpu-features.c
	@mkdir -p $(dir $@)
	cp -f $< $@

$(TOOLCHAIN_OUTPUT_DIR)/sources/android/cpufeatures/cpu-features.h: $(NDK)/sources/android/cpufeatures/cpu-features.h
	@mkdir -p $(dir $@)
	cp -f $< $@

$(TOOLCHAIN_OUTPUT_DIR)/bin/$(ANDROID_TARGET)-clang:
	@mkdir -p $(dir $@)
	@echo 'bin_dir=`dirname "$$0"`' > $@
	@echo 'if [ "$$1" != "-cc1" ]; then' >> $@
	@echo '    "$$bin_dir/../host/bin/clang" --target=$(ANDROID_TARGET)$(ANDROID_PLATFORM_LEVEL) "$$@"' >> $@
	@echo 'else' >> $@
	@echo '    "$$bin_dir/../host/bin/clang" "$$@"' >> $@
	@echo 'fi' >> $@
	@chmod +x $@

$(TOOLCHAIN_OUTPUT_DIR)/bin/$(ANDROID_TARGET)-llvm-ar:
	@mkdir -p $(dir $@)
	@echo 'bin_dir=`dirname "$$0"`' > $@
	@echo '"$$bin_dir/../host/bin/llvm-ar" "$$@"' >> $@
	@chmod +x $@

$(TOOLCHAIN_OUTPUT_DIR)/bin/$(ANDROID_TARGET)-ranlib:
	@mkdir -p $(dir $@)
	@echo 'bin_dir=`dirname "$$0"`' > $@
	@echo '"$$bin_dir/../host/bin/llvm-ranlib" "$$@"' >> $@
	@chmod +x $@

all: $(TARGET_HEADERS) $(TARGET_ARCH_HEADERS) $(TARGET_CRT) $(TARGET_LIBS) $(TARGET_LIBCXX) $(TARGET_CPU_FEATURES) \
	$(TOOLCHAIN_OUTPUT_DIR)/AndroidVersion.txt \
	$(TOOLCHAIN_OUTPUT_DIR)/bin/$(ANDROID_TARGET)-clang \
	$(TOOLCHAIN_OUTPUT_DIR)/bin/$(ANDROID_TARGET)-llvm-ar \
	$(TOOLCHAIN_OUTPUT_DIR)/bin/$(ANDROID_TARGET)-ranlib \
	$(TOOLCHAIN_OUTPUT_DIR)/target.mk

.PHONY: all
.DEFAULT_GOAL := all
