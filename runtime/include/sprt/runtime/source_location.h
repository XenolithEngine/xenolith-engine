/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_SOURCE_LOCATION_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_SOURCE_LOCATION_H_

#include <sprt/runtime/int.h>

#if __SPRT_USE_LIBCXX_SOURCE_LOCATION

#include <source_location>

namespace sprt {

using source_location = std::source_location;

}

#else // __SPRT_USE_LIBCXX_SOURCE_LOCATION

// __builtin_source_location requires source_location:: __impl to be defined
namespace std {
class source_location {
public:
	// The names source_location::__impl, _M_file_name, _M_function_name, _M_line, and _M_column
	// are hard-coded in the compiler and must not be changed here.
	struct __impl {
		const char *_M_file_name;
		const char *_M_function_name;
		unsigned _M_line;
		unsigned _M_column;
	};
};
} // namespace std

namespace sprt {

class source_location_impl;

class source_location {
protected:
	using __impl = std::source_location::__impl;

	const __impl *__ptr_ = nullptr;

	using __bsl_ty = decltype(__builtin_source_location());

public:
	// The defaulted __ptr argument is necessary so that the builtin is evaluated
	// in the context of the caller. An explicit value should never be provided.
	static consteval source_location current(
			__bsl_ty __ptr = __builtin_source_location()) noexcept {
		source_location __sl;
		__sl.__ptr_ = static_cast<const __impl *>(__ptr);
		return __sl;
	}
	constexpr source_location() noexcept = default;

	constexpr uint_least32_t line() const noexcept {
		return __ptr_ != nullptr ? __ptr_->_M_line : 0;
	}
	constexpr uint_least32_t column() const noexcept {
		return __ptr_ != nullptr ? __ptr_->_M_column : 0;
	}
	constexpr const char *file_name() const noexcept {
		return __ptr_ != nullptr ? __ptr_->_M_file_name : "";
	}
	constexpr const char *function_name() const noexcept {
		return __ptr_ != nullptr ? __ptr_->_M_function_name : "";
	}
};

// Manually fill source_location fields, used by 'assert' implementation
class source_location_ext : public source_location {
public:
	constexpr source_location_ext(const char *file, const char *fn, unsigned line, unsigned col)
	: _data(std::source_location::__impl{file, fn, line, col}) {
		this->__ptr_ = &_data;
	}

private:
	std::source_location::__impl _data;
};

} // namespace sprt

#endif // __SPRT_USE_LIBCXX_SOURCE_LOCATION

#define __SPRT_LOCATION (::sprt::source_location::current())
#define __SPRT_FUNC (::sprt::source_location::current().function_name())

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_SOURCE_LOCATION_H_
