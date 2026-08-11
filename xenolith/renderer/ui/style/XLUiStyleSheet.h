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

#ifndef XENOLITH_RENDERER_UI_STYLE_XLUISTYLESHEET_H_
#define XENOLITH_RENDERER_UI_STYLE_XLUISTYLESHEET_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "SPDocStyleContainer.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A CSS stylesheet for scene-graph nodes, built on the stappler_document CSS engine.

The sheet owns a dedicated memory pool with a `document::DocumentData` (string/media-query
registry) and a `document::StyleContainer` (parsed rules). It is Rc-managed and attached to
a node via `StyleSystem` (see XLUiStyleSystem.h).

Selector support:
`*`, `tag`, `.class`, `tag.class`, `#id`, `tag#id`, compound selectors, the four combinators
(`A B`, `A > B`, `A + B`, `A ~ B`), interactive pseudo-classes (`:hover` `:focus` `:active`
`:checked` `:enabled` `:disabled`) and structural pseudo-classes (`:nth-child()`
`:nth-last-child()` `:first-child` `:last-child` `:only-child`, the four `*-of-type` forms,
`:empty`, `:root`).

Combinator rules are bucketed by their rightmost ("target") compound and matched right-to-left
against the scene-graph tree. Full CSS specificity is honored by the caller (see
`collectMatches`). Attribute selectors and pseudo-elements are NOT supported (such rules are
dropped at parse time). `@media` queries are fully supported. */
class SP_PUBLIC StyleSheet : public Ref {
public:
	virtual ~StyleSheet();

	virtual bool init(uint32_t initVersion = 0);
	virtual bool init(StringView css, uint32_t initVersion = 0);
	virtual bool init(const FileInfo &, uint32_t initVersion = 0);

	// parse CSS text or file into the sheet (additive); returns false on read failure
	bool addStyle(StringView css);
	bool addStyle(const FileInfo &);

	/* Append every rule matching `node` (simple string-keyed selectors + structured
	combinator/pseudo selectors) to `out`, each as a MatchedRule carrying its CSS specificity
	and source order - WITHOUT merging. The caller gathers matches across all in-scope sheets,
	sorts them by (specificity, order) and merges in that order, so the full CSS cascade is
	honored (a `#id` rule beats a lower-specificity `.a .b`, ties broken by source order).

	`ancestorFilterBits` is the Bloom filter of the node's ancestor tokens (lets descendant/
	child rules be rejected in O(1) before the right-to-left walk). `orderBias` folds the
	sheet's scope rank into the tie-break; `mediaResolved` is stamped on each matched rule and
	applied when the caller merges. `scopeRoot` is the node owning the nearest stylesheet
	scope - what `:root` matches. Must be called within a memory pool context. */
	void collectMatches(Vector<document::StyleContainer::MatchedRule> &out, NotNull<Node> node,
			uint64_t ancestorFilterBits, uint64_t orderBias, SpanView<bool> mediaResolved,
			const Node *scopeRoot) const;

	// does any rule in the sheet use a structural pseudo-class? A node's style then depends on
	// its position among its siblings, so the resolver must invalidate on child-list changes.
	bool hasStructuralSelectors() const;

	// does any rule declare a custom property or reference one with var()? Lets the cascade
	// skip its custom-property pass for an ordinary sheet.
	bool hasCustomProperties() const;

	/* Parse an inline `style="..."` declaration list; parsed once per distinct text,
	cached for the sheet's lifetime. Returned pointer is valid while the sheet lives. */
	const document::StyleList *getInlineStyle(StringView css);

	// evaluate the sheet's @media queries against the environment
	Vector<bool> resolveMedia(const document::MediaParameters &) const;

	// interned strings (font families, image urls) for document::SimpleStyleInterface
	SpanView<StringView> getStrings() const;

	// incremented on every addStyle; used by systems to detect sheet changes
	uint32_t getVersion() const { return _version; }

protected:
	struct Container;

	memory::pool_t *_pool = nullptr;
	document::DocumentData *_data = nullptr;
	Container *_container = nullptr;
	uint32_t _version = 0;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_STYLE_XLUISTYLESHEET_H_ */
