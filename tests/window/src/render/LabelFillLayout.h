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

#ifndef TESTS_WINDOW_SRC_RENDER_LABELFILLLAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_LABELFILLLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class LabelFillLabel;

// What a label's damage box costs, in both modes of basic2d::Label::setBoundsFromLayout.
//
// A grid of labels in Latin, Cyrillic and Greek. `label-fill.bench` writes each label's quads again
// (what a label pays on every change) and asks the result for its box, the way damage tracking and
// the painter's order do - once with the box read from the quads through the font atlas, once with
// the box the layout supplies. The two times, per label and per glyph, are what the new mode costs
// against the old one; the two boxes are what each one damages. Times mean something only in a
// release build (docs/agents/measuring-frames.md).
class LabelFillLayout : public TestLayout {
public:
	static constexpr uint32_t DefaultCount = 24;
	static constexpr uint32_t DefaultLength = 160;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void fill(uint32_t count, uint32_t length);

	Value bench(uint32_t iterations);

	Vector<LabelFillLabel *> _labels;
	uint32_t _count = DefaultCount;
	uint32_t _length = DefaultLength;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_RENDER_LABELFILLLAYOUT_H_
