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
#include "XLUiStyleResolver.h" // IWYU pragma: keep - document::ParameterName below

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// `calc()` and per-node custom properties (ui::StyleVariables) - the pair that makes a per-element
// number expressible in CSS at all.
//
// A Metric carries ONE value and ONE unit, so calc() folds an expression down to exactly that: a
// sum only combines like with like, a product needs a plain number on one side, and a divisor must
// be a plain number. An expression that cannot reduce (`100% - 20px`, `2px * 3px`) is not an
// approximation to be salvaged - the declaration is dropped, like any other unparseable value, and
// whatever less specific rule stands is used instead. Those failures are pinned here alongside the
// arithmetic, because silently yielding a wrong length is the failure mode that would matter.
//
// The other half is the channel: a stylesheet rule reaches a SET of nodes, so a value that differs
// per node has nowhere to live in the sheet. `setStyleVariable` declares one on a single node; it
// then behaves exactly like a `--name:` declaration written for that node - inherited by the
// subtree, visible to var(), and beating every rule that matched the same node. The runtime part of
// the test is the invalidation: changing it moves nothing and matches no new rule, so only the
// custom-property path can repaint the box.
class CalcLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runArithmetic();
	void runInvalid();
	void runNodeVariables();
	void runRuntimeChange();
	void report();

	// resolved value of a metric parameter, in the units the parameter stores
	void expectMetric(StringView what, Node *, document::ParameterName, float expected);
	void expectNoValue(StringView what, Node *, document::ParameterName);
	void expectVar(StringView what, Node *, StringView name, StringView expected);
	// the size a layout pass actually committed, which is what proves invalidation happened
	void expectAppliedWidth(StringView what, Node *, float expected);

	basic2d::Layer *_sum = nullptr; // calc(8px + 16px)
	basic2d::Layer *_diff = nullptr; // calc(100px - 40px)
	basic2d::Layer *_mulRight = nullptr; // calc(16px * 2)
	basic2d::Layer *_mulLeft = nullptr; // calc(2 * 16px)
	basic2d::Layer *_div = nullptr; // calc(64px / 4)
	basic2d::Layer *_parens = nullptr; // calc((2 + 1) * 10px)
	basic2d::Layer *_nested = nullptr; // calc(((4 + 1)) * 8px)
	basic2d::Layer *_percent = nullptr; // calc(50% + 10%)
	basic2d::Layer *_withVar = nullptr; // calc(var(--pad) * 3)

	basic2d::Layer *_mixedUnits = nullptr; // calc(100% - 20px) - dropped
	basic2d::Layer *_unitSquared = nullptr; // calc(2px * 3px) - dropped
	basic2d::Layer *_divByZero = nullptr; // calc(10px / 0) - dropped
	basic2d::Layer *_unitPlusNumber = nullptr; // calc(10px + 5) - dropped
	basic2d::Layer *_unbalanced = nullptr; // calc(10px + 5px - dropped

	basic2d::Layer *_local = nullptr; // --k declared on the node itself
	basic2d::Layer *_localChild = nullptr; // inherits --k from _local
	basic2d::Layer *_localWins = nullptr; // node-local beats a rule that matched the node
	basic2d::Layer *_normalized = nullptr; // declared as "K", read as --k
	basic2d::Layer *_removed = nullptr; // declaration dropped at runtime

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_CALCLAYOUT_H_
