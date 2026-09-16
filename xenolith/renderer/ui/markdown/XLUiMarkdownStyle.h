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

/* Resolves the stylesheet's rules for an inline construct. Inlines are style ranges, not nodes, so
a temporary probe node is built, queried through `StyleResolver::resolveStyleForNode` (a static
that walks parents and needs no frame or running scene) and removed. Inlines are therefore styled
during the build.

The probe hangs under the block's Label, never among the blocks: applyIdentity derives z-order
(document order) from the child count, and `:nth-child` counts every child.

The result is a delta over the block's style: a property is taken only when the probe declared it
(`has()`) and its value differs from the block's, so defaults and inherited values are not
re-emitted. `LabelBase::Style` has twelve properties; paragraph-level ones (`white-space`,
`text-align`, `line-height`) stay with the block. */
class SP_PUBLIC MarkdownInlineResolver final {
public:
	// The controller maps a family name to a range style's index; without one, an inline
	// `font-family` is dropped.
	explicit MarkdownInlineResolver(font::FontController * = nullptr);

	/* The delta the chain `strong>em` (outermost first) adds over `label`'s own text style. False
	when no sheet is in scope or nothing declares anything for the chain; the caller then uses the
	built-in table. */
	bool resolve(NotNull<basic2d::Label>, StringView chain, basic2d::Label::Style &out);

	// Did any stylesheet answer at all? False for a view with no sheet in scope.
	bool isValid() const { return _valid; }

	// How many probes were actually resolved (cache misses).
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
