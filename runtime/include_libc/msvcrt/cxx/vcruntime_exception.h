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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_VCRUNTIME_EXCEPTION_H_
#define CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_VCRUNTIME_EXCEPTION_H_

#include <sprt/runtime/init.h>

__SPRT_BEGIN_DECL

struct __std_exception_data {
	char const *_What;
	bool _DoFree;
};

void __cdecl __std_exception_copy(__std_exception_data const *_From, __std_exception_data *_To);
void __cdecl __std_exception_destroy(__std_exception_data *_Data);

__SPRT_END_DECL

namespace std {

SPRT_API class [[nodiscard]] exception {
public:
	exception() noexcept : _Data() { }

	explicit exception(char const *const _Message) noexcept : _Data() {
		__std_exception_data _InitData = {_Message, true};
		__std_exception_copy(&_InitData, &_Data);
	}

	exception(char const *const _Message, int) noexcept : _Data() { _Data._What = _Message; }

	exception(exception const &_Other) noexcept : _Data() {
		__std_exception_copy(&_Other._Data, &_Data);
	}

	exception &operator=(exception const &_Other) noexcept {
		if (this == &_Other) {
			return *this;
		}

		__std_exception_destroy(&_Data);
		__std_exception_copy(&_Other._Data, &_Data);
		return *this;
	}

	virtual ~exception() noexcept { __std_exception_destroy(&_Data); }

	[[nodiscard]]
	virtual char const *what() const {
		return _Data._What ? _Data._What : "Unknown exception";
	}

private:
	__std_exception_data _Data;
};


SPRT_API class [[nodiscard]] bad_array_new_length;

SPRT_API class [[nodiscard]] bad_alloc : public exception {
public:
	bad_alloc() noexcept : exception("bad allocation", 1) { }

private:
	friend bad_array_new_length;

	bad_alloc(char const *const _Message) noexcept : exception(_Message, 1) { }
};

SPRT_API class [[nodiscard]] bad_array_new_length : public bad_alloc {
public:
	bad_array_new_length() noexcept : bad_alloc("bad array new length") { }
};

} // namespace std

#endif // CORE_RUNTIME_INCLUDE_LIBC_MSVCRT_CXX_VCRUNTIME_EXCEPTION_H_
