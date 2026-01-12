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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_BASE16_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_BASE16_H_

#include <sprt/runtime/callback.h>

namespace sprt::base16 {

SPRT_API const char *toHex(const uint8_t &c, bool upper);

SPRT_API uint8_t toChar(const char &c);

SPRT_API uint8_t toChar(const char &c, const char &d);

SPRT_API constexpr inline size_t getEncodeSize(size_t l) { return l * 2; }

SPRT_API constexpr inline size_t getDecodeSize(size_t l) { return l / 2; }

SPRT_API size_t encode(const uint8_t *in, size_t insize, char *out, size_t outsize,
		bool upper = true);

SPRT_API size_t encode(const uint8_t *in, size_t insize,
		const callback<void(const char *, size_t)> &, bool upper = true);

SPRT_API size_t decode(const char *in, size_t insize, uint8_t *out, size_t outsize);

SPRT_API size_t decode(const char *in, size_t insize,
		const callback<void(const uint8_t *, size_t)> &);

} // namespace sprt::base16

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_BASE16_H_
