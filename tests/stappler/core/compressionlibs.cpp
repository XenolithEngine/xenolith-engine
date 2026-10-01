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


// Round trips through the sysroot zstd and lzma that stappler_zstd_lib and stappler_lzma_lib link.

#include "SPCommon.h"

#ifdef MODULE_STAPPLER_ZSTD_LIB
#include <zstd.h>
#endif

#ifdef MODULE_STAPPLER_LZMA_LIB
#include <lzma.h>
#endif

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

using Bytes = memory::StandardInterface::BytesType;

static Bytes makeSample() {
	Bytes ret;
	ret.resize(64 * 1'024);
	for (size_t i = 0; i < ret.size(); ++i) { ret[i] = uint8_t((i / 7) * 31 + (i % 13)); }
	return ret;
}

void performCompressionLibTests() {
	sprt::cout << "\n== stappler compression library tests ==\n";

	auto sample = makeSample();

#ifdef MODULE_STAPPLER_ZSTD_LIB
	{
		Bytes packed;
		packed.resize(ZSTD_compressBound(sample.size()));
		auto packedSize = ZSTD_compress(packed.data(), packed.size(), sample.data(), sample.size(), 3);
		check(!ZSTD_isError(packedSize) && packedSize < sample.size(), "zstd: compress");

		Bytes unpacked;
		unpacked.resize(sample.size());
		auto size = ZSTD_isError(packedSize)
				? packedSize
				: ZSTD_decompress(unpacked.data(), unpacked.size(), packed.data(), packedSize);
		check(!ZSTD_isError(size) && size == sample.size() && unpacked == sample,
				"zstd: round trip");
	}
#else
	sprt::cout << "SKIP  zstd (stappler_zstd_lib not enabled)\n";
#endif

#ifdef MODULE_STAPPLER_LZMA_LIB
	{
		Bytes packed;
		packed.resize(lzma_stream_buffer_bound(sample.size()));
		size_t packedSize = 0;
		auto ret = lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, nullptr, sample.data(),
				sample.size(), packed.data(), &packedSize, packed.size());
		check(ret == LZMA_OK && packedSize < sample.size(), "lzma: compress");

		Bytes unpacked;
		unpacked.resize(sample.size());
		uint64_t memlimit = UINT64_MAX;
		size_t inPos = 0;
		size_t outPos = 0;
		ret = lzma_stream_buffer_decode(&memlimit, 0, nullptr, packed.data(), &inPos, packedSize,
				unpacked.data(), &outPos, unpacked.size());
		check(ret == LZMA_OK && inPos == packedSize && outPos == sample.size()
						&& unpacked == sample,
				"lzma: round trip");
	}
#else
	sprt::cout << "SKIP  lzma (stappler_lzma_lib not enabled)\n";
#endif
}

} // namespace STAPPLER_VERSIONIZED stappler
