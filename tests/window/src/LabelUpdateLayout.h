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

#ifndef TESTS_WINDOW_SRC_LABELUPDATELAYOUT_H_
#define TESTS_WINDOW_SRC_LABELUPDATELAYOUT_H_

#include "TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Changing a label's text AFTER the first layout must resize every content-sized ancestor, exactly
// as if the final text had been there from the start.
//
// The chain that used to break: a container sized by `fit-content` does not own its ContentSize -
// the container above it measures the subtree and assigns it. So re-laying out its children never
// changes its own size, the content-size event never leaves it, and every ancestor kept the box it
// had measured from the OLD text. The visible result was a label overflowing its box and drawing
// over its neighbours, while the label's own geometry looked perfectly correct - which is what made
// it read like a glyph-rendering bug.
//
// Each row here is a two-level content-sized chain, outer -> chip -> label, inside one fixed-size
// flex column. Three of them are built per group: the reference gets the final text at init, the
// short reference keeps the initial text forever, and the subject is re-stringed at runtime and
// must match first one, then the other.
//
// Two groups, because a growing label moves two different things: the unconstrained group grows
// along the main axis, the clamped group (maxMain on the chip) hits its limit, wraps, and has to
// grow along the CROSS axis instead - which is the case that used to leave text outside its box.
class LabelUpdateLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	struct Chain {
		basic2d::Layer *outer = nullptr;
		basic2d::Layer *chip = nullptr;
		basic2d::Label *label = nullptr;
	};

	// one reference / short-reference / subject triple, sharing a chip main-size limit
	struct Group {
		StringView name;
		float maxMain = 0.0f;
		Chain reference;
		Chain shortReference;
		Chain subject;
	};

	Chain makeChain(StringView text, float maxMain, const Color4F &color);
	void makeGroup(Group &, StringView name, float maxMain);

	// grow the subject text to the reference text
	void runPhase1();
	// compare every level against the reference
	void runPhase2();
	// shrink it back: the ancestors must collapse again
	void runPhase3();
	void runPhase4();

	void compare(StringView phase, const Group &, StringView what, const Size2 &subject,
			const Size2 &reference);

	// the fixed-size flex column holding every row; the chains are its items, so their own sizes
	// are measured rather than assigned
	basic2d::Layer *_column = nullptr;

	Group _free;
	Group _clamped;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LABELUPDATELAYOUT_H_
