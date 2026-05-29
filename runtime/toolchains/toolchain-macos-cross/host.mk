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

HOST_SYSROOT := sysroot-host
HOST_CLANG_CC := $(HOST_SYSROOT)/bin/clang
HOST_CLANG_CXX := $(HOST_SYSROOT)/bin/clang++
HOST_LLVM_LIPO := $(HOST_SYSROOT)/bin/llvm-lipo
HOST_LLVM_LIBTOOL := $(HOST_SYSROOT)/bin/llvm-libtool-darwin

HOST_TARGET_CLANG_CC := $(HOST_SYSROOT)/bin/$(SP_ARCH_CLANG)-clang
HOST_TARGET_CLANG_CXX := $(HOST_SYSROOT)/bin/$(SP_ARCH_CLANG)-clang++

$(HOST_CLANG_CC): $(LLVM_DIR)
	rm -rf build/host-llvm
	cmake \
		-G Ninja -S $(LLVM_DIR)/llvm -B build/host-llvm \
		-DLLVM_ENABLE_PROJECTS="lld;clang" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLIBCXX_HAS_ATOMIC_LIB=Off \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXX_USE_COMPILER_RT=On \
		-DLIBCXXABI_USE_COMPILER_RT=On \
		-DLIBUNWIND_USE_COMPILER_RT=On \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_USE_LLVM_UNWINDER=On \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(HOST_SYSROOT)) \
		-DLLVM_LOCAL_RPATH='$(realpath $(HOST_SYSROOT))/lib;$$ORIGIN/../lib' \
		-DCMAKE_INSTALL_RPATH="$(realpath $(HOST_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_BUILD_RPATH="$(realpath $(HOST_SYSROOT))/lib;$$ORIGIN/../lib" \
		-DCMAKE_INSTALL_PREFIX=$(HOST_SYSROOT)
	cmake --build build/host-llvm
	cmake --install build/host-llvm


$(HOST_TARGET_CLANG_CC): $(HOST_CLANG_CC)
	@echo '#!/usr/bin/env bash' > $@
	@echo 'bin_dir=`dirname "$$0"`' >> $@
	@echo 'if [ "$$1" != "-cc1" ]; then' >> $@
	@echo '    "$$bin_dir/clang" --target=$(SP_ARCH_CLANG) -mmacosx-version-min=14.4 "$$@"' >> $@
	@echo 'else' >> $@
	@echo '    "$$bin_dir/clang" "$$@"' >> $@
	@echo 'fi' >> $@
	chmod +x $@

$(HOST_TARGET_CLANG_CXX): $(HOST_CLANG_CC)
	@echo '#!/usr/bin/env bash' > $@
	@echo 'bin_dir=`dirname "$$0"`' >> $@
	@echo 'if [ "$$1" != "-cc1" ]; then' >> $@
	@echo '    "$$bin_dir/clang++" --target=$(SP_ARCH_CLANG) -mmacosx-version-min=14.4 "$$@"' >> $@
	@echo 'else' >> $@
	@echo '    "$$bin_dir/clang++" "$$@"' >> $@
	@echo 'fi' >> $@
	chmod +x $@

host: $(HOST_CLANG_CC) $(HOST_TARGET_CLANG_CC) $(HOST_TARGET_CLANG_CXX)
