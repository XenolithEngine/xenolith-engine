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

OSTYPE_IS_LINUX := 1

OSTYPE_EXEC_SUFFIX :=
OSTYPE_DSO_SUFFIX := .so
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

# Non-NDK Android also uses common linux preset, but with ANDROID config flag
ifeq ($(TARGET_SYSTEM),Android)
OSTYPE_CONFIG_FLAGS := ANDROID
else
OSTYPE_CONFIG_FLAGS := LINUX
endif

OSTYPE_GENERAL_CFLAGS := -Wall -fvisibility=hidden
OSTYPE_LIB_CFLAGS := -fPIC -DPIC
OSTYPE_EXEC_CFLAGS :=

OSTYPE_GENERAL_CXXFLAGS := -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual \
	-frtti -fvisibility=hidden -fvisibility-inlines-hidden -fno-exceptions

# Use dynamically loaded unwinder instead of prebuilt one
ifneq (,$(findstring linux-gnu,$(TARGET_NAME)))
OSTYPE_UNWIND_DLOPEN := 1
else ifneq (,$(findstring linux-musl,$(TARGET_NAME)))
OSTYPE_UNWIND_DLOPEN := 1
endif

ifeq ($(OSTYPE_UNWIND_DLOPEN),1)
OSTYPE_GENERAL_CFLAGS += -D__SPRT_UNWIND_DLOPEN=1
OSTYPE_GENERAL_CXXFLAGS += -D__SPRT_UNWIND_DLOPEN=1
endif
OSTYPE_LIB_CXXFLAGS := -fPIC -DPIC
OSTYPE_EXEC_CXXFLAGS :=

OSTYPE_GENERAL_LDFLAGS :=
OSTYPE_EXEC_LDFLAGS :=
OSTYPE_LIB_LDFLAGS := -rdynamic -Wl,--exclude-libs,ALL

#
# C++ ABI runtime interposition (ELF-specific, executables only)
#
# The Linux target links libc++abi STATICALLY (-l:libc++abi.a, see
# runtime/runtime.mk) and defines its own replaceable operator new/delete over the
# sprt allocator (runtime/libc_wrapper/cxx/SPRTCxxNewDelete.cpp). None of that
# reaches .dynsym: an executable exports only what the linker is explicitly asked
# to export. So a shared object that pulls in the system libc++abi.so ends up with
# a SECOND C++ ABI runtime in the same process.
#
# The exception machinery is not the problem here — the runtime is built
# -fno-exceptions and nothing throws across the boundary. What breaks is the state
# that must be unique per process:
#
#   * operator new / delete   the .so allocates through libc++abi's malloc
#                             wrappers while the executable frees through the
#                             sprt allocator, and vice versa
#   * new / terminate handler storage is per-copy, so a handler installed on one
#                             side is invisible to the other
#   * __cxa_guard_*           a function-local static in an inline function from a
#                             shared header is ONE merged guard variable driven by
#                             TWO independent acquire/release implementations
#   * __dynamic_cast and the  compiler-emitted typeinfo carries a vptr to whichever
#     __cxxabiv1 type_info    image defined the vtable, and the cast walker
#     vtables                 dispatches through it. That is an internal,
#                             unversioned ABI — the pair MUST come from one
#                             implementation. Exporting it is OFF by default all
#                             the same, and the measurement that says why is at
#                             SPRT_EXPORT_TYPEINFO below.
#   * the unwinder            libc++abi.a is built with the LLVM unwinder merged
#                             in (LIBCXXABI_ENABLE_STATIC_UNWINDER), and sprt's
#                             longjmp is built on _Unwind_ForcedUnwind, so every
#                             executable carries a live copy. Details below.
#
# Exporting exactly those symbols from the executable fixes it: the dynamic linker
# searches the executable first, so every image in the process — including
# libc++abi.so's own PLT calls — binds to this copy.
#
# Each symbol needs two flags:
#   -u NAME                       pull the defining member out of libc++abi.a;
#                                 --export-dynamic-symbol never triggers archive
#                                 extraction by itself, and an export request for
#                                 a symbol that was never linked in is a no-op
#   --export-dynamic-symbol=NAME  put it into .dynsym
#
# Deliberately NOT exported:
#
#   * _ZNSt* / _ZSt* other than the handler entry points below (libc++ template
#     instantiations). The runtime is compiled -fno-exceptions, which changes the
#     bodies of inline libc++ code; interposing those into an exception-enabled .so
#     would replace its throw paths with aborts.
#   * __cxa_throw / __cxa_begin_catch / __cxa_end_catch / __gxx_personality_v0.
#     Exception handling is all-or-nothing: a process that takes __cxa_throw from
#     one image and __cxa_begin_catch from another corrupts the per-thread
#     __cxa_eh_globals (uncaughtExceptions under/overflow, cross-copy
#     caughtExceptions). Nothing here throws, so the whole group is left to the
#     .so's own libc++abi.so.
#   * __cxa_bad_cast / __cxa_bad_typeid / __cxa_throw_bad_array_new_length. Same
#     reason seen from the other side: this libc++abi copy aborts where a normal
#     one throws, so exporting them would silently downgrade a plugin's
#     `catch (std::bad_cast &)` into an abort.
#   * the nothrow operator new/delete forms. clang gives them hidden visibility
#     under -fvisibility=hidden (only the core replaceable set is exempt), so they
#     cannot be exported at all — and need not be: libc++abi's nothrow forms
#     forward to ::operator new / ::operator delete through the global scope, which
#     the core exports below already capture.
#
# The unwinder is exported even though the __cxa_* exception group is not, and the
# distinction is deliberate. They are different layers with different shared state:
#
#   * __cxa_throw / __cxa_begin_catch share the per-thread __cxa_eh_globals AND the
#     heap-allocated __cxa_exception. Splitting them across images corrupts both,
#     and unifying them is only correct if the personality routine is unified too —
#     which cannot be done, because every image carries its own compiler-emitted
#     LSDAs and its own __gxx_personality_v0.
#   * the unwinder's shared state is the FDE table: __register_frame /
#     __deregister_frame register JIT-compiled code with ONE copy, and the other
#     copy is then blind to those frames. This is not hypothetical here — a Vulkan
#     driver with a shader JIT (mesa llvmpipe/lavapipe) registers frames at
#     runtime, and an executable that walks its own stack through a driver callback
#     would stop dead at the first JIT frame.
#
# Unifying the unwinder alone IS coherent, because _Unwind_Context is opaque: a
# foreign personality routine touches it only through _Unwind_Get*/_Unwind_Set*,
# and those now come from the same copy that built the context. Today the two
# copies interoperate only because both are the same LLVM build — the moment a
# plugin ships a differently versioned libunwind (or libgcc's), that stops being
# true. Exporting the group makes it a contract instead of a coincidence.
#
# One caveat worth knowing: glibc reaches the unwinder through
# dlopen("libgcc_s.so.1") + dlsym(handle, ...), and dlsym on a handle searches that
# object and its dependencies, NOT the global scope. So pthread_exit,
# pthread_cancel and backtrace() keep using whatever libgcc_s.so.1 pulls in. That
# path only does forced unwinds through frames with cleanups, so it does not touch
# the FDE table this export unifies.
#
# Android reuses this preset but has a different C++ runtime layout (no
# -l:libc++abi.a), so it is excluded. Set SPRT_EXPORT_CXX_ABI=0 to opt out.
#

SPRT_EXPORT_CXX_ABI ?= 1

ifeq ($(TARGET_SYSTEM),Linux)
ifeq ($(SPRT_EXPORT_CXX_ABI),1)

OSTYPE_COMMA := ,

# size_t mangling: 'm' (unsigned long) on LP64, 'j' (unsigned int) on ILP32
ifneq ($(filter $(TARGET_ARCH),i386 i686 x86 arm armv7a armv7 mips),)
OSTYPE_CXX_ABI_Z := j
else
OSTYPE_CXX_ABI_Z := m
endif

# One-time initialisation guards
OSTYPE_CXX_ABI_EXPORTS := \
	__cxa_guard_acquire \
	__cxa_guard_release \
	__cxa_guard_abort

# dynamic_cast walker + the type_info vtables its virtual dispatch goes through.
#
# OFF BY DEFAULT, and the reason is measured, not theoretical. Exporting these
# hijacks the vptr of EVERY type_info in the process, including in images that use
# a different C++ ABI library. libstdc++'s __gxx_personality_v0 does virtual
# dispatch on type_info (__do_catch / __do_upcast) while matching a catch clause,
# and libstdc++'s vtable layout is not libc++abi's — so a g++-built plugin
# segfaults on its OWN `catch`, with no exception ever crossing the boundary.
# Verified with a libstdc++ plugin against tests/libc: throw/catch inside the
# plugin dies at the catch with these exported, passes without them.
#
# Turn it on only when every image in the process is known to use libc++abi (the
# Xenolith OS image is such a case: the driver is built by clang against this
# sysroot). Then it does what the note above describes — pairs the cast walker
# with the vtables it dispatches through.
#
# Note what this does NOT lose when off: each image keeps its own walker and its
# own vtables, which is self-consistent. The pairing only matters when one image's
# type_info is walked by another image's __dynamic_cast, and that needs the two
# libc++abi copies to differ in vtable layout — i.e. different LLVM versions.
SPRT_EXPORT_TYPEINFO ?= 0
ifeq ($(SPRT_EXPORT_TYPEINFO),1)
OSTYPE_CXX_ABI_EXPORTS += \
	__dynamic_cast \
	_ZTVN10__cxxabiv116__shim_type_infoE \
	_ZTVN10__cxxabiv117__class_type_infoE \
	_ZTVN10__cxxabiv120__si_class_type_infoE \
	_ZTVN10__cxxabiv121__vmi_class_type_infoE \
	_ZTVN10__cxxabiv117__pbase_type_infoE \
	_ZTVN10__cxxabiv119__pointer_type_infoE \
	_ZTVN10__cxxabiv129__pointer_to_member_type_infoE \
	_ZTVN10__cxxabiv123__fundamental_type_infoE \
	_ZTVN10__cxxabiv117__array_type_infoE \
	_ZTVN10__cxxabiv120__function_type_infoE \
	_ZTVN10__cxxabiv116__enum_type_infoE
endif

# Process-wide handler storage and its accessors. The storage objects go with the
# accessors: sprt's operator new reads the new-handler through std::get_new_handler,
# and libc++abi's __cxa_call_terminate reaches std::terminate through the PLT.
OSTYPE_CXX_ABI_EXPORTS += \
	__cxa_new_handler \
	__cxa_terminate_handler \
	_ZSt15set_new_handlerPFvvE \
	_ZSt15get_new_handlerv \
	_ZSt13set_terminatePFvvE \
	_ZSt13get_terminatev \
	_ZSt9terminatev

# Core replaceable allocation functions (the derived nothrow forms route through
# these via the global scope, see the note above)
OSTYPE_CXX_ABI_EXPORTS += \
	_Znw$(OSTYPE_CXX_ABI_Z) \
	_Zna$(OSTYPE_CXX_ABI_Z) \
	_Znw$(OSTYPE_CXX_ABI_Z)St11align_val_t \
	_Zna$(OSTYPE_CXX_ABI_Z)St11align_val_t \
	_ZdlPv \
	_ZdaPv \
	_ZdlPv$(OSTYPE_CXX_ABI_Z) \
	_ZdaPv$(OSTYPE_CXX_ABI_Z) \
	_ZdlPvSt11align_val_t \
	_ZdaPvSt11align_val_t \
	_ZdlPv$(OSTYPE_CXX_ABI_Z)St11align_val_t \
	_ZdaPv$(OSTYPE_CXX_ABI_Z)St11align_val_t

# Export-only, no -u: on gnu the entry points are defined by the runtime itself
# (runtime/core/runtime_core_setjmp.cpp brokers them onto whatever
# dlopen("libgcc_s.so.1") resolves), so they are always linked in and there is
# nothing to pull out of an archive. Elsewhere libc++abi.a still carries the
# unwinder and the set is arch-dependent — _Unwind_Find_FDE and __register_frame
# are DWARF-specific and absent on ARM EHABI. -u on a symbol nothing defines is a
# link error; --export-dynamic-symbol on one is a silent no-op, which is exactly
# the degradation we want.
SPRT_EXPORT_UNWINDER ?= 1

OSTYPE_CXX_ABI_EXPORTS_ONLY := \
	_Unwind_RaiseException \
	_Unwind_Resume \
	_Unwind_Resume_or_Rethrow \
	_Unwind_ForcedUnwind \
	_Unwind_DeleteException \
	_Unwind_Backtrace \
	_Unwind_GetGR \
	_Unwind_SetGR \
	_Unwind_GetIP \
	_Unwind_SetIP \
	_Unwind_GetIPInfo \
	_Unwind_GetCFA \
	_Unwind_GetLanguageSpecificData \
	_Unwind_GetRegionStart \
	_Unwind_GetDataRelBase \
	_Unwind_GetTextRelBase \
	_Unwind_FindEnclosingFunction \
	_Unwind_Find_FDE \
	__register_frame \
	__deregister_frame

OSTYPE_EXEC_LDFLAGS += $(foreach sym,$(OSTYPE_CXX_ABI_EXPORTS),\
	-Wl$(OSTYPE_COMMA)-u$(OSTYPE_COMMA)$(sym) \
	-Wl$(OSTYPE_COMMA)--export-dynamic-symbol=$(sym))

ifeq ($(SPRT_EXPORT_UNWINDER),1)
OSTYPE_EXEC_LDFLAGS += $(foreach sym,$(OSTYPE_CXX_ABI_EXPORTS_ONLY),\
	-Wl$(OSTYPE_COMMA)--export-dynamic-symbol=$(sym))
endif

endif # ifeq ($(SPRT_EXPORT_CXX_ABI),1)
endif # ifeq ($(TARGET_SYSTEM),Linux)

ifeq ($(ASAN),1)
	OSTYPE_GENERAL_CFLAGS += -fsanitize=address -shared-libasan
	OSTYPE_GENERAL_CXXFLAGS += -fsanitize=address -shared-libasan
	OSTYPE_EXEC_LDFLAGS += -fsanitize=address -shared-libasan
endif

ifdef BUILD_SHARED

OSTYPE_LIB_LDFLAGS += -Wl,-z,defs

endif # BUILD_SHARED

LINUX := 1