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

#include <sprt/runtime/utils/compress.h>
#include <sprt/cxx/algorithm>
#include <sprt/c/__sprt_stdlib.h>

#include "private/thirdparty/lz4hc.h"

namespace sprt {

// The LZ4 states of a thread come from the heap at its first compression and go with the thread:
// a thread's static TLS block is taken whole whether it compresses or not, and the states are
// about 270 KiB of it.
struct Lz4States {
	void *encode = nullptr;
	LZ4_stream_t *dict = nullptr;

	~Lz4States() {
		::__sprt_free(encode);
		::__sprt_free(dict);
	}
};

static thread_local Lz4States tl_lz4States;

static constexpr size_t LZ4_ENCODE_STATE_SIZE = sprt::max(sizeof(LZ4_streamHC_t), sizeof(LZ4_stream_t));

// LZ4's public API takes the source/destination sizes as `int`. An untrusted
// size_t that exceeds INT_MAX wraps to a negative `int`, which LZ4 does not
// validate (it computes `iend = ip + srcSize`, so `iend < ip` defeats every
// bounds check). Reject anything that would not survive the narrowing cast.
static constexpr size_t LZ4_MAX_SIZE = 0x7FFFFFFF; // INT_MAX

// nullptr when the heap has no room for it.
uint8_t *lz4_getEncodeState() {
	if (!tl_lz4States.encode) {
		tl_lz4States.encode = ::__sprt_malloc(LZ4_ENCODE_STATE_SIZE);
	}
	return static_cast<uint8_t *>(tl_lz4States.encode);
}

size_t lz4_getCompressBounds(size_t size) {
	if (size < LZ4_MAX_INPUT_SIZE) {
		return LZ4_compressBound(int(size)) + ((size <= 0xFFFF) ? 2 : 4);
	}
	return 0;
}

size_t lz4_compressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize) {
	if (srcSize >= LZ4_MAX_INPUT_SIZE || destSize > LZ4_MAX_SIZE) {
		return 0;
	}
	auto state = lz4_getEncodeState();
	if (!state) {
		return 0;
	}
	auto ret = LZ4_compress_fast_extState(state, (const char *)src, (char *)dest, int(srcSize),
			int(destSize), 65'537);
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}

size_t lz4hc_compressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize) {
	if (srcSize >= LZ4_MAX_INPUT_SIZE || destSize > LZ4_MAX_SIZE) {
		return 0;
	}
	auto state = lz4_getEncodeState();
	if (!state) {
		return 0;
	}
	auto ret = LZ4_compress_HC_extStateHC(state, (const char *)src, (char *)dest, int(srcSize),
			int(destSize), LZ4HC_CLEVEL_MAX);
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}

size_t lz4_decompressData(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize) {
	if (srcSize > LZ4_MAX_SIZE || destSize > LZ4_MAX_SIZE) {
		return 0;
	}
	auto ret = LZ4_decompress_safe((const char *)src, (char *)dest, int(srcSize), int(destSize));
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}

size_t lz4_compressDataDict(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize,
		const uint8_t *dict, size_t dictSize) {
	if (srcSize >= LZ4_MAX_INPUT_SIZE || destSize > LZ4_MAX_SIZE || dictSize > LZ4_MAX_SIZE) {
		return 0;
	}
	// A stream of its own: LZ4_initStream needs it aligned, unlike the extState API above.
	if (!tl_lz4States.dict) {
		tl_lz4States.dict = static_cast<LZ4_stream_t *>(::__sprt_malloc(sizeof(LZ4_stream_t)));
	}
	LZ4_stream_t *stream =
			tl_lz4States.dict ? LZ4_initStream(tl_lz4States.dict, sizeof(LZ4_stream_t)) : nullptr;
	if (!stream) {
		return 0;
	}
	if (dict && dictSize > 0) {
		LZ4_loadDict(stream, (const char *)dict, int(dictSize));
	}
	auto ret = LZ4_compress_fast_continue(stream, (const char *)src, (char *)dest, int(srcSize),
			int(destSize), 1);
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}

size_t lz4_decompressDataDict(const uint8_t *src, size_t srcSize, uint8_t *dest, size_t destSize,
		const uint8_t *dict, size_t dictSize) {
	if (srcSize > LZ4_MAX_SIZE || destSize > LZ4_MAX_SIZE || dictSize > LZ4_MAX_SIZE) {
		return 0;
	}
	auto ret = LZ4_decompress_safe_usingDict((const char *)src, (char *)dest, int(srcSize),
			int(destSize), (const char *)dict, int(dictSize));
	if (ret > 0) {
		return size_t(ret);
	}
	return 0;
}


} // namespace sprt
