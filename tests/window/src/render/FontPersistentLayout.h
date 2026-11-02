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


#ifndef TESTS_WINDOW_SRC_RENDER_FONTPERSISTENTLAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_FONTPERSISTENTLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class FontPersistentLabel;

// Labels with persistent glyph data, shown one at a time: each has a size and a run of characters
// of its own, so each brings new persistent glyphs, and every font input after it copies the old
// ones from the blocks they live in. With a small block (XL_FONT_PERSISTENT_BLOCK) the persistent
// glyphs spread over many blocks; the picture must be the one a single block gives.
class FontPersistentLayout : public TestLayout {
public:
	static constexpr uint32_t MaxLabels = 32;
	static constexpr uint32_t CharsPerLabel = 24;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void show(uint32_t count);

	// The shown labels, their glyphs and how many of them the current atlas lacks
	Value state() const;

	Vector<FontPersistentLabel *> _labels;
	uint32_t _shown = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_RENDER_FONTPERSISTENTLAYOUT_H_
