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

#include "SPFontBidi.h"
#include "SPCore.h"
#include "SPMemory.h"

#include <SheenBidi/SheenBidi.h>

namespace STAPPLER_VERSIONIZED stappler::font {

// ---------------------------------------------------------------------------------------------
// Pool-backed SheenBidi allocator
//
// Every block is allocated from the CURRENT memory pool (memory::pool::acquire()) at the moment of
// the call. A small, alignment-preserving header records the block size so deallocate/reallocate
// can return it to the pool's free list. SheenBidi keeps a single process-global default allocator,
// but the pool target is resolved per call, so each thread naturally allocates from its own current
// pool.
//
// CALLER CONTRACT (see SPFontBidi.h): SheenBidi data must not be shared across threads, nor used
// after the owning pool context has changed or been cleared. The current pool at free/realloc time
// is therefore the same context that performed the allocation.
// ---------------------------------------------------------------------------------------------

static constexpr size_t SB_HEADER_SIZE =
		16; // holds the block size, keeps 16-byte payload alignment
static_assert(SB_HEADER_SIZE >= sizeof(size_t), "allocation header must hold the block size");

static void *poolAllocateBlock(SBUInteger size, void *) {
	auto pool = memory::pool::acquire();
	if (!pool) {
		return nullptr;
	}
	auto base =
			reinterpret_cast<uint8_t *>(memory::pool::palloc(pool, size_t(size) + SB_HEADER_SIZE));
	if (!base) {
		return nullptr;
	}
	*reinterpret_cast<size_t *>(base) = size_t(size);
	return base + SB_HEADER_SIZE;
}

static void poolDeallocateBlock(void *pointer, void *) {
	if (!pointer) {
		return;
	}
	auto base = reinterpret_cast<uint8_t *>(pointer) - SB_HEADER_SIZE;
	auto size = *reinterpret_cast<size_t *>(base);
	if (auto pool = memory::pool::acquire()) {
		memory::pool::free(pool, base, size + SB_HEADER_SIZE);
	}
}

static void *poolReallocateBlock(void *pointer, SBUInteger newSize, void *info) {
	if (!pointer) {
		return poolAllocateBlock(newSize, info);
	}
	auto base = reinterpret_cast<uint8_t *>(pointer) - SB_HEADER_SIZE;
	auto oldSize = *reinterpret_cast<size_t *>(base);
	auto target = poolAllocateBlock(newSize, info);
	if (target) {
		sprt::memcpy(target, pointer, sprt::min(oldSize, size_t(newSize)));
		poolDeallocateBlock(pointer, info);
	}
	return target;
}

// ---------------------------------------------------------------------------------------------
// Initialization scheme (mirrors stappler_crypto's BackendInterface): a static object registers an
// init/term pair via addInitializer. On init it installs the pool allocator as SheenBidi's default;
// on term it reverts to SheenBidi's built-in (malloc) allocator and releases the allocator object.
// ---------------------------------------------------------------------------------------------

struct BidiInterface {
	static void initialize(void *ptr) { reinterpret_cast<BidiInterface *>(ptr)->init(); }
	static void terminate(void *ptr) { reinterpret_cast<BidiInterface *>(ptr)->term(); }

	BidiInterface() { addInitializer(this, initialize, terminate); }

	void init() {
		static const SBAllocatorProtocol protocol = {
			&poolAllocateBlock,
			&poolReallocateBlock,
			&poolDeallocateBlock,
			nullptr, // allocateScratch: fall back to allocateBlock (pools are reclaimed in bulk)
			nullptr, // resetScratch
			nullptr, // finalize
		};

		// Created while SheenBidi's default is still its built-in (malloc) allocator, so the
		// allocator object itself is process-global and pool-independent.
		_allocator = SBAllocatorCreate(&protocol, nullptr);
		if (_allocator) {
			SBAllocatorSetDefault(_allocator);
		}
	}

	void term() {
		SBAllocatorSetDefault(nullptr); // revert to SheenBidi's built-in allocator
		if (_allocator) {
			SBAllocatorRelease(_allocator);
			_allocator = nullptr;
		}
	}

	SBAllocatorRef _allocator = nullptr;
};

static BidiInterface s_bidiInterface;

// ---------------------------------------------------------------------------------------------
// TextBidi
// ---------------------------------------------------------------------------------------------

static SBLevel sbBaseLevel(TextDirection dir) {
	switch (dir) {
	case TextDirection::LeftToRight: return 0;
	case TextDirection::RightToLeft: return 1;
	case TextDirection::Neutral: break;
	}
	return SBLevelDefaultLTR; // auto: derive from first strong character, default LTR
}

TextBidi::~TextBidi() {
	if (_algorithm) {
		SBAlgorithmRelease(reinterpret_cast<SBAlgorithmRef>(_algorithm));
		_algorithm = nullptr;
	}
}

TextBidi::TextBidi(TextBidi &&other) {
	_algorithm = other._algorithm;
	_length = other._length;
	_encoding = other._encoding;
	_base = other._base;

	other._algorithm = nullptr;
	other._length = 0;
}

TextBidi &TextBidi::operator=(TextBidi &&other) {
	if (this == &other) {
		return *this;
	}
	if (_algorithm) {
		SBAlgorithmRelease(reinterpret_cast<SBAlgorithmRef>(_algorithm));
	}

	_algorithm = other._algorithm;
	_length = other._length;
	_encoding = other._encoding;
	_base = other._base;

	other._algorithm = nullptr;
	other._length = 0;
	return *this;
}

bool TextBidi::doInit(const void *buffer, size_t length, uint32_t encoding, TextDirection base) {
	if (_algorithm) {
		SBAlgorithmRelease(reinterpret_cast<SBAlgorithmRef>(_algorithm));
		_algorithm = nullptr;
	}
	_length = 0;

	if (!buffer || length == 0) {
		return false;
	}

	SBCodepointSequence sequence;
	sequence.stringEncoding = encoding;
	sequence.stringBuffer = buffer;
	sequence.stringLength = length;

	auto algorithm = SBAlgorithmCreate(&sequence);
	if (!algorithm) {
		return false;
	}

	_algorithm = static_cast<const void *>(algorithm);
	_length = uint32_t(length);
	_encoding = encoding;
	_base = base;
	return true;
}

bool TextBidi::init(StringView utf8, TextDirection base) {
	return doInit(utf8.data(), utf8.size(), SBStringEncodingUTF8, base);
}

bool TextBidi::init(WideStringView utf16, TextDirection base) {
	return doInit(utf16.data(), utf16.size(), SBStringEncodingUTF16, base);
}

bool TextBidi::init(const char32_t *str, size_t length, TextDirection base) {
	return doInit(str, length, SBStringEncodingUTF32, base);
}

char32_t TextBidi::mirrorCodepoint(char32_t cp) {
	return char32_t(SBCodepointGetMirror(SBCodepoint(cp))); // returns 0 when there is no mirror
}

void TextBidi::foreachParagraph(
		const Callback<void(uint32_t, uint32_t, uint8_t, SpanView<uint8_t>)> &cb) const {
	if (!_algorithm) {
		return;
	}
	auto algorithm = reinterpret_cast<SBAlgorithmRef>(_algorithm);
	auto base = sbBaseLevel(_base);

	SBUInteger offset = 0;
	while (offset < _length) {
		auto paragraph = SBAlgorithmCreateParagraph(algorithm, offset, _length - offset, base);
		if (!paragraph) {
			break;
		}

		auto len = SBParagraphGetLength(paragraph);
		auto levels = SBParagraphGetLevelsPtr(paragraph);
		cb(uint32_t(offset), uint32_t(len), uint8_t(SBParagraphGetBaseLevel(paragraph)),
				SpanView<uint8_t>(levels, len));

		SBParagraphRelease(paragraph);
		if (len == 0) {
			break; // defensive: never spin on a zero-length paragraph
		}
		offset += len;
	}
}

void TextBidi::foreachVisualRun(uint32_t offset, uint32_t length,
		const Callback<void(const BidiRun &)> &cb) const {
	if (!_algorithm || length == 0) {
		return;
	}
	auto algorithm = reinterpret_cast<SBAlgorithmRef>(_algorithm);

	auto paragraph = SBAlgorithmCreateParagraph(algorithm, offset, length, sbBaseLevel(_base));
	if (!paragraph) {
		return;
	}

	if (auto line = SBParagraphCreateLine(paragraph, offset, length)) {
		auto count = SBLineGetRunCount(line);
		auto runs = SBLineGetRunsPtr(line);
		for (SBUInteger i = 0; i < count; ++i) {
			BidiRun run;
			run.offset = uint32_t(runs[i].offset);
			run.length = uint32_t(runs[i].length);
			run.level = uint8_t(runs[i].level);
			cb(run);
		}
		SBLineRelease(line);
	}

	SBParagraphRelease(paragraph);
}

} // namespace stappler::font
