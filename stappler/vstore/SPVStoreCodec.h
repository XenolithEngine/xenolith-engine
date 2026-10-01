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

#ifndef STAPPLER_VSTORE_SPVSTORECODEC_H_
#define STAPPLER_VSTORE_SPVSTORECODEC_H_

#include "SPVStore.h"

// Only a journal compresses anything, so without one there is nothing here to configure.

namespace STAPPLER_VERSIONIZED stappler::vstore {

// How a journal payload is stored. The id kept in a record is the codec that was actually applied,
// never the one selected in the config: that is what makes the decoder a table and what makes the
// per-page fallback to raw work at all, so a journal always holds a mix of None and its configured
// codec and reads back correctly without knowing which page went which way. Lz4 and Lz4Hc emit the
// same format and share one decoder, but they sit at opposite ends of the LZ4 curve (acceleration
// 65537 against CLEVEL_MAX), so they stay separate ids - otherwise a measurement could not tell
// which one produced a number.
enum class JournalCodec : uint16_t {
	None = 0, // stored verbatim: the uncompressed default AND the incompressible-page fallback
	Lz4 = 1,
	Lz4Hc = 2,
	Zstd = 3,
	Lzma = 4,
};

static constexpr uint32_t JournalCodecCount = 5;

// What a journal uses when its config does not say: Zstd, the only codec that buys a large ratio
// without a restore cost an interactive undo would feel - see docs/usage/data/vstore-journal.adoc. A
// compile-time default, not a fallback: an explicit request for a codec whose module is missing
// still fails in init(), and only the unstated default follows what the build can do, so a build
// without stappler_zstd_lib still works with a default-constructed Config.
#ifdef MODULE_STAPPLER_ZSTD_LIB
static constexpr JournalCodec DefaultCodec = JournalCodec::Zstd;
#else
static constexpr JournalCodec DefaultCodec = JournalCodec::None;
#endif

// Compiled-in availability, not a runtime probe: Zstd and Lzma need their optional modules
// (stappler_zstd_lib, stappler_lzma_lib) on the link line. Tests loop over this instead of #ifdef'ing.
SP_PUBLIC bool isCodecAvailable(JournalCodec);
SP_PUBLIC StringView getCodecName(JournalCodec);

// One per Journal, holding whatever reusable state the codecs want plus the single scratch buffer.
// Not thread-safe, and neither is an Arena - a journal's stores are single-threaded together.
// Deliberately not thread_local: a thread-local ZSTD_CCtx would leak at thread exit and would be
// shared by two journals on one thread, where this has an obvious owner.
class SP_PUBLIC JournalCodecContext final {
public:
	// The destination is one byte short of a page, which makes "did not fit" and "not worth it" the
	// same branch: every codec already reports a too-small destination as a failure, so no arm
	// needs a second size comparison and no compressBound-sized buffer exists anywhere.
	static constexpr uint32_t ScratchSize = PageSize - 1;

	~JournalCodecContext();

	JournalCodecContext() = default;
	JournalCodecContext(const JournalCodecContext &) = delete;
	JournalCodecContext &operator=(const JournalCodecContext &) = delete;

	// Fails if the codec's module is not linked in. Not a compile error and not a silent fallback:
	// the module set is a build fact while the codec is a config value, and a journal asked for
	// Zstd that quietly delivered Lz4 would report sizes that are a lie.
	Status init(JournalCodec, memory::pool_t *);

	JournalCodec getCodec() const { return _codec; }

	// Compresses `srcSize` bytes - a whole page, or the gathered sub-blocks of a sub-page delta.
	// Returns the codec actually used; None means "store src verbatim" and leaves out/outSize
	// untouched, and on anything else out points into the internal scratch and stays valid until
	// the next call. `srcSize` may be anything up to PageSize, and the keep rule scales with it
	// rather than with the page: a result is kept iff it is strictly smaller than the input it
	// replaces.
	JournalCodec encodeBlock(const uint8_t *src, uint32_t srcSize, const uint8_t *&out,
			uint32_t &outSize);

	// Decompresses one payload straight into a destination of exactly `dstSize` bytes. There is no
	// bounce buffer on the whole-page restore path - every destination the journal has for one, a
	// hosted chunk page or a page of the slot-0 scratch, is a whole page - and a delta decodes into
	// the caller's gather buffer before being scattered. const, unlike encodeBlock: decoding writes
	// only the caller's destination and the library's own context, never this object's scratch.
	bool decodeBlock(JournalCodec, const uint8_t *src, uint32_t srcSize, uint8_t *dst,
			uint32_t dstSize) const;

	// The whole-page cases, which are still the majority and still say what they mean at a call
	// site.
	JournalCodec encodePage(const uint8_t *src, const uint8_t *&out, uint32_t &outSize) {
		return encodeBlock(src, PageSize, out, outSize);
	}
	bool decodePage(JournalCodec codec, const uint8_t *src, uint32_t srcSize, uint8_t *dst) const {
		return decodeBlock(codec, src, srcSize, dst, PageSize);
	}

private:
	JournalCodec _codec = JournalCodec::None;
	uint8_t *_scratch = nullptr;

	// Opaque per-codec state, held as void * so this header pulls in no third-party declarations.
	// Guarded because the module flags are global to a build, so the layout cannot differ between
	// two translation units of one binary.
#if defined(MODULE_STAPPLER_ZSTD_LIB) || defined(MODULE_STAPPLER_LZMA_LIB)
	void *_encodeState = nullptr;
	void *_decodeState = nullptr;
#endif
};

} // namespace stappler::vstore


#endif /* STAPPLER_VSTORE_SPVSTORECODEC_H_ */
