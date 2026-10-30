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


# The options lavapipe is built with on Tau (vulkan.mk) and on the build host (lavapipe-host.mk):
# one list, so the host's driver is the reference the Tau one is compared with. LLVM is static
# and position-independent (it ends up inside libvulkan_lvp.so), Release, without RTTI and
# exceptions (Mesa checks against them, -Dcpp_rtti=false), and without anything optional.

LLVM_COMMON := \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_PROJECTS= \
	-DLLVM_ENABLE_RUNTIMES= \
	-DLLVM_ENABLE_RTTI=OFF \
	-DLLVM_ENABLE_EH=OFF \
	-DLLVM_ENABLE_ASSERTIONS=OFF \
	-DLLVM_ENABLE_BINDINGS=OFF \
	-DLLVM_ENABLE_ZLIB=OFF \
	-DLLVM_ENABLE_ZSTD=OFF \
	-DLLVM_ENABLE_LIBXML2=OFF \
	-DLLVM_ENABLE_TERMINFO=OFF \
	-DLLVM_ENABLE_LIBEDIT=OFF \
	-DLLVM_ENABLE_LIBPFM=OFF \
	-DLLVM_ENABLE_CURL=OFF \
	-DLLVM_ENABLE_HTTPLIB=OFF \
	-DLLVM_ENABLE_TELEMETRY=OFF \
	-DLLVM_INCLUDE_TESTS=OFF \
	-DLLVM_INCLUDE_EXAMPLES=OFF \
	-DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_INCLUDE_DOCS=OFF \
	-DLLVM_BUILD_LLVM_DYLIB=OFF \
	-DLLVM_LINK_LLVM_DYLIB=OFF \
	-DLLVM_APPEND_VC_REV=OFF

# The libraries' build: no programs, PIC, threads, no crash handlers or unwind tables.
LLVM_LIBRARIES := \
	-DLLVM_INCLUDE_TOOLS=OFF \
	-DLLVM_BUILD_TOOLS=OFF \
	-DLLVM_INCLUDE_UTILS=OFF \
	-DLLVM_BUILD_UTILS=OFF \
	-DLLVM_ENABLE_PIC=ON \
	-DLLVM_ENABLE_THREADS=ON \
	-DLLVM_ENABLE_BACKTRACES=OFF \
	-DLLVM_ENABLE_CRASH_OVERRIDES=OFF \
	-DLLVM_ENABLE_PLUGINS=OFF \
	-DLLVM_ENABLE_UNWIND_TABLES=OFF

# The libraries, their headers and CMake package: `all` would also link programs.
LLVM_INSTALL_TARGETS := install-llvm-libraries install-llvm-headers install-cmake-exports

# Mesa without window systems and shader cache; MESA_OPTIONS adds lavapipe with MCJIT
# (llvm-orcjit=false), MESA_VENUS_OPTIONS Venus, the driver of virtio-gpu, without LLVM.
MESA_COMMON_OPTIONS := \
	--buildtype release \
	-Db_ndebug=true \
	-Ddefault_library=static \
	-Dcpp_std=gnu++20 \
	-Dplatforms= \
	-Dcpp_rtti=false \
	-Dglx=disabled \
	-Degl=disabled \
	-Dgbm=disabled \
	-Dgles1=disabled \
	-Dgles2=disabled \
	-Dopengl=false \
	-Dvulkan-layers= \
	-Dtools= \
	-Dshader-cache=disabled \
	-Dzlib=disabled \
	-Dzstd=disabled \
	-Dexpat=disabled \
	-Dxmlconfig=disabled \
	-Dlibunwind=disabled \
	-Dvalgrind=disabled \
	-Ddisplay-info=disabled \
	-Dspirv-tools=disabled \
	-Dbuild-tests=false \
	-Dvideo-codecs= \
	-Dperfetto=false

MESA_OPTIONS := $(MESA_COMMON_OPTIONS) \
	-Dvulkan-drivers=swrast \
	-Dgallium-drivers=llvmpipe \
	-Dllvm=enabled \
	-Dshared-llvm=disabled \
	-Ddraw-use-llvm=true \
	-Dllvm-orcjit=false

MESA_VENUS_OPTIONS := $(MESA_COMMON_OPTIONS) \
	-Dvulkan-drivers=virtio \
	-Dgallium-drivers= \
	-Dllvm=disabled

MESA_BROADCOM_OPTIONS := $(MESA_COMMON_OPTIONS) \
	-Dvulkan-drivers=broadcom \
	-Dgallium-drivers= \
	-Dllvm=disabled

MESA_LVP := src/gallium/targets/lavapipe/libvulkan_lvp.so
