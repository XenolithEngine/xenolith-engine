/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Page codecs for the journal, and for keyframe images, which use the same block format. The whole
// surface is two functions, and the interesting decision is in encodePage: whether the compressed
// form is kept at all is decided per page, never once for the store, because arena metadata pages
// are long runs of zeros and near-identical block headers while payload pages may be
// incompressible. The rule is to compress into a destination of PageSize - 1 and keep the result
// iff the codec returned n != 0 && n < PageSize; sizing the destination one byte short of a page is
// what collapses "did not fit" and "not worth it" into one branch, every codec below already
// signalling a too-small destination as a plain failure.

#include "SPVStoreCodec.h"

#include "SPLog.h"

#include <sprt/runtime/utils/compress.h>

#ifdef MODULE_STAPPLER_ZSTD_LIB
#include <zstd.h>
#endif

#ifdef MODULE_STAPPLER_LZMA_LIB
#include <lzma.h> // LZMA_API_STATIC comes from stappler_lzma_lib
#endif

namespace STAPPLER_VERSIONIZED stappler::vstore {

#ifdef MODULE_STAPPLER_ZSTD_LIB
// The library default. Named because a benchmark that wants another point has to change one thing.
static constexpr int ZstdLevel = 3;
#endif

#ifdef MODULE_STAPPLER_LZMA_LIB
static constexpr uint32_t LzmaPreset = 6; // the library default

// The raw single-call encoder has no reusable context - it allocates internally per call. What is
// reusable, and what is expensive to get wrong, is the filter chain, so it is built once.
struct LzmaState {
	lzma_options_lzma opts;
	lzma_filter filters[2];
};
#endif

bool isCodecAvailable(JournalCodec codec) {
	switch (codec) {
	case JournalCodec::None:
	case JournalCodec::Lz4:
	case JournalCodec::Lz4Hc:
		// Both live in the runtime module, which every consumer already depends on transitively.
		return true;
	case JournalCodec::Zstd:
#ifdef MODULE_STAPPLER_ZSTD_LIB
		return true;
#else
		return false;
#endif
	case JournalCodec::Lzma:
#ifdef MODULE_STAPPLER_LZMA_LIB
		return true;
#else
		return false;
#endif
	}
	return false;
}

StringView getCodecName(JournalCodec codec) {
	switch (codec) {
	case JournalCodec::None: return StringView("none");
	case JournalCodec::Lz4: return StringView("lz4");
	case JournalCodec::Lz4Hc: return StringView("lz4hc");
	case JournalCodec::Zstd: return StringView("zstd");
	case JournalCodec::Lzma: return StringView("lzma");
	}
	return StringView("?");
}

JournalCodecContext::~JournalCodecContext() {
#ifdef MODULE_STAPPLER_ZSTD_LIB
	// Keyed on the codec, not merely on the pointer being set: _encodeState is a different type for
	// every arm that uses it, and Lzma's lives in a pool that must not be freed here at all.
	if (_codec == JournalCodec::Zstd) {
		ZSTD_freeCCtx(reinterpret_cast<ZSTD_CCtx *>(_encodeState));
		ZSTD_freeDCtx(reinterpret_cast<ZSTD_DCtx *>(_decodeState));
		_encodeState = nullptr;
		_decodeState = nullptr;
	}
#endif
}

Status JournalCodecContext::init(JournalCodec codec, memory::pool_t *pool) {
	if (!isCodecAvailable(codec)) {
		slog().error("vstore::JournalCodec", "codec ", getCodecName(codec),
				" is not available in this build");
		return Status::ErrorNotSupported;
	}

	_codec = codec;
	if (codec == JournalCodec::None) {
		return Status::Ok;
	}

	// A member rather than a stack buffer: it stays warm across the page loop and keeps a 4 KiB
	// frame out of the write barrier's inner loop.
	_scratch = reinterpret_cast<uint8_t *>(memory::pool::palloc(pool, ScratchSize, MaxAlign));
	if (!_scratch) {
		return Status::ErrorOutOfHostMemory;
	}

#ifdef MODULE_STAPPLER_ZSTD_LIB
	if (codec == JournalCodec::Zstd) {
		// Created once and reused: the per-call ZSTD_compress() allocates a context every time, and
		// at 4 KiB a page that allocation is the dominant cost.
		_encodeState = ZSTD_createCCtx();
		_decodeState = ZSTD_createDCtx();
		if (!_encodeState || !_decodeState) {
			return Status::ErrorOutOfHostMemory;
		}
	}
#endif

#ifdef MODULE_STAPPLER_LZMA_LIB
	if (codec == JournalCodec::Lzma) {
		auto st = reinterpret_cast<LzmaState *>(
				memory::pool::palloc(pool, sizeof(LzmaState), MaxAlign));
		if (!st) {
			return Status::ErrorOutOfHostMemory;
		}
		if (lzma_lzma_preset(&st->opts, LzmaPreset)) {
			slog().error("vstore::JournalCodec", "lzma preset ", LzmaPreset, " is unsupported");
			return Status::ErrorNotSupported;
		}

		// The single most important line here. The encoder sizes its match finder off dict_size,
		// and the preset default is 8 MiB - leaving it there would allocate and initialise
		// megabytes for every page. No input is ever longer than a page, so nothing beyond a page
		// can be referenced anyway. Comfortably above LZMA_DICT_SIZE_MIN, which is 4 KiB.
		st->opts.dict_size = PageSize;

		// LZMA1EXT rather than LZMA1: plain LZMA1 always writes an end-of-payload marker, while EXT
		// omits it when the uncompressed size is known out of band - which it always is here, since
		// the record carries it. Worth ~6 bytes a payload. ext_size is read by the decoder only,
		// and is set per call rather than here: a delta's length is not the constant a page's was.
		st->opts.ext_flags = 0;

		st->filters[0].id = LZMA_FILTER_LZMA1EXT;
		st->filters[0].options = &st->opts;
		st->filters[1].id = LZMA_VLI_UNKNOWN;
		st->filters[1].options = nullptr;

		_encodeState = st;
	}
#endif

	return Status::Ok;
}

JournalCodec JournalCodecContext::encodeBlock(const uint8_t *src, uint32_t srcSize,
		const uint8_t *&out, uint32_t &outSize) {
	// The uncompressed default must not cost more than one compare.
	if (_codec == JournalCodec::None) {
		return JournalCodec::None;
	}

	// One byte short of the input rather than of a page, which is what makes "did not fit" and "not
	// worth it" the same branch for a delta as they were for a page: a codec that cannot reach
	// srcSize - 1 has already lost, whatever the scratch could have held.
	uint32_t limit = srcSize > 0 ? srcSize - 1 : 0;
	if (limit == 0) {
		return JournalCodec::None;
	}

	size_t n = 0;
	switch (_codec) {
	case JournalCodec::Lz4: n = sprt::lz4_compressData(src, srcSize, _scratch, limit); break;
	case JournalCodec::Lz4Hc: n = sprt::lz4hc_compressData(src, srcSize, _scratch, limit); break;
#ifdef MODULE_STAPPLER_ZSTD_LIB
	case JournalCodec::Zstd: {
		auto r = ZSTD_compressCCtx(reinterpret_cast<ZSTD_CCtx *>(_encodeState), _scratch, limit, src,
				srcSize, ZstdLevel);
		// A destination too small is reported as an error, which is the "not worth it" answer here.
		n = ZSTD_isError(r) ? 0 : r;
		break;
	}
#endif
#ifdef MODULE_STAPPLER_LZMA_LIB
	case JournalCodec::Lzma: {
		auto st = reinterpret_cast<LzmaState *>(_encodeState);
		// ext_size is the decoder's only record of how long the payload was, so it moves with the
		// input. It was set once at init while every input was a page; it cannot be now.
		lzma_set_ext_size(st->opts, srcSize);
		size_t outPos = 0;
		// LZMA_BUF_ERROR is the "did not fit" answer, and out_pos is only updated on success.
		if (lzma_raw_buffer_encode(st->filters, nullptr, src, srcSize, _scratch, &outPos, limit)
				== LZMA_OK) {
			n = outPos;
		}
		break;
	}
#endif
	default: break;
	}

	// Strictly smaller, with no margin. A margin would be a tuning knob with no principled value,
	// while n == srcSize has to be rejected outright: at equal size the raw form is better, because
	// it does not have to be decoded. The check is spelled out even though `limit` implies it - it
	// is the rule, and it must survive someone changing how the destination is sized.
	if (n == 0 || n >= srcSize) {
		return JournalCodec::None;
	}

	out = _scratch;
	outSize = uint32_t(n);
	return _codec;
}

bool JournalCodecContext::decodeBlock(JournalCodec codec, const uint8_t *src, uint32_t srcSize,
		uint8_t *dst, uint32_t dstSize) const {
	switch (codec) {
	case JournalCodec::None:
		if (srcSize != dstSize) {
			return false;
		}
		__sprt_memcpy(dst, src, dstSize);
		return true;
	case JournalCodec::Lz4:
	case JournalCodec::Lz4Hc:
		// One decoder for both: the two differ only in how hard the encoder looked. The exact
		// uncompressed size is known from the record, which is why the bare LZ4 block needs no
		// framing.
		return sprt::lz4_decompressData(src, srcSize, dst, dstSize) == dstSize;
#ifdef MODULE_STAPPLER_ZSTD_LIB
	case JournalCodec::Zstd: {
		// Null when this context was initialized for a different codec. Reachable only from a
		// record written by some other context, which is corruption, so refuse rather than crash.
		if (!_decodeState) {
			return false;
		}
		auto r = ZSTD_decompressDCtx(reinterpret_cast<ZSTD_DCtx *>(_decodeState), dst, dstSize, src,
				srcSize);
		return !ZSTD_isError(r) && r == dstSize;
	}
#endif
#ifdef MODULE_STAPPLER_LZMA_LIB
	case JournalCodec::Lzma: {
		auto st = reinterpret_cast<LzmaState *>(_encodeState);
		if (!st) {
			return false;
		}
		// The filter chain is shared with the encoder, so this has to restate the payload length
		// the same way encodeBlock did. Harmless to the decoder's own accounting - outPos is what
		// is checked - but the chain must not be left describing some other block's size.
		lzma_set_ext_size(st->opts, dstSize);
		size_t inPos = 0;
		size_t outPos = 0;
		return lzma_raw_buffer_decode(st->filters, nullptr, src, &inPos, srcSize, dst, &outPos,
					   dstSize)
				== LZMA_OK
				&& outPos == dstSize;
	}
#endif
	default: break;
	}
	return false;
}

} // namespace stappler::vstore
