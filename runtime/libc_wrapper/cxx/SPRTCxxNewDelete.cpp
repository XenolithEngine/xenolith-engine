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

// The single, non-inline definition of the global replaceable operator new/delete
// set for the platforms where sprt links the SYSTEM C++ ABI (linux/macos/android).
//
// Why this file exists: <sprt/cxx/new> only DECLARES the operators (no inline
// bodies — an inline replaceable operator is weak COMDAT and cannot reliably
// override the platform library). The runtime statically links libc++abi, which
// provides these operators as WEAK symbols; the strong definitions here override
// them when the runtime image (static archive or .so) is assembled, so EVERY
// new/delete in code that links the runtime is routed through the runtime's
// allocator — the same in all three deployment shapes (runtime static in the app,
// runtime as a .so, runtime + user code both .so). An sprt program uses sprt's own
// STL, so no second, competing operator new is present in the process.
//
// Windows and Wasm get the mimalloc-typed set from libc_impl/builtin_libcxx.cpp
// (there sprt fully owns the libc), so this TU is intentionally empty on them to
// avoid a duplicate definition.
//
// Opt-out: define SPRT_NO_STRONG_OPERATOR_NEW_DELETE to emit these definitions as
// WEAK instead of strong (they are still all defined). A program that provides its
// own replacement global operators then overrides the weak runtime copies with no
// duplicate-symbol link error, while a program that does not replace them still
// resolves to the runtime's copy. This is the switch the libc++ conformance suite
// needs: its allocation tests define their own operator new/delete (count_new.h,
// new.*.replace), which collide with the strong set the runtime installs by default.

#define __SPRT_BUILD 1

#include <sprt/cxx/new>

#if !(SPRT_WINDOWS || SPRT_WASM || SPRT_EMBOX_USER)

// The set splits into two groups under SPRT_NO_STRONG_OPERATOR_NEW_DELETE:
//
//  * The STANDARD replaceable operators (plain / sized / aligned new & delete,
//    mangled only with builtin and platform std types) are DROPPED. libc++abi
//    then supplies them — and, crucially, its sized/aligned forms forward to the
//    (possibly replaced) unsized `operator delete(void*)`, the forwarding a
//    program's own replacement relies on; the runtime's leaf definitions do not.
//  * The aligned nothrow-DELETE overloads are kept, but WEAK, so a non-replacing
//    program still resolves them while a program that replaces them overrides
//    without a clash. (The nothrow NEW forms use the standard const nothrow_t&
//    signature and live in the STANDARD group above — sprt::nothrow_t == std::nothrow_t,
//    so `new (sprt::nothrow)` binds to them / libc++abi's copy.)
//
// With the macro undefined (the default) both groups are STRONG, exactly as before.
#if defined(SPRT_NO_STRONG_OPERATOR_NEW_DELETE)
#define SPRT_STD_NEWDELETE 0
#define SPRT_NOTHROW_LINKAGE __attribute__((weak))
#else
#define SPRT_STD_NEWDELETE 1
#define SPRT_NOTHROW_LINKAGE
#endif

namespace {

// Over-aligned free: the runtime tracks whether an allocation came from the plain
// or the aligned path by alignment, mirroring <sprt/cxx/new>'s inline deleters.
inline void __sprt_delete_aligned(void *__ptr, sprt::align_val_t __al) noexcept {
	if (sprt::to_underlying(__al) <= alignof(__sprt_max_align_t)) {
		__sprt_free(__ptr);
	} else {
		__sprt_aligned_free(__ptr);
	}
}

} // namespace

// --- operator delete (aligned nothrow) -----------------------------------
// Kept out of the SPRT_STD_NEWDELETE group as a weak fallback (the exception-free
// runtime never actually throws from the matching aligned nothrow new, so a
// non-replacing program still needs these defined).
SPRT_NOTHROW_LINKAGE void operator delete(void *__p, sprt::align_val_t __al,
		const sprt::nothrow_t &) noexcept {
	__sprt_delete_aligned(__p, __al);
}
SPRT_NOTHROW_LINKAGE void operator delete[](void *__p, sprt::align_val_t __al,
		const sprt::nothrow_t &) noexcept {
	__sprt_delete_aligned(__p, __al);
}

#if SPRT_STD_NEWDELETE

// --- operator new (throwing) ---------------------------------------------
void *operator new(sprt::size_t __n) { return sprt::__new_with_handler(__n ? __n : 1, 0); }
void *operator new[](sprt::size_t __n) { return sprt::__new_with_handler(__n ? __n : 1, 0); }
void *operator new(sprt::size_t __n, sprt::align_val_t __al) {
	return sprt::__new_with_handler(__n ? __n : 1, sprt::to_underlying(__al));
}
void *operator new[](sprt::size_t __n, sprt::align_val_t __al) {
	return sprt::__new_with_handler(__n ? __n : 1, sprt::to_underlying(__al));
}

// --- operator new/delete (standard nothrow, const std::nothrow_t&) --------
// The STANDARD replaceable nothrow forms. Without these, `new (std::nothrow) T`
// in libc++-built code resolves to libc++abi's weak stubs, which — when the plain
// operator new IS overridden (by this very TU) and exceptions are disabled —
// abort with a "must override operator new(nothrow) as well" diagnostic. Like the
// plain forms above, they go straight to the runtime allocator (which already has
// null-on-exhaustion semantics in the exception-free runtime).
// sprt::nothrow_t is the platform std::nothrow_t, so these mangle canonically.
void *operator new(sprt::size_t __n, const sprt::nothrow_t &) noexcept {
	return sprt::__new_with_handler(__n ? __n : 1, 0);
}
void *operator new[](sprt::size_t __n, const sprt::nothrow_t &) noexcept {
	return sprt::__new_with_handler(__n ? __n : 1, 0);
}
void *operator new(sprt::size_t __n, sprt::align_val_t __al, const sprt::nothrow_t &) noexcept {
	return sprt::__new_with_handler(__n ? __n : 1, sprt::to_underlying(__al));
}
void *operator new[](sprt::size_t __n, sprt::align_val_t __al, const sprt::nothrow_t &) noexcept {
	return sprt::__new_with_handler(__n ? __n : 1, sprt::to_underlying(__al));
}
void operator delete(void *__p, const sprt::nothrow_t &) noexcept { __sprt_free(__p); }
void operator delete[](void *__p, const sprt::nothrow_t &) noexcept { __sprt_free(__p); }

// --- operator delete ------------------------------------------------------
void operator delete(void *__p) noexcept { __sprt_free(__p); }
void operator delete[](void *__p) noexcept { __sprt_free(__p); }
void operator delete(void *__p, sprt::size_t __n) noexcept { __sprt_free_sized(__p, __n); }
void operator delete[](void *__p, sprt::size_t __n) noexcept { __sprt_free_sized(__p, __n); }
void operator delete(void *__p, sprt::align_val_t __al) noexcept {
	__sprt_delete_aligned(__p, __al);
}
void operator delete[](void *__p, sprt::align_val_t __al) noexcept {
	__sprt_delete_aligned(__p, __al);
}
void operator delete(void *__p, sprt::size_t, sprt::align_val_t __al) noexcept {
	__sprt_delete_aligned(__p, __al);
}
void operator delete[](void *__p, sprt::size_t, sprt::align_val_t __al) noexcept {
	__sprt_delete_aligned(__p, __al);
}

#endif // SPRT_STD_NEWDELETE

#undef SPRT_STD_NEWDELETE
#undef SPRT_NOTHROW_LINKAGE

#endif // !(SPRT_WINDOWS || SPRT_WASM || SPRT_EMBOX_USER)
