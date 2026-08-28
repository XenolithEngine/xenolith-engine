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

#ifndef TESTS_WINDOW_SRC_CSS_CALCLAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_CALCLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// `calc()`, `min()`/`max()`/`clamp()` and per-node custom properties (ui::StyleVariables) - the
// pair that makes a per-element number expressible in CSS at all.
//
// A Metric carries ONE value and ONE unit, so the arithmetic folds an expression down to exactly
// that: a sum only combines like with like, a product needs a plain number on one side, a divisor
// must be a plain number, and the three functions pick among arguments that are alike. An
// expression that cannot reduce (`100% - 20px`, `2px * 3px`, `clamp(1px, 2em, 3px)`) is not an
// approximation to be salvaged - the declaration is dropped, like any other unparseable value, and
// whatever less specific rule stands is used instead. Those failures are asserted alongside the
// arithmetic, because silently yielding a wrong length is the failure mode that would matter.
//
// The other half is the channel: a stylesheet rule reaches a SET of nodes, so a value that differs
// per node has nowhere to live in the sheet. `setStyleVariable` declares one on a single node; it
// then behaves exactly like a `--name:` declaration written for that node - inherited by the
// subtree, visible to var(), and beating every rule that matched the same node. The runtime part is
// the invalidation: changing it moves nothing and matches no new rule, so only the custom-property
// path can repaint the box - which is why the check reads the APPLIED width and not just the
// resolved one.
//
// Driven headless by style-check.py. It used to check itself and write the tally to the log, where
// nobody ran it; the expectations now live in the script, duplicated on purpose, and the stand only
// reports what resolved.
class CalcLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	struct Sample {
		String name;
		basic2d::Layer *node = nullptr;
	};

	basic2d::Layer *addBox(Node *parent, StringView cls);

	Value encodeSample(const Sample &) const;

	Node *getTarget(const Value &args) const;

	Vector<Sample> _samples;

	basic2d::Layer *_local = nullptr; // carries --k, and a child that inherits it
	basic2d::Layer *_removed = nullptr; // --k is taken away at runtime
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_CALCLAYOUT_H_
