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

#ifndef TESTS_WINDOW_SRC_CSS_SELECTORLAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_SELECTORLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* `:not()`, `:is()` and `:where()` - the three functional pseudo-classes, and the specificity each
one carries.

Two of the three claims here are about MATCHING and can be seen on a screen; the third is about
SPECIFICITY and cannot. `:where()` exists precisely because it matches like `:is()` while counting
for nothing, so the only way to tell the two apart is to put a rule that uses one against a rule
that would otherwise lose to it - which is a number, not a picture. That is what most of this stand
is: pairs of rules written to conflict, with the winner asserted by style-check.py.

The argument of all three is ONE COMPOUND, with no combinator and no functional pseudo-class inside
it. The refusals are pinned here beside the arithmetic, because a selector that is quietly
half-applied is worse than one that is refused: the rules that follow it in the sheet must survive,
and the rule itself must not. */
class SelectorLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	struct Sample {
		String name;
		basic2d::Layer *node = nullptr;
		// Placed by its parent rather than by the grid below; see handleContentSizeDirty
		bool nested = false;
	};

	// `parent` null puts the sample in the grid; naming one nests it, which is the only way to say
	// anything about INHERITANCE - every other sample here is a sibling of every other.
	basic2d::Layer *addSample(StringView name, StringView type,
			sprt::initializer_list<StringView> classes, Node *parent = nullptr);

	Value encodeSample(const Sample &) const;

	Node *getTarget(const Value &args) const;

	Vector<Sample> _samples;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_SELECTORLAYOUT_H_
