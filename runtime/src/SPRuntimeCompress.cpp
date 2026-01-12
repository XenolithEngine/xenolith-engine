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

#include <sprt/runtime/compress.h>
#include <sprt/runtime/detail/operations.h>

#include "private/thirdparty/lz4hc.h"

namespace sprt {

thread_local uint8_t tl_lz4HCEncodeState[sprt::max(sizeof(LZ4_streamHC_t), sizeof(LZ4_stream_t))];

uint8_t *lz4_getEncodeState() { return tl_lz4HCEncodeState; }

size_t lz4_getCompressBounds(size_t size) {
	if (size < LZ4_MAX_INPUT_SIZE) {
		return LZ4_compressBound(int(size)) + ((size <= 0xFFFF) ? 2 : 4);
	}
	return 0;
}

size_t lz4_compressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize) {
	auto ret = LZ4_compress_fast_extState(tl_lz4HCEncodeState, (const char *)src, (char *)dest,
			int(srcSize), int(destSize), 65'537);
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}

size_t lz4hc_compressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize) {
	auto ret = LZ4_compress_HC_extStateHC(tl_lz4HCEncodeState, (const char *)src, (char *)dest,
			int(srcSize), int(destSize), LZ4HC_CLEVEL_MAX);
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}

size_t lz4_decompressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize) {
	auto ret = LZ4_decompress_safe((const char *)src, (char *)dest, int(srcSize), int(destSize));
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}


} // namespace sprt
