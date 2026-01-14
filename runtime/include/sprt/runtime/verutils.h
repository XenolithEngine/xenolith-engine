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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_VERUTILS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_VERUTILS_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/stringview.h>

namespace sprt {

inline constexpr uint32_t SPRT_MAKE_VERSION(uint32_t variant, uint32_t major, uint32_t minor,
		uint32_t patch) {
	return (uint32_t(variant) << 29) | (uint32_t(major) << 22) | (uint32_t(minor) << 12)
			| uint32_t(patch & 0b1111'1111'1111);
}

inline constexpr uint32_t SPRT_VERSION_PATCH(uint32_t index) {
	return uint32_t(index & 0b1111'1111'1111);
}

inline constexpr uint32_t SPRT_VERSION_MINOR(uint32_t index) {
	return uint32_t(index >> 12 & 0b11'1111'1111);
}

inline constexpr uint32_t SPRT_VERSION_MAJOR(uint32_t index) {
	return uint32_t(index >> 22 & 0b111'1111);
}

inline constexpr uint32_t SPRT_VERSION_VARIANT(uint32_t index) {
	return uint32_t(index >> 29 & 0b111);
}

inline uint32_t SPRT_MAKE_VERSION(StringView version) {
	uint32_t ver[4];
	uint32_t i = 0;
	version.split<StringView::Chars<'.'>>([&](StringView str) {
		if (i < 4) {
			ver[i++] = uint32_t(str.readInteger(10).get(0));
		}
	});

	uint32_t verCode = 0;
	switch (i) {
	case 0: verCode = SPRT_MAKE_VERSION(0, 0, 1, 0); break;
	case 1: verCode = SPRT_MAKE_VERSION(0, ver[0], 0, 0); break;
	case 2: verCode = SPRT_MAKE_VERSION(0, ver[0], ver[1], 0); break;
	case 3: verCode = SPRT_MAKE_VERSION(0, ver[0], ver[1], ver[2]); break;
	case 4: verCode = SPRT_MAKE_VERSION(ver[0], ver[1], ver[2], ver[3]); break;
	}
	return verCode;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_VERUTILS_H_
