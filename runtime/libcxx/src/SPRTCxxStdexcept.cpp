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

// Out-of-line bodies for the vendored libc++ <stdexcept> (the nine exception classes
// and the __libcpp_refstring message store, in libc++abi's Itanium ABI layout), PLUS
// the small exception "throwers" that would otherwise each need their own vendored TU:
// bad_optional_access, bad_variant_access, bad_any_cast, bad_expected_access,
// bad_function_call (+ __hash_memory), and vector's legacy __vector_base_common.
// Folding them here keeps the libcxx module from carrying five extra one-symbol TUs.
//
// Two namespaces are in play (see <sprt/cxx/exception>):
//   * refstring, __throw_runtime_error, __hash_memory and __vector_base_common are
//     ordinary libc++ symbols -> the versioned namespace std::__sprt.
//   * the EXPORTED_FROM_ABI exception classes and the bad_* throwers derive from
//     std::exception and must interoperate with libc++abi -> the CANONICAL namespace
//     std (__SPRT_STD_OWNED_BEGIN: the versioned namespace on every target — unified ABI).
//
// Compiled against the vendored libc++ headers (libcxx/include) with _LIBCPP_BUILDING_LIBRARY
// so the classes' key functions (~/what) are emitted out-of-line, exactly as the upstream
// src TUs do. The refstring uses __builtin_malloc/free rather than the freestanding-
// deprecated ::operator new; __uses_refcount() is always true (the GCC empty-string
// interop is an Apple-libstdc++ concern that does not apply here).

#define _LIBCPP_BUILDING_LIBRARY

#include <cstddef>
#include <stdexcept>
#include <string>
#include <optional>
#include <variant>
#include <any>
#include <functional>
#include <vector>
#include <__verbose_abort>
#if _LIBCPP_STD_VER >= 23
#include <expected>
#endif

// ===========================================================================
// Versioned std::__sprt: refstring + free throw/hash helpers + vector legacy.
// ===========================================================================
_LIBCPP_BEGIN_NAMESPACE_STD

// The single pointer member must alias a plain `const char*` for ABI parity.
static_assert(sizeof(std::__libcpp_refstring) == sizeof(const char *), "");

namespace {

typedef int count_t;

struct _Rep_base {
	size_t len;
	size_t cap;
	count_t count;
};

inline _Rep_base *rep_from_data(const char *__data) noexcept {
	char *__d = const_cast<char *>(__data);
	return reinterpret_cast<_Rep_base *>(__d - sizeof(_Rep_base));
}

inline char *data_from_rep(_Rep_base *__rep) noexcept {
	return reinterpret_cast<char *>(__rep) + sizeof(*__rep);
}

// The refcount is manipulated with sequentially-consistent atomics (a message string
// may be shared across threads); atomic_add returns the *new* value.
inline count_t atomic_add(count_t *__p, count_t __delta) noexcept {
	return __atomic_add_fetch(__p, __delta, __ATOMIC_SEQ_CST);
}

} // namespace

__libcpp_refstring::__libcpp_refstring(const char *__msg) {
	size_t __len = __builtin_strlen(__msg);
	_Rep_base *__rep = static_cast<_Rep_base *>(__builtin_malloc(sizeof(_Rep_base) + __len + 1));
	__rep->len = __len;
	__rep->cap = __len;
	__rep->count = 0;
	char *__data = data_from_rep(__rep);
	__builtin_memcpy(__data, __msg, __len + 1);
	__imp_ = __data;
}

__libcpp_refstring::__libcpp_refstring(const __libcpp_refstring &__s) noexcept
: __imp_(__s.__imp_) {
	if (__uses_refcount()) {
		atomic_add(&rep_from_data(__imp_)->count, 1);
	}
}

__libcpp_refstring &__libcpp_refstring::operator=(const __libcpp_refstring &__s) noexcept {
	bool __adjust_old = __uses_refcount();
	_Rep_base *__old_rep = rep_from_data(__imp_);
	__imp_ = __s.__imp_;
	if (__uses_refcount()) {
		atomic_add(&rep_from_data(__imp_)->count, 1);
	}
	if (__adjust_old) {
		if (atomic_add(&__old_rep->count, count_t(-1)) < 0) {
			__builtin_free(__old_rep);
		}
	}
	return *this;
}

__libcpp_refstring::~__libcpp_refstring() {
	if (__uses_refcount()) {
		_Rep_base *__rep = rep_from_data(__imp_);
		if (atomic_add(&__rep->count, count_t(-1)) < 0) {
			__builtin_free(__rep);
		}
	}
}

bool __libcpp_refstring::__uses_refcount() const { return true; }

void __throw_runtime_error(const char *__msg) {
#if _LIBCPP_HAS_EXCEPTIONS
	throw runtime_error(__msg);
#else
	_LIBCPP_VERBOSE_ABORT("runtime_error was thrown in -fno-exceptions mode with message \"%s\"",
			__msg);
#endif
}

// __hash_memory lives in functional.cpp upstream (the hashing backend for hash<T>).
size_t __hash_memory(_LIBCPP_NOESCAPE const void *__ptr, size_t __size) noexcept {
	return __murmur2_or_cityhash<size_t>()(__ptr, __size);
}

// vector's legacy out-of-line throw helpers (kept for ABI; the struct is no longer in
// the headers). Guarded exactly as vector.cpp.
#ifndef _LIBCPP_ABI_DO_NOT_EXPORT_VECTOR_BASE_COMMON
template <bool>
struct __vector_base_common;

template <>
struct __vector_base_common<true> {
	[[noreturn]]
	_LIBCPP_EXPORTED_FROM_ABI void __throw_length_error() const;
	[[noreturn]]
	_LIBCPP_EXPORTED_FROM_ABI void __throw_out_of_range() const;
};

void __vector_base_common<true>::__throw_length_error() const {
	std::__throw_length_error("vector");
}
void __vector_base_common<true>::__throw_out_of_range() const {
	std::__throw_out_of_range("vector");
}
#endif

_LIBCPP_END_NAMESPACE_STD

// ===========================================================================
// The EXPORTED_FROM_ABI exception classes and throwers. Versioned (std::__sprt):
// the sprt runtime must not define symbols in the canonical namespace, so a
// system libc++/libstdc++ in the same process can never be interposed by us.
// The vendored headers declare these classes versioned to match.
// ===========================================================================
_LIBCPP_BEGIN_NAMESPACE_STD

// [stdexcept] the nine standard exception classes.
logic_error::logic_error(const char *__msg) : __imp_(__msg) { }
logic_error::logic_error(const string &__s) : __imp_(__s.c_str()) { }
logic_error::logic_error(const logic_error &__le) noexcept : __imp_(__le.__imp_) { }
logic_error &logic_error::operator=(const logic_error &__le) noexcept {
	__imp_ = __le.__imp_;
	return *this;
}
const char *logic_error::what() const noexcept { return __imp_.c_str(); }

runtime_error::runtime_error(const char *__msg) : __imp_(__msg) { }
runtime_error::runtime_error(const string &__s) : __imp_(__s.c_str()) { }
runtime_error::runtime_error(const runtime_error &__re) noexcept : __imp_(__re.__imp_) { }
runtime_error &runtime_error::operator=(const runtime_error &__re) noexcept {
	__imp_ = __re.__imp_;
	return *this;
}
const char *runtime_error::what() const noexcept { return __imp_.c_str(); }

logic_error::~logic_error() noexcept { }
domain_error::~domain_error() noexcept { }
invalid_argument::~invalid_argument() noexcept { }
length_error::~length_error() noexcept { }
out_of_range::~out_of_range() noexcept { }

runtime_error::~runtime_error() noexcept { }
range_error::~range_error() noexcept { }
overflow_error::~overflow_error() noexcept { }
underflow_error::~underflow_error() noexcept { }

// Small "throwers", folded from optional/variant/any/functional/expected.cpp. Only the
// key functions (~/what) are out-of-line; the __throw_* helpers are inline in headers.
bad_optional_access::~bad_optional_access() noexcept = default;
const char *bad_optional_access::what() const noexcept { return "bad_optional_access"; }

const char *bad_variant_access::what() const noexcept { return "bad_variant_access"; }

const char *bad_any_cast::what() const noexcept { return "bad any cast"; }

bad_function_call::~bad_function_call() noexcept { }
const char *bad_function_call::what() const noexcept { return "std::bad_function_call"; }

#if _LIBCPP_STD_VER >= 23
const char *bad_expected_access<void>::what() const noexcept {
	return "bad access to std::expected";
}
#endif

_LIBCPP_END_NAMESPACE_STD
