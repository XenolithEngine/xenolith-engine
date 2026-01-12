/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_BIT_CAST_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_BIT_CAST_H_

#include <sprt/runtime/detail/constructable.h>
#include <sprt/runtime/detail/constexpr.h>

namespace sprt {

template <typename To, typename From>
sprt::enable_if_t< sizeof(To) == sizeof(From)
				&& sprt::is_trivially_copyable_v<From> && sprt::is_trivially_copyable_v<To>,
		To>
// constexpr support needs compiler magic
bit_cast(const From &src) noexcept {
	static_assert(sprt::is_trivially_constructible_v<To>,
        "This implementation additionally requires "
        "destination type to be trivially constructible");

	To dst;
	__builtin_memcpy(&dst, &src, sizeof(To));
	return dst;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_BIT_CAST_H_
