/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#include <stdio.h>
#include <stdlib.h>
#include <sprt/cxx/new>

#if SPRT_WINDOWS
#define llvm sprt_demangle
#include "windows/libcxx.cc"
#include "windows/Demangle/MicrosoftDemangle.cc"
#include "windows/Demangle/MicrosoftDemangleNodes.cc"
#undef llvm
#endif

// std::align_val_t now comes from <sprt/cxx/new> (declared directly in namespace
// std so aligned new-expressions mangle against the real std::align_val_t).

// -------------------------------------------------------------------------
// [alloc.errors] std::set_new_handler / std::get_new_handler
//
// Where libc++abi is built from source it owns these (cxa_handlers.cpp /
// cxa_default_handlers.cpp), so defining them here too would be a duplicate
// symbol -- that is wasm and Embox EL0. Elsewhere (the freestanding MSVC ABI, no
// libc++abi) sprt provides the single new-handler storage.
#if !SPRT_WASM && !SPRT_EMBOX_USER
namespace std {
static void *__sprt_new_handler = nullptr;

new_handler set_new_handler(new_handler __h) noexcept {
	return reinterpret_cast<new_handler>(__atomic_exchange_n(&__sprt_new_handler,
			reinterpret_cast<void *>(__h), __ATOMIC_SEQ_CST));
}
new_handler get_new_handler() noexcept {
	return reinterpret_cast<new_handler>(__atomic_load_n(&__sprt_new_handler, __ATOMIC_SEQ_CST));
}
} // namespace std
#endif // !SPRT_WASM && !SPRT_EMBOX_USER

__SPRT_C_FUNC void (*__sprt_get_new_handler(void))(void) {
	return reinterpret_cast<void (*)(void)>(std::get_new_handler());
}

#if SPRT_WINDOWS || SPRT_WASM || SPRT_EMBOX_USER

// Full replaceable operator new/delete set over mimalloc's typed API. mimalloc is
// the standard allocator on these targets (libc_impl/malloc.mk); its SCU is
// compiled as C, so mi_new runs the new-handler retry loop and aborts on final OOM
// instead of throwing — the correct behaviour for the freestanding, no-exceptions
// ABI. Routing every delete through mi_free_size / mi_free_aligned hands mimalloc
// the exact block size and alignment so it can skip the pointer→page lookup.
extern "C" {
void *mi_new(size_t size);
void *mi_new_aligned(size_t size, size_t alignment);
void *mi_new_nothrow(size_t size) noexcept;
void *mi_new_aligned_nothrow(size_t size, size_t alignment) noexcept;
void mi_free(void *p) noexcept;
void mi_free_size(void *p, size_t size) noexcept;
void mi_free_aligned(void *p, size_t alignment) noexcept;
void mi_free_size_aligned(void *p, size_t size, size_t alignment) noexcept;
}


#if defined(SPRT_WASM) && !defined(SPRT_WEAK_OPERATOR_NEW_DELETE)
#define SPRT_REPLACEABLE_OP
#elif defined(SPRT_BUILD_SHARED_RUNTIME)
#define SPRT_REPLACEABLE_OP
#else
#define SPRT_REPLACEABLE_OP __attribute__((weak))
#endif

// throwing new / new[]. These plus the over-aligned forms and the plain / aligned
// deletes are the CORE replaceable operators (leaf allocator calls). Every derived
// form below forwards to a core operator through the global scope, so replacing just
// a core operator (e.g. operator delete(void*)) is honoured by the sized / aligned
// variants the compiler actually emits — the behaviour the standard mandates and the
// new.delete/*.replace + sized_delete_calls_unsized_delete tests check.
// Core (leaf) throwing allocators: scalar plain + scalar over-aligned. These are the
// only forms that touch mimalloc directly (mi_new runs the new-handler retry loop and
// aborts on final OOM — correct for the no-EH ABI).
SPRT_REPLACEABLE_OP void *operator new(size_t n) { return mi_new(n); }
SPRT_REPLACEABLE_OP void *operator new(size_t n, std::align_val_t al) {
	return mi_new_aligned(n, sprt::to_underlying(al));
}
// Derived array forms: the default operator new[] must call the (possibly replaced)
// core scalar ::operator new through global scope — NOT mimalloc directly — so a
// program replacing only scalar operator new still has its array allocations routed
// through it. Symmetric with the derived delete forms below. Required by
// new.delete.array/new.size[.align].replace.indirect: on Windows sprt is the sole
// operator provider (no libc++abi weak set), so if new[] shortcuts to mi_new the
// scalar replacement is silently bypassed and new_called stays 0.
SPRT_REPLACEABLE_OP void *operator new[](size_t n) { return ::operator new(n); }
SPRT_REPLACEABLE_OP void *operator new[](size_t n, std::align_val_t al) {
	return ::operator new(n, al);
}

// nothrow new / new[]. The STANDARD replaceable signature takes const std::nothrow_t&
// (by reference, matching the nothrow delete operators below); sprt::nothrow_t IS that
// std::nothrow_t, so these also back sprt's own `new (sprt::nothrow) T` (which binds the
// nothrow lvalue to the reference). Route to mimalloc's nothrow allocator, which returns
// null on exhaustion — correct for the exception-free ABI.
SPRT_REPLACEABLE_OP void *operator new(size_t n, const sprt::nothrow_t &) noexcept {
	return mi_new_nothrow(n);
}
SPRT_REPLACEABLE_OP void *operator new[](size_t n, const sprt::nothrow_t &) noexcept {
	return mi_new_nothrow(n);
}
SPRT_REPLACEABLE_OP void *operator new(size_t n, std::align_val_t al,
		const sprt::nothrow_t &) noexcept {
	return mi_new_aligned_nothrow(n, sprt::to_underlying(al));
}
SPRT_REPLACEABLE_OP void *operator new[](size_t n, std::align_val_t al,
		const sprt::nothrow_t &) noexcept {
	return mi_new_aligned_nothrow(n, sprt::to_underlying(al));
}

// Core (leaf) deletes: scalar plain + scalar over-aligned → mimalloc.
SPRT_REPLACEABLE_OP void operator delete(void *p) noexcept { mi_free(p); }
SPRT_REPLACEABLE_OP void operator delete(void *p, std::align_val_t al) noexcept {
	mi_free_aligned(p, sprt::to_underlying(al));
}
// Derived array deletes: the default operator delete[] calls the (possibly replaced)
// scalar ::operator delete through global scope — symmetric with operator new[] above
// (and matching libc++abi's stdlib_new_delete). A program replacing only scalar
// operator delete must still have delete[] routed through it; new.delete.array/
// *.replace.indirect check exactly this (delete_called==1 after replacing scalar).
SPRT_REPLACEABLE_OP void operator delete[](void *p) noexcept { ::operator delete(p); }
SPRT_REPLACEABLE_OP void operator delete[](void *p, std::align_val_t al) noexcept {
	::operator delete(p, al);
}
// Derived forms: the default sized / sized+aligned deletes must call the (possibly
// replaced) core delete, NOT the allocator directly — otherwise replacing only
// operator delete(void*) is silently bypassed for sized deallocations.
SPRT_REPLACEABLE_OP void operator delete(void *p, size_t) noexcept { ::operator delete(p); }
SPRT_REPLACEABLE_OP void operator delete[](void *p, size_t) noexcept { ::operator delete[](p); }
SPRT_REPLACEABLE_OP void operator delete(void *p, size_t, std::align_val_t al) noexcept {
	::operator delete(p, al);
}
SPRT_REPLACEABLE_OP void operator delete[](void *p, size_t, std::align_val_t al) noexcept {
	::operator delete[](p, al);
}
SPRT_REPLACEABLE_OP void operator delete(void *p, std::align_val_t al,
		const sprt::nothrow_t &) noexcept {
	::operator delete(p, al);
}
SPRT_REPLACEABLE_OP void operator delete[](void *p, std::align_val_t al,
		const sprt::nothrow_t &) noexcept {
	::operator delete[](p, al);
}
// plain (non-aligned) nothrow placement delete / delete[]. The compiler emits these
// as the matching deallocation when a `new (nothrow) T` constructor throws; the no-EH
// runtime never reaches that, but the standard requires the operators to exist and the
// new.delete/new.size_nothrow tests take their address. Sibling of the aligned nothrow
// forms above — they were the oversight (present) that made these (absent) link-fail.
// Forward through global scope so a program replacing only the core delete still wins.
SPRT_REPLACEABLE_OP void operator delete(void *p, const sprt::nothrow_t &) noexcept {
	::operator delete(p);
}
SPRT_REPLACEABLE_OP void operator delete[](void *p, const sprt::nothrow_t &) noexcept {
	::operator delete[](p);
}

#undef SPRT_REPLACEABLE_OP

#else

void *operator new(size_t __blockLen) { return ::malloc(__blockLen); }

void *operator new[](size_t __blockLen) { return ::malloc(__blockLen); }

void *operator new(size_t __blockLen, std::align_val_t align) {
	// aligned_alloc(alignment, size) — alignment first, size second.
	return ::aligned_alloc(sprt::to_underlying(align), __blockLen);
}

void *operator new[](size_t __blockLen, std::align_val_t align) {
	return ::aligned_alloc(sprt::to_underlying(align), __blockLen);
}

void operator delete(void *ptr) noexcept { return ::free(ptr); }

void operator delete(void *ptr, size_t sz) noexcept { return ::free_sized(ptr, sz); }

void operator delete[](void *ptr) noexcept { return ::free(ptr); }

void operator delete[](void *ptr, size_t sz) noexcept { return ::free_sized(ptr, sz); }

// operator delete(void*, size_t, align_val_t) and its array form are provided
// inline by <sprt/cxx/new>.

#endif // SPRT_WINDOWS || SPRT_WASM || SPRT_EMBOX_USER

// MS C++ ABI function
__SPRT_C_FUNC void _purecall(void) {
// Platform-specific debug break
#ifdef _WIN32
	__debugbreak(); // Break into debugger
#else
	__builtin_debugtrap();
#endif
	fprintf(stderr, "Fatal: %s; aborting;\n", "Pure virtual function called");
	abort();
}

// General C++ ABI function. On wasm the linked libc++abi owns __cxa_pure_virtual,
// so the freestanding libc must not also define it (even weakly) — leave it to
// libc++abi to avoid duplicating a c++abi entry point.
#if !SPRT_WASM
__SPRT_C_FUNC __attribute__((weak)) void __cxa_pure_virtual(void) { _purecall(); }
#endif

#if SPRT_WASM
// RTTI failure paths. libc++abi's __cxa_bad_typeid / __cxa_bad_cast /
// __cxa_throw_bad_array_new_length THROW, which drags in the Itanium unwinder
// (_Unwind_RaiseException) that the wasm build does not provide (C++ exceptions
// are a later milestone). Any RTTI use (e.g. typeid(*this) in dispatch::Thread)
// references these, so define non-throwing, trapping versions here: for a valid
// object these paths are unreachable, and without EH a trap is the only correct
// behavior. Resolving the reference here prevents libc++abi's throwing versions
// (and cxa_exception.cpp) from being extracted.
__SPRT_C_FUNC void __cxa_bad_typeid(void) { __builtin_trap(); }
__SPRT_C_FUNC void __cxa_bad_cast(void) { __builtin_trap(); }
__SPRT_C_FUNC void __cxa_throw_bad_array_new_length(void) { __builtin_trap(); }
#endif
