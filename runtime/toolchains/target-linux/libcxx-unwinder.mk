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

# The single place that decides whether libc++abi.a carries the unwinder inside.
#
# Included BOTH from libc++.mk AND from compiler_rt.mk. That is not belt and
# braces: compiler_rt.mk builds THE SAME runtimes (libcxx;libcxxabi;libunwind)
# together with compiler-rt and its `--target install` overwrites what libc++.mk
# produced. So any divergence between the two option lists would silently be won
# by compiler_rt.mk — on glibc targets it is its settings that end up in the
# target.
#
# On glibc and musl the unwinder is NOT linked in. There must be exactly one per
# process: its mutable state is the FDE table, into which a JIT (the shader
# compiler inside a GPU driver) registers frames through __register_frame, and a
# second copy cannot see that table. On top of that _Unwind_Context is opaque: a
# foreign personality routine reads it only through _Unwind_Get*/_Unwind_Set*,
# and those must come from the copy that built the context. A copy merged into
# the archive makes the unwinder private to every executable — precisely what
# must not happen.
#
# Instead the runtime defines the entry points itself and resolves them through
# dlopen("libgcc_s.so.1"), i.e. the same way and on the same SONAME glibc uses
# in misc/unwind-link.c — that way the copy is shared, with no glibc patch and
# no new link-time dependency. Details in
# runtime/core/runtime_core_setjmp.cpp.
#
# On glibc this is no new requirement on the environment: libgcc_s.so.1 is
# needed already, otherwise pthread_exit dies with __libc_fatal — and sprt calls
# it on every thread teardown. On musl there is no such guarantee (libgcc_s.so.1
# is a separate package there), but that is not a problem either: the runtime
# survives its absence. The brokers warn and return "nothing was unwound",
# longjmp jumps without running destructors, backtraces come out empty.
# Functionality is lost, the program is not.
#
# This condition MUST match the __SPRT_UNWIND_DLOPEN gate in make/os/linux.mk:
# if the runtime defines the entry points where libc++abi still carries its own,
# the link fails on duplicate symbols. Hence an explicit list rather than
# "anything Linux" — e2k has a toolchain of its own and nothing to borrow from.

ifneq (,$(findstring linux-gnu,$(SP_TARGET)))
LIBCXX_STATIC_UNWINDER := Off
else ifneq (,$(findstring linux-musl,$(SP_TARGET)))
LIBCXX_STATIC_UNWINDER := Off
else
LIBCXX_STATIC_UNWINDER := On
endif
