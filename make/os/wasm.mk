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

OSTYPE_IS_WASM := 1

# wasm object files are still ELF-ish archives from ar's point of view; the final
# artifact is a .wasm module produced by wasm-ld, but the runtime is delivered as
# static archives (.a) that later link into the module.
OSTYPE_EXEC_SUFFIX := .wasm
OSTYPE_DSO_SUFFIX := .wasm
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := WASM

OSTYPE_CFLAGS := -Wall -Wno-unused-command-line-argument

ifeq ($(RELEASE),1)
OSTYPE_CFLAGS +=
OSTYPE_LDFLAGS :=
else
OSTYPE_CFLAGS += -g
OSTYPE_LDFLAGS := -g
endif

OSTYPE_GENERAL_CFLAGS := $(OSTYPE_CFLAGS)
OSTYPE_LIB_CFLAGS :=
OSTYPE_EXEC_CFLAGS :=

# Freestanding wasm: no host EH/RTTI runtime yet (setjmp/EH is a later milestone),
# so build without exceptions like the Linux preset, and drop the vla/overloaded
# diagnostics the rest of the tree already silences.
# SIMDE_FLOAT16_API=1 (SIMDE_FLOAT16_API_PORTABLE): wasm clang has no usable
# native _Float16 / __fp16, so force simde's portable (struct-backed) float16 in
# the geom SIMD headers instead of its default native-type detection.
OSTYPE_GENERAL_CXXFLAGS := $(OSTYPE_CFLAGS) -Wno-vla-cxx-extension -Wno-overloaded-virtual \
	-frtti -fno-exceptions -DSIMDE_FLOAT16_API=1
# simde (SIMD-everywhere) headers live in the sysroot usr/include; the geom SIMD headers
# pull <simde/x86/*.h>. -idirafter keeps this at lowest priority so it only resolves
# includes not already satisfied by the freestanding libc/STL trees.
OSTYPE_GENERAL_CFLAGS += -idirafter $(TARGET_SYSROOT)/usr/include
OSTYPE_GENERAL_CXXFLAGS += -idirafter $(TARGET_SYSROOT)/usr/include
OSTYPE_LIB_CXXFLAGS :=
OSTYPE_EXEC_CXXFLAGS :=

OSTYPE_GENERAL_LDFLAGS := $(OSTYPE_LDFLAGS) -fuse-ld=lld
# Graphics/font/image dependency libraries (freetype/harfbuzz/gif/png/...) live in the
# sysroot; add its lib dir to the link search path.
OSTYPE_GENERAL_LDFLAGS += -L$(TARGET_SYSROOT)/usr/lib
# The host (JS glue) owns linear memory and shares it with the module: the runtime
# startup imports env.memory and the T1 host functions read/write through it. Link
# executables to import (not export/own) memory so host and module see one buffer.
#
# Threads: --shared-memory makes the imported memory a SharedArrayBuffer and switches
# thread-local storage to the per-thread model, emitting __wasm_init_tls / __tls_size.
# Each thread runs in its own Worker with its own module instance over the one shared
# memory; the broker sets that instance's __stack_pointer to a freshly malloc'd stack and
# calls __wasm_init_tls before the thread entry (__xl_thread_entry). The exports below are
# the surface the JS thread broker needs, plus __sprt_malloc_usage so the JS host can
# watch allocator growth live (a heap burn-down is invisible otherwise).
#
# wasm64 (memory64) lifts the 4 GiB address limit; 16 GiB is the most V8 accepts for a
# memory64 maximum. The host may still hand the module a smaller maximum (an imported
# memory only has to fit inside the declared one), which is how a browser that cannot
# reserve 16 GiB of shared memory falls back.
ifeq ($(TARGET_ARCH),wasm64)
OSTYPE_WASM_MAX_MEMORY ?= 17179869184 # 16 GiB (262144 pages)
else
OSTYPE_WASM_MAX_MEMORY ?= 1073741824 # 1 GiB (16384 pages)
endif
# The main thread runs on the stack wasm-ld lays out at the bottom of memory, and wasm-ld's
# default is 64 KiB - far below what a native main thread gets. An overflow runs below
# address 0 and traps as "memory access out of bounds" in whatever function it hit: edlib's
# alignment in stappler_search does it on wasm32, libc++'s format tests on wasm64, where every
# pointer-sized field doubles the frames. The main thread gets the 1 MiB a spawned thread has
# (__SPRT_WASM_THREAD_STACK in pthread_native_wasm.cc).
OSTYPE_WASM_STACK_SIZE ?= 1048576
OSTYPE_EXEC_LDFLAGS := -Wl,--import-memory,--shared-memory,--max-memory=$(OSTYPE_WASM_MAX_MEMORY) \
	-Wl,-z,stack-size=$(OSTYPE_WASM_STACK_SIZE) \
	-Wl,--export=__wasm_init_tls,--export=__tls_size,--export=__tls_align,--export=__tls_base \
	-Wl,--export=__stack_pointer,--export=malloc,--export=free,--export=__xl_thread_entry \
	-Wl,--export=__sprt_malloc_usage,--export-table
OSTYPE_LIB_LDFLAGS :=

WASM := 1
