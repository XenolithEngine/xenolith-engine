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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_COMPRESS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_COMPRESS_H_

#include <sprt/runtime/int.h>

namespace sprt {

SPRT_API uint8_t *lz4_getEncodeState();

SPRT_API size_t lz4_getCompressBounds(size_t size);

SPRT_API size_t lz4_compressData(const uint8_t *src, size_t srcSize, uint8_t *dest,
		size_t destSize);

SPRT_API size_t lz4hc_compressData(const uint8_t *src, size_t srcSize, uint8_t *dest,
		size_t destSize);

SPRT_API size_t lz4_decompressData(const uint8_t *src, size_t srcSize, uint8_t *dest,
		size_t destSize);

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_COMPRESS_H_
