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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_VCRUNTIME_TYPEINFO_H_
#define CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_VCRUNTIME_TYPEINFO_H_

#include <sprt/runtime/init.h>

struct __type_info_node;

extern __type_info_node __type_info_root_node;

__SPRT_BEGIN_DECL

struct __std_type_info_data {
	const char *_UndecoratedName;
	const char _DecoratedName[1];
	__std_type_info_data() = delete;
	__std_type_info_data(const __std_type_info_data &) = delete;
	__std_type_info_data(__std_type_info_data &&) = delete;

	__std_type_info_data &operator=(const __std_type_info_data &) = delete;
	__std_type_info_data &operator=(__std_type_info_data &&) = delete;
};

int __cdecl __std_type_info_compare(const __std_type_info_data *_Lhs,
		const __std_type_info_data *_Rhs);

size_t __cdecl __std_type_info_hash(const __std_type_info_data *_Data);

const char *__cdecl __std_type_info_name(__std_type_info_data *_Data, __type_info_node *_RootNode);

__SPRT_END_DECL


SPRT_API class type_info {
public:
	type_info(const type_info &) = delete;
	type_info &operator=(const type_info &) = delete;

	[[nodiscard]]
	size_t hash_code() const noexcept {
		return __std_type_info_hash(&_Data);
	}

	[[nodiscard]]
	constexpr bool operator==(const type_info &_Other) const noexcept {
		if (__builtin_is_constant_evaluated()) {
			return &_Data == &_Other._Data;
		}

		return __std_type_info_compare(&_Data, &_Other._Data) == 0;
	}

	[[nodiscard]]
	bool before(const type_info &_Other) const noexcept {
		return __std_type_info_compare(&_Data, &_Other._Data) < 0;
	}

	[[nodiscard]]
	const char *name() const noexcept {
		return __std_type_info_name(&_Data, &__type_info_root_node);
	}

	[[nodiscard]]
	const char *raw_name() const noexcept {
		return _Data._DecoratedName;
	}

	virtual ~type_info() noexcept;

private:
	mutable __std_type_info_data _Data;
};

namespace std {
SPRT_API using ::type_info;
}

#endif // CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_VCRUNTIME_TYPEINFO_H_
