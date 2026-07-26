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

#ifndef XENOLITH_RENDERER_SIMPLEUI_XLSIMPLESTYLESHEET_H_
#define XENOLITH_RENDERER_SIMPLEUI_XLSIMPLESTYLESHEET_H_

#include "XLSimpleUiConfig.h" // IWYU pragma: keep
#include "SPDocStyleContainer.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

/* A CSS stylesheet for scene-graph nodes, built on the stappler_document CSS engine.

The sheet owns a dedicated memory pool with a `document::DocumentData` (string/media-query
registry) and a `document::StyleContainer` (parsed rules). It is Rc-managed and attached to
a node via `StyleSheetSystem` (see XLSimpleStyle.h).

Selector support mirrors the document engine's matcher: `*`, `tag`, `.class`, `tag.class`,
`#id`, `tag#id`. Descendant/child combinators, attribute selectors, pseudo-classes and
specificity are NOT supported (such rules are parsed but never match); the cascade is
fixed-order last-write-wins. `@media` queries are fully supported. */
class SP_PUBLIC StyleSheet : public Ref {
public:
	virtual ~StyleSheet();

	virtual bool init();
	virtual bool init(StringView css);
	virtual bool init(const FileInfo &);

	// parse CSS text or file into the sheet (additive); returns false on read failure
	bool addStyle(StringView css);
	bool addStyle(const FileInfo &);

	/* Merge all rules matching the given identity into `target`.

	Match/merge order (last wins): `*` (inheritable params only - upstream quirk),
	`tag`, per class `.cls` + `tag.cls`, then `#id` + `tag#id`. Parameters gated by media
	queries are filtered against `mediaResolved` and flattened.

	MUST be called within a memory pool context (target is pool-backed). */
	void resolveForIdentity(document::StyleList &target, StringView type, StringView id,
			const HashSet<String, sprt::hash<void>> &classes, SpanView<bool> mediaResolved) const;

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

} // namespace stappler::xenolith::simpleui

#endif /* XENOLITH_RENDERER_SIMPLEUI_XLSIMPLESTYLESHEET_H_ */
