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

# libcxx.mk — cross-build libc++ + libc++abi from src/llvm-project into an Apple
# target sysroot (used by the +open flow to give the SDK-free sysroot a real C++
# runtime). SDK-like layout (NON-merged, matching the macOS SDK exactly):
#   - separate libc++.1.dylib + libc++abi.1.dylib, both install-name'd under /usr/lib;
#   - libc++abi is NOT merged into libc++ (LIBCXX_ENABLE_STATIC_ABI_LIBRARY=OFF), so
#     libcxx's Apple weak.exp/notweak.exp RTTI-visibility lists apply (they are gated
#     on the non-merged abi path, libcxx/src/CMakeLists.txt:212) and the exported
#     symbol set matches the SDK's; libc++ carries a load dep on /usr/lib/libc++abi;
#   - tbd-v4 link stubs are generated from both dylibs with llvm-readtapi.
# Cross-linking Mach-O needs ld64.lld — carried by the sysroot toolchain.cmake
# (CMAKE_LINKER_TYPE LLD, emitted by init-target.mk on non-Darwin hosts).
#
# Invoked from target-apple/Makefile with the standard FORWARD_VARS
# (SP_INSTALL_PREFIX = the target sysroot, SP_TOOLCHAIN_FILE, LIB_SRC_DIR, ...).

.DEFAULT_GOAL := all

# Build libc++/libc++abi from the LLVM release whose libc++ interfaces MATCH the
# target SDK's, not the (newer) toolchain LLVM: macOS 14.5's <__config> reports
# _LIBCPP_VERSION 170006, so pin llvmorg-17.0.6. Cloned blobless+sparse (runtimes
# tree only) beside the other third-party sources.
LIBCXX_LLVM_TAG := llvmorg-17.0.6
LLVM_DIR   := $(LIB_SRC_DIR)/llvm-project-17
BUILD_DIR  := build/libcxx-$(SP_ARCH)
READTAPI   := $(SP_INSTALL_PREFIX)/host/bin/llvm-readtapi
NAMETOOL   := $(SP_INSTALL_PREFIX)/host/bin/llvm-install-name-tool
NM         := $(SP_INSTALL_PREFIX)/host/bin/llvm-nm

# The exact libc++abi symbol lists libcxx reexports into libc++ on Apple (cxx_shared
# + the cxxabi-reexports target): base itanium ABI, exceptions, operator new/delete,
# the personality routine, and libcxx's own curated subset. Their union (∩ what our
# libc++abi actually exports) is spliced into libc++.tbd as `reexports:`.
REEXP_LISTS := \
	$(LLVM_DIR)/libcxx/lib/libc++abi.exp \
	$(LLVM_DIR)/libcxxabi/lib/itanium-base.exp \
	$(LLVM_DIR)/libcxxabi/lib/exceptions.exp \
	$(LLVM_DIR)/libcxxabi/lib/new-delete.exp \
	$(LLVM_DIR)/libcxxabi/lib/personality-v0.exp

$(LLVM_DIR):
	@mkdir -p $(dir $@)
	git clone --depth 1 --filter=blob:none --sparse --branch $(LIBCXX_LLVM_TAG) \
		https://github.com/llvm/llvm-project.git $@
	cd $@ && git sparse-checkout set runtimes cmake libcxx libcxxabi libunwind third-party \
		llvm/cmake llvm/utils/llvm-lit llvm/utils/lit

CONFIGURE := \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE)) \
	-DCMAKE_OSX_SYSROOT=$(SP_INSTALL_PREFIX) \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_HOST_TRIPLE="$(SP_TARGET)" \
	-DLLVM_DEFAULT_TARGET_TRIPLE="$(SP_TARGET)" \
	-DLIBCXX_ABI_VERSION=1 \
	-DLIBCXX_CXX_ABI=libcxxabi \
	-DLIBCXX_USE_COMPILER_RT=OFF \
	-DLIBCXX_ENABLE_SHARED=ON \
	-DLIBCXX_ENABLE_STATIC=OFF \
	-DLIBCXX_ENABLE_VENDOR_AVAILABILITY_ANNOTATIONS=ON \
	-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=OFF \
	-DLIBCXX_INCLUDE_BENCHMARKS=OFF \
	-DLIBCXX_INCLUDE_TESTS=OFF \
	-DLIBCXX_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXX_INSTALL_INCLUDE_DIR=include_libc/c++/v1 \
	-DLIBCXX_INSTALL_INCLUDE_TARGET_DIR=include_libc/c++/v1 \
	-DLIBCXXABI_INSTALL_INCLUDE_DIR=include_libc/c++/v1 \
	-DLIBUNWIND_INSTALL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/include_libc \
	-DLIBCXXABI_USE_COMPILER_RT=OFF \
	-DLIBCXXABI_USE_LLVM_UNWINDER=ON \
	-DLIBCXXABI_ENABLE_SHARED=ON \
	-DLIBCXXABI_ENABLE_STATIC=OFF \
	-DLIBCXXABI_INCLUDE_TESTS=OFF \
	-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBUNWIND_USE_COMPILER_RT=OFF \
	-DLIBUNWIND_ENABLE_SHARED=ON \
	-DLIBUNWIND_ENABLE_STATIC=OFF \
	-DLIBUNWIND_INCLUDE_TESTS=OFF \
	-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
	-DCMAKE_EXE_LINKER_FLAGS=-L$(SP_INSTALL_PREFIX)/usr/lib \
	-DCMAKE_SHARED_LINKER_FLAGS=-L$(SP_INSTALL_PREFIX)/usr/lib \
	-DCMAKE_INSTALL_NAME_DIR=/usr/lib \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX)

all: | $(LLVM_DIR)
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR); cmake -G "Ninja" -S $(LLVM_DIR)/runtimes $(CONFIGURE)
	cd $(BUILD_DIR); cmake --build . --target install
	@UW=`ls $(BUILD_DIR)/usr/lib/libunwind.1.0.dylib $(BUILD_DIR)/lib/libunwind.1.0.dylib 2>/dev/null | head -1`; \
	cp -f "$$UW" $(SP_INSTALL_PREFIX)/usr/lib/libunwind.1.0.dylib
	cd $(SP_INSTALL_PREFIX)/usr/lib && ln -sf libunwind.1.0.dylib libunwind.1.dylib && ln -sf libunwind.1.dylib libunwind.dylib
	rm -rf $(BUILD_DIR)
	@# Force the SDK /usr/lib install-names (CMAKE_INSTALL_NAME_DIR does not propagate
	@# into the runtimes sub-build, which defaults to @rpath) and re-point libc++'s
	@# dependency on libc++abi to /usr/lib. Applied to the real files; symlinks follow.
	$(NAMETOOL) -id /usr/lib/libunwind.1.dylib $(SP_INSTALL_PREFIX)/usr/lib/libunwind.1.0.dylib
	$(NAMETOOL) -id /usr/lib/libc++abi.1.dylib $(SP_INSTALL_PREFIX)/usr/lib/libc++abi.1.0.dylib
	$(NAMETOOL) -id /usr/lib/libc++.1.dylib $(SP_INSTALL_PREFIX)/usr/lib/libc++.1.0.dylib
	$(NAMETOOL) -change @rpath/libc++abi.1.dylib /usr/lib/libc++abi.1.dylib \
		$(SP_INSTALL_PREFIX)/usr/lib/libc++.1.0.dylib
	@# libc++abi statically links the unwinder by default, but re-point any @rpath dep on
	@# libunwind to /usr/lib anyway (harmless no-op if the load command isn't present).
	-$(NAMETOOL) -change @rpath/libunwind.1.dylib /usr/lib/libunwind.1.dylib \
		$(SP_INSTALL_PREFIX)/usr/lib/libc++abi.1.0.dylib 2>/dev/null || true
	@# Generate tbd-v4 link stubs from the dylibs; consumers link the stubs and resolve
	@# the real dylibs at run time (SDK-style, separate libc++ + libc++abi + libunwind).
	$(READTAPI) -stubify --filetype=tbd-v4 $(SP_INSTALL_PREFIX)/usr/lib/libunwind.1.dylib \
		-o $(SP_INSTALL_PREFIX)/usr/lib/libunwind.tbd
	$(READTAPI) -stubify --filetype=tbd-v4 $(SP_INSTALL_PREFIX)/usr/lib/libc++abi.1.dylib \
		-o $(SP_INSTALL_PREFIX)/usr/lib/libc++abi.tbd
	$(READTAPI) -stubify --filetype=tbd-v4 $(SP_INSTALL_PREFIX)/usr/lib/libc++.1.dylib \
		-o $(SP_INSTALL_PREFIX)/usr/lib/libc++.tbd
	@# install-name-matching aliases: ld64 resolves a recorded load path like
	@# /usr/lib/libc++.1.dylib against the sysroot by swapping the extension to .tbd
	@# (flat-namespace links do this for TRANSITIVE loads too), so the stub must also
	@# exist under the versioned basename — mirroring the real SDK's file+symlink pair.
	cd $(SP_INSTALL_PREFIX)/usr/lib && ln -sf libc++.tbd libc++.1.tbd \
		&& ln -sf libc++abi.tbd libc++abi.1.tbd && ln -sf libunwind.tbd libunwind.1.tbd
	@# ld64.lld does not implement Apple's -reexported_symbols_list, so the built
	@# libc++.dylib carries no LC_REEXPORT of libc++abi. Fix it at the tbd level like
	@# the SDK (which ships tbd-only): splice a `reexports:` section listing libcxx's
	@# curated libc++abi.exp symbols into libc++.tbd. `-lc++` then resolves the abi at
	@# link time; at run time the system /usr/lib/libc++.1.dylib (== our install-name)
	@# reexports libc++abi, exactly as macOS ships it.
	@echo "Splice libc++abi reexports into libc++.tbd"
	@TBD=$(SP_INSTALL_PREFIX)/usr/lib/libc++.tbd; \
	cat $(REEXP_LISTS) | grep '^_' | sort -u > $$TBD.want; \
	$(NM) --defined-only --extern-only $(SP_INSTALL_PREFIX)/usr/lib/libc++abi.1.dylib \
		| awk '{print $$NF}' | sort -u > $$TBD.have; \
	SYMS=`comm -12 $$TBD.want $$TBD.have | paste -sd, - | sed 's/,/, /g'`; \
	TGT=`grep -m1 '^targets:' $$TBD | sed 's/^targets: *//'`; \
	{ grep -v '^\.\.\.$$' $$TBD; \
	  echo "reexports:"; \
	  echo "  - targets:         $$TGT"; \
	  echo "    symbols:         [ $$SYMS ]"; \
	  echo "..."; } > $$TBD.new; \
	mv $$TBD.new $$TBD; rm -f $$TBD.want $$TBD.have

.PHONY: all
