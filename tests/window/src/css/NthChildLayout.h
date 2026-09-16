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

#ifndef TESTS_WINDOW_SRC_CSS_NTHCHILDLAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_NTHCHILDLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// CSS structural pseudo-classes: `:nth-child()`, `:nth-last-child()`, `:first-child`,
// `:last-child`, `:only-child`, the four `*-of-type` forms, `:empty` and `:root`.
//
// Two halves. The static half matches selectors against a fixed tree and reads the answer
// straight out of `resolveStyleForNode` - it pins parsing, An+B arithmetic, the same-type
// filter and specificity. The dynamic half owns a recursive resolver and mutates the child
// list afterwards: inserting, removing and re-ordering a sibling all change which rule matches
// its NEIGHBOURS, and nothing but the parent's child-list version can tell the resolver that.
//
// Sibling order is z-order, not insertion order (`Node::sortAllChildren`), exactly as for the
// `+`/`~` combinators - the reorder case pins that too.
class NthChildLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	// build a `.list` container holding `count` `.item` swatches, styled by `cls`
	basic2d::Layer *makeList(StringView cls, uint32_t count);

	// re-place every row; also called after each mutation, since a plain container does not
	// re-position its children on its own and the rows would otherwise show stale geometry
	void layoutRows();

	void runStatic();
	void runDynamicInsert();
	void runDynamicRemove();
	void runDynamicReorder();
	void report();

	// resolved (not yet applied) background colour of a node
	void expectResolved(StringView what, Node *, const Color4B &);
	// colour actually applied to a layer by the recursive resolver
	void expectApplied(StringView what, basic2d::Layer *, const Color4B &);
	void expectTrue(StringView what, bool);

	Vector<basic2d::Layer *> _staticLists;

	// the dynamic container: a `.dyn` list whose items are coloured by :first-child/:last-child
	basic2d::Layer *_dynList = nullptr;
	basic2d::Layer *_inserted = nullptr;

	// same mutation under a plain Node parent that runs no layout - the documented residual case
	Node *_plainParent = nullptr;
	basic2d::Layer *_plainFirst = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_NTHCHILDLAYOUT_H_
