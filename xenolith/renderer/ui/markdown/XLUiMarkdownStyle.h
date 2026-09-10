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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNSTYLE_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNSTYLE_H_

#include "XLUiMarkdownTypes.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* WHAT THE STYLESHEET SAYS ABOUT AN INLINE CONSTRUCT.

An inline is a style range, not a node, so the cascade has nothing to reach: there is no `strong`
element for a `strong { }` rule to match. This class manufactures one for the length of a
question - a PROBE - asks the cascade about it, and throws it away.

WHY THIS CAN BE DONE WHILE THE TREE IS BEING BUILT. `StyleResolver::resolveStyleForNode` is a
plain static that computes the cascade on the spot: it walks `getParent()`, collects the sheets
it finds on the way and answers. It needs no frame, no visit and no running scene - only an
ancestor carrying a StyleSystem, and MarkdownView has had one since its own init(). So an inline
is styled in the same pass that produces it, and nothing is ever shown unstyled.

WHERE THE PROBE HANGS, and why it is not a detail. It goes UNDER THE BLOCK'S LABEL, which is a
leaf, never beside the blocks. Two things in this tree count children:

  - MarkdownBuilder::applyIdentity takes a node's z-order from its parent's child count, and
    z-order here IS document order;
  - `:nth-child` counts every child of the parent.

A probe parked among the blocks would silently move a paragraph and renumber its siblings. Under
a Label neither is true, and the descendant chain a rule matches on is the same either way.

A DELTA, NOT A STYLE. A range style is applied over the block's own, so only what DIFFERS from
the block may be emitted. `ResolvedStyle` answers with the CSS default for a property nobody
declared, which would make every unstyled property look like a deliberate one - so a property is
taken only when the probe actually declared it (`has()`) AND its value differs from the block's.
That second half is what keeps an inherited value - the colour of the paragraph, reaching the
probe by inheritance - from being re-emitted as a range of its own.

WHAT A RANGE CANNOT CARRY. `LabelBase::Style` names twelve properties; `white-space`,
`text-align` and `line-height` are not among them and stay with the block, which is correct -
they are properties of a paragraph, not of a word inside it. */
class SP_PUBLIC MarkdownInlineResolver final {
public:
	// The controller resolves a family NAME into the index a range style carries. Without one, a
	// `font-family` on an inline is dropped rather than guessed.
	explicit MarkdownInlineResolver(font::FontController * = nullptr);

	/* The delta the chain `strong>em` (outermost first) adds over `label`'s own text style.

	False means "the sheet had nothing to say about this" - either no stylesheet is in scope, or
	nothing in it declares anything for the chain. The caller then falls back to the built-in
	table, which is why a view outside any scene still renders bold as bold. */
	bool resolve(NotNull<basic2d::Label>, StringView chain, basic2d::Label::Style &out);

	// Did any stylesheet answer at all? False for a view with no sheet in scope.
	bool isValid() const { return _valid; }

	// How many probes were actually resolved: the cost of the pass, and how a test sees that the
	// cache is doing its job.
	uint32_t getProbeCount() const { return _probes; }

protected:
	// The block's own resolved style, cached per identity path; nullptr when no sheet answered.
	const ResolvedStyle *acquireBaseline(basic2d::Label *, StringView contextKey);

	font::FontController *_controller = nullptr;

	// Identity path of a block -> its resolved style.
	Map<String, ResolvedStyle> _baselines;

	// Identity path + '|' + chain -> the delta. Not written while a sheet in scope uses a
	// structural selector: two blocks with the same identity path can then resolve differently
	// because of where they sit among their siblings.
	Map<String, basic2d::Label::Style> _chains;

	bool _valid = false;
	bool _structural = false;
	uint32_t _probes = 0;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNSTYLE_H_
