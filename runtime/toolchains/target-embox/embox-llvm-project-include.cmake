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

# CMAKE_PROJECT_LLVM_INCLUDE for LLVM's own project(LLVM) on the Embox target
# (llvm.mk). Runs right after project(), before HandleLLVMOptions decides what
# kind of system this is.
#
# toolchain-libs.cmake says CMAKE_SYSTEM_NAME Generic, and CMake's Generic
# platform never sets UNIX. HandleLLVMOptions then takes
#   elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic") set(LLVM_ON_UNIX 0)
# and lib/Support compiles no platform code at all: no sys::Memory (the JIT's
# pages), no Process, no Path. The libc under LLVM here is POSIX enough -- the
# engine's sprt over Embox -- so say UNIX, as xenolith-host-wasm's platform file
# does for WASI.
set(UNIX 1)

# config-ix probes libc symbols. Under the toolchain-wide STATIC_LIBRARY default
# every check_symbol_exists answers yes; as EXECUTABLE the probes link against
# libsprt.a and the Embox libc with --no-undefined (common/configure.mk,
# SP_EMBOX_PROBE_LDFLAGS), so an absent function is detected absent. The same
# reasoning as embox-curl-project-include.cmake.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "EXECUTABLE")

# libc++ is the STL in every LLVM translation unit. sprt decides whether it is,
# once, at the first inclusion of its config header, from whether libc++'s
# <__config> came first; a unit that starts with a C header (through
# include_libc) decided "no", and sprt's <mutex> overlay then declared nothrow_t
# and the comparison categories a second time. Not through CMAKE_CXX_FLAGS: a
# value given for it replaces the toolchain file's CMAKE_CXX_FLAGS_INIT, include
# paths and all.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-D__SPRT_STD_EXTERNAL=1>)
