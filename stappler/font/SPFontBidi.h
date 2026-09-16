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

#ifndef CORE_FONT_SPFONTBIDI_H_
#define CORE_FONT_SPFONTBIDI_H_

#include "SPFontStyle.h"

namespace STAPPLER_VERSIONIZED stappler::font {

// TextDirection -- the UAX #9 base direction (CSS `direction`) -- is declared in SPFontStyle.h and
// shared with the style/layout system.

// A visual run produced after reordering (rules L1-L2): a maximal span of code units that share a
// single embedding level. `offset` and `length` are expressed in code units of the source encoding
// passed to TextBidi::init (bytes for UTF-8, 16-bit units for UTF-16, code points for UTF-32).
struct SP_PUBLIC BidiRun {
	uint32_t offset = 0;
	uint32_t length = 0;
	uint8_t level = 0; // UAX #9 embedding level; odd levels are right-to-left

	bool isRightToLeft() const { return (level & 1) != 0; }
};

// C++ wrapper over the SheenBidi resolver.
//
// Memory model: every SheenBidi object backing a TextBidi is allocated from the CURRENT memory pool
// (memory::pool::acquire()) at the moment a method is called. The pool allocator is installed once
// during module initialization via a stappler_crypto-style scheme (see SPFontBidi.cc).
//
// CALLER CONTRACT: a TextBidi, and any pointer or SpanView it hands out, MUST NOT be shared across
// threads, and MUST NOT outlive the pool context that was current when it was created -- the data
// lives and dies with that pool. Use a TextBidi within a single pool scope (e.g. one layout pass).
class SP_PUBLIC TextBidi {
public:
	TextBidi() = default;
	~TextBidi();

	TextBidi(const TextBidi &) = delete;
	TextBidi &operator=(const TextBidi &) = delete;

	TextBidi(TextBidi &&);
	TextBidi &operator=(TextBidi &&);

	// Build the resolver over a logical-order text buffer. Returns false on empty input or failure.
	bool init(StringView utf8, TextDirection base = TextDirection::Neutral); // UTF-8
	bool init(WideStringView utf16, TextDirection base = TextDirection::Neutral); // UTF-16
	bool init(const char32_t *str, size_t length,
			TextDirection base = TextDirection::Neutral); // UTF-32

	explicit operator bool() const { return _algorithm != nullptr; }

	// Length of the source buffer, in source code units.
	uint32_t getLength() const { return _length; }

	// Visit each paragraph in logical order. `levels` covers `length` resolved embedding levels, one
	// per code unit of the paragraph (rules X1-I2). The span is backed by pool memory and is only
	// valid under the caller contract above.
	void foreachParagraph(const Callback<void(uint32_t offset, uint32_t length, uint8_t baseLevel,
					SpanView<uint8_t> levels)> &) const;

	// Reorder the span [offset, length] (in source code units) into visual order and emit each
	// visual run (rules L1-L2). The span is treated as a single paragraph + line: pass a whole
	// paragraph, or a single line of standalone text. For a line that is a sub-range of a larger
	// paragraph, resolve the paragraph levels via foreachParagraph and reorder from those instead.
	void foreachVisualRun(uint32_t offset, uint32_t length,
			const Callback<void(const BidiRun &)> &) const;

	// UAX #9 L4: the mirror of a Bidi_Mirrored code point (e.g. '(' <-> ')'), or 0 if it has none.
	// Stateless (no resolver instance needed). Used to mirror brackets in RTL runs on the non-shaped
	// path; HarfBuzz performs this substitution itself when shaping.
	static char32_t mirrorCodepoint(char32_t);

private:
	bool doInit(const void *buffer, size_t length, uint32_t encoding, TextDirection base);

	const void *_algorithm = nullptr; // SBAlgorithmRef (an opaque const handle; SheenBidi stays out
	// of the public header)
	uint32_t _length = 0;
	uint32_t _encoding = 0; // SBStringEncoding
	TextDirection _base = TextDirection::Neutral;
};

} // namespace stappler::font

#endif /* CORE_FONT_SPFONTBIDI_H_ */
