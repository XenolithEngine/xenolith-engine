/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_DETAIL_SPHASH_H_
#define STAPPLER_CORE_DETAIL_SPHASH_H_

#include "SPPlatformInit.h"
#include <sprt/runtime/hash.h>

// A part of SPCore.h, DO NOT include this directly

// Based on XXH (https://cyan4973.github.io/xxHash/#benchmarks)
// constexpr implementation from https://github.com/ekpyron/xxhashct

// Requires C++17

namespace STAPPLER_VERSIONIZED stappler {

// used for naming/hashing (like "MyTag"_tag)
constexpr sprt::uint32_t operator""_hash(const char *str, sprt::size_t len) {
	return sprt::hash32(str, sprt::uint32_t(len));
}
constexpr sprt::uint32_t operator""_tag(const char *str, sprt::size_t len) {
	return sprt::hash32(str, sprt::uint32_t(len));
}

constexpr sprt::uint64_t operator""_hash64(const char *str, sprt::size_t len) {
	return sprt::hash64(str, len);
}
constexpr sprt::uint64_t operator""_tag64(const char *str, sprt::size_t len) {
	return sprt::hash64(str, len);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* LIBSTAPPLER_COMMON_DETAIL_SPHASH_H_ */
