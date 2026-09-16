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

// Out-of-line bodies for the sprt-owned part of <exception>. Everything here is in
// the canonical namespace std (no versioned/inline namespace) so the symbols carry
// the standard _ZSt... mangling — required because on wasm libc++abi is compiled
// from source against these headers and must interoperate with the compiler/unwinder.
//
// Ownership is split by what the platform's ABI library already provides
// (<sprt/cxx/exception> documents the full map):
//
//   * std::exception_ptr / current_exception / rethrow_exception / make_exception_ptr
//     / nested_exception / uncaught_exception[s] are libc++ (NOT libc++abi) symbols.
//     libc++ is never built in this runtime, so sprt owns them on EVERY target.
//
//   * The terminate / unexpected handler family lives in libc++abi (cxa_handlers.cpp)
//     on every target that links it (linux/macos/android prebuilt, wasm from source).
//     sprt therefore defines it ONLY on the MSVC ABI, where there is no libc++abi
//     (guarded by SPRT_WINDOWS below). std::exception / std::bad_exception are the
//     same story and come from windows/libcxx.cc there.
//
// The runtime is built without C++ exceptions, so an exception_ptr can never be
// rethrown into a catch. It therefore holds a refcounted COPY of the exception
// object (so make_exception_ptr, comparison, null semantics and lifetime all work
// correctly) and terminates on rethrow — the honest maximum for a no-EH platform,
// identical on every target and with no dependency on __cxa_*.

#define __SPRT_BUILD 1

#include <sprt/cxx/exception>

#if !SPRT_WINDOWS
// libc++abi provides these Itanium ABI primitives; the std uncaught_exception[s]
// wrappers forward to them.
extern "C" bool __cxa_uncaught_exception() noexcept;
extern "C" unsigned int __cxa_uncaught_exceptions() noexcept;
#endif

namespace std {

// =========================================================================
//  exception_ptr / current_exception / rethrow_exception / nested_exception
//  make_exception_ptr / swap / uncaught_exception[s]  — owned by sprt on all
//  targets (these are libc++ symbols, never provided by libc++abi). Always in
//  the versioned inline namespace __sprt (unified ABI, every target) —
//  __SPRT_STD_OWNED_* from <sprt/c/bits/__sprt_config.h>.
// =========================================================================

__SPRT_STD_OWNED_BEGIN

namespace {

// Refcounted holder for one copied exception object. The block and the object
// share a single allocation: the object is placed after the header, bumped up to
// its required alignment (the extra `__align` bytes reserved below guarantee the
// bump always fits).
struct __ex_block {
	long __ref;
	void (*__destroy)(void *); // runs the stored object's destructor
	void *__obj; // aligned pointer to the stored object
};

inline __SIZE_TYPE__ __align_up(__SIZE_TYPE__ __v, __SIZE_TYPE__ __a) {
	return (__v + __a - 1) & ~(__a - 1);
}

} // namespace

exception_ptr __make_exception_ptr(__SIZE_TYPE__ __size, __SIZE_TYPE__ __align, void *__src,
		void (*__copy)(void *, void *), void (*__destroy)(void *)) noexcept {
	if (__align < 1) {
		__align = 1;
	}
	// header + alignment slack + object payload
	void *__raw = __builtin_malloc(sizeof(__ex_block) + __align + __size);
	if (!__raw) {
		std::terminate();
	}

	auto __b = static_cast<__ex_block *>(__raw);
	__b->__ref = 1;
	__b->__destroy = __destroy;
	__b->__obj = reinterpret_cast<void *>(
			__align_up(reinterpret_cast<__SIZE_TYPE__>(__b) + sizeof(__ex_block), __align));

	__copy(__b->__obj, __src);
	return exception_ptr(__raw);
}

#if SPRT_WINDOWS
namespace {
void __ex_noop_destroy(void *) noexcept { }
} // namespace

// MSVC-ABI make_exception_ptr path: libc++'s make_exception_ptr(e) lowers to
// __copy_exception_ptr(&e, __GetExceptionInfo(e)) on _LIBCPP_ABI_MICROSOFT, where __ptr is
// the MSVC _ThrowInfo. In the no-EH runtime a stored exception can never be handed to a
// catch (rethrow_exception terminates), so the copied object is never observable; the
// result only needs to be non-null with correct refcount/lifetime semantics. Parsing the
// _ThrowInfo to copy the object would add no observable behaviour, so allocate a minimal
// owning block with a no-op destroy — the honest maximum for a platform without EH.
exception_ptr __copy_exception_ptr(void *__except, const void *__ptr) {
	(void)__except;
	(void)__ptr;
	void *__raw = __builtin_malloc(sizeof(__ex_block));
	if (!__raw) {
		std::terminate();
	}
	auto __b = static_cast<__ex_block *>(__raw);
	__b->__ref = 1;
	__b->__destroy = &__ex_noop_destroy;
	__b->__obj = nullptr;
	return exception_ptr(__raw);
}
#endif // SPRT_WINDOWS

exception_ptr::exception_ptr(const exception_ptr &__other) noexcept : __ptr_(__other.__ptr_) {
	if (__ptr_) {
		__atomic_add_fetch(&static_cast<__ex_block *>(__ptr_)->__ref, 1L, __ATOMIC_ACQ_REL);
	}
}

exception_ptr &exception_ptr::operator=(const exception_ptr &__other) noexcept {
	// Acquire the new reference before releasing the old (self-assignment safe).
	void *__new = __other.__ptr_;
	if (__new) {
		__atomic_add_fetch(&static_cast<__ex_block *>(__new)->__ref, 1L, __ATOMIC_ACQ_REL);
	}
	void *__old = __ptr_;
	__ptr_ = __new;
	if (__old && __atomic_sub_fetch(&static_cast<__ex_block *>(__old)->__ref, 1L, __ATOMIC_ACQ_REL) == 0) {
		auto __b = static_cast<__ex_block *>(__old);
		__b->__destroy(__b->__obj);
		__builtin_free(__b);
	}
	return *this;
}

exception_ptr &exception_ptr::operator=(decltype(nullptr)) noexcept {
	return *this = exception_ptr();
}

exception_ptr::~exception_ptr() noexcept {
	if (__ptr_
			&& __atomic_sub_fetch(&static_cast<__ex_block *>(__ptr_)->__ref, 1L, __ATOMIC_ACQ_REL)
					== 0) {
		auto __b = static_cast<__ex_block *>(__ptr_);
		__b->__destroy(__b->__obj);
		__builtin_free(__b);
	}
}

#if SPRT_WINDOWS
// The default ctor, the nullptr ctor, operator bool and operator== are inline in
// <sprt/cxx/exception> on every non-MSVC target; on Windows they are declared out-of-line
// so this one set of definitions also backs libc++'s <exception> exception_ptr — the
// primary std::__sprt::exception_ptr there, whose two-pointer MS-ABI layout declares the
// same members out-of-line and emits references to them. (An inline definition is emitted
// weakly in debug but inlined away at -O2, so RELEASE links would report these three as
// undefined symbols.) All read/init only the offset-0 control-block pointer, shared by the
// sprt single-pointer and the libc++ two-pointer layouts.
exception_ptr::exception_ptr() noexcept : __ptr_(nullptr) { }
exception_ptr::exception_ptr(decltype(nullptr)) noexcept : __ptr_(nullptr) { }
exception_ptr::operator bool() const noexcept { return __ptr_ != nullptr; }
bool operator==(const exception_ptr &__x, const exception_ptr &__y) noexcept {
	return __x.__ptr_ == __y.__ptr_;
}
#endif

void swap(exception_ptr &__x, exception_ptr &__y) noexcept {
	void *__t = __x.__ptr_;
	__x.__ptr_ = __y.__ptr_;
	__y.__ptr_ = __t;
}

// No C++ exception can be in flight in the no-EH runtime.
exception_ptr current_exception() noexcept { return exception_ptr(); }

// There is no unwinder to hand the exception to a catch clause, so the only
// correct behaviour on rethrow is to terminate.
void rethrow_exception(exception_ptr) { std::terminate(); }

nested_exception::nested_exception() noexcept : __ptr_(std::current_exception()) { }

nested_exception::~nested_exception() noexcept { }

void nested_exception::rethrow_nested() const {
	if (__ptr_ == nullptr) {
		std::terminate();
	}
	std::rethrow_exception(__ptr_);
}

#if SPRT_WINDOWS
// On the MSVC ABI there is no libc++abi, so the counters have no backing runtime.
bool uncaught_exception() noexcept { return false; }
int uncaught_exceptions() noexcept { return 0; }
#else
bool uncaught_exception() noexcept { return __cxa_uncaught_exception(); }
int uncaught_exceptions() noexcept { return static_cast<int>(__cxa_uncaught_exceptions()); }
#endif

__SPRT_STD_OWNED_END

// =========================================================================
//  terminate / unexpected handler family — sprt owns this ONLY on the MSVC
//  ABI (canonical std); elsewhere libc++abi (cxa_handlers.cpp) supplies the
//  canonical bodies.
// =========================================================================

#if SPRT_WINDOWS

namespace {

[[noreturn]] void __default_terminate_handler() { __builtin_abort(); }
[[noreturn]] void __default_unexpected_handler() { std::terminate(); }

terminate_handler s_terminate_handler = &__default_terminate_handler;
unexpected_handler s_unexpected_handler = &__default_unexpected_handler;

} // namespace

terminate_handler set_terminate(terminate_handler __h) noexcept {
	return __atomic_exchange_n(&s_terminate_handler, __h, __ATOMIC_ACQ_REL);
}

terminate_handler get_terminate() noexcept {
	return __atomic_load_n(&s_terminate_handler, __ATOMIC_ACQUIRE);
}

unexpected_handler set_unexpected(unexpected_handler __h) noexcept {
	return __atomic_exchange_n(&s_unexpected_handler, __h, __ATOMIC_ACQ_REL);
}

unexpected_handler get_unexpected() noexcept {
	return __atomic_load_n(&s_unexpected_handler, __ATOMIC_ACQUIRE);
}

void __terminate(terminate_handler __h) noexcept {
	// A well-behaved handler does not return; abort if it does (or if unset).
	if (__h) {
		__h();
	}
	__builtin_abort();
}

void terminate() noexcept { std::__terminate(std::get_terminate()); }

void __unexpected(unexpected_handler __h) {
	if (__h) {
		__h();
	}
	std::terminate();
}

void unexpected() { std::__unexpected(std::get_unexpected()); }

#endif // SPRT_WINDOWS

} // namespace std
