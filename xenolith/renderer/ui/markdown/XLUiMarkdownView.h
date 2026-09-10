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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIEW_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIEW_H_

#include "XLUiMarkdownBuilder.h"
#include "XLUiStyleSystem.h"
#include "XLUiScrollSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A Markdown document, rendered as ordinary scene nodes and styled by ordinary CSS.

	auto view = addChild(Rc<ui::MarkdownView>::create());
	view->setSourceFile(FileInfo{"README.md", FileCategory::Bundled});

WHAT IT IS MADE OF. Every block becomes a node typed with its html tag (`p`, `h1`, `li`, `td`)
and classed `md-<tag>`; every inline construct becomes a STYLE RANGE inside the block's Label
rather than a node of its own, which is what lets a line break fall inside a bold phrase. See
ui::MarkdownBuilder for the shape of the tree and ui::MarkdownRegistry for replacing one tag.

STYLING, AND HOW TO OVERRIDE IT. The view carries its own stylesheet so a document is readable
with no application CSS at all, and that sheet is written with bare tag selectors on purpose. The
cascade sorts by specificity and only breaks ties by which sheet is nearer, and the built-in one
is the nearest - so an application rule of EQUAL specificity loses. Two routes win:

  .md-p { color: #b71c1c; }        // a class beats a tag, always
  view->addStyle(".md-p { … }");   // or append into the view's own sheet

`getDefaultStyleSheet()` returns the built-in text, and replacing the sheet outright
(`getStyleSystem()->setStyleSheet(…)`) turns it off.

WHAT IT DOES NOT DO YET. Selection, copying and link activation are later milestones. What this
milestone already records for them is on every produced Label: a `MarkdownRunMap` saying which
bytes of the source each character came from, and where the links are. */
class SP_PUBLIC MarkdownView : public Node {
public:
	virtual ~MarkdownView() = default;

	virtual bool init() override;
	virtual bool init(StringView markdown);
	virtual bool init(const FileInfo &);

	virtual void handleEnter(Scene *) override;

	/* The view's box is its owner's decision, and never its own content's.

	A node carrying a HandleMeasure system - and a LayoutSystem is one - fixes its own size from its
	content during the measure phase. For a document that is exactly backwards: the width is the
	question the caller answers and the wrapping is the answer, so a view that grew to its widest
	unwrapped paragraph would stop every paragraph in it from ever wrapping. */
	virtual void handleMeasure() override { }

	// --- content ---

	// Parse and show. Building is deferred until the view is in a scene: the monospace family
	// index comes from the FontController, which only exists from `handleEnter` on.
	void setSource(StringView markdown);
	void setSourceFile(const FileInfo &);
	void setDocument(Rc<document::DocumentMarkdown> &&);

	document::DocumentMarkdown *getDocument() const { return _document; }

	// The text every source span indexes into; empty without a document.
	StringView getSource() const;

	// Discard the node tree and build it again. What a registry swap and a live style reload
	// both go through.
	void rebuild();

	// --- styling ---

	// The built-in sheet, so a test can assert on what actually ships.
	static StringView getDefaultStyleSheet();

	StyleSystem *getStyleSystem() const { return _styleSystem; }

	// Append into the view's OWN sheet: later source order breaks the tie against the built-in
	// rules, which is the second way an application overrides them.
	bool addStyle(StringView css);
	bool addStyle(const FileInfo &);

	// --- inline appearance ---

	const MarkdownInlineStyles &getInlineStyles() const { return _inlineStyles; }
	void setInlineStyles(const MarkdownInlineStyles &);

	// --- extension ---

	void setRegistry(Rc<MarkdownRegistry> &&);
	MarkdownRegistry *getRegistry() const { return _registry; }

	// --- tree ---

	// The node the blocks are built under; the view itself also carries the systems.
	Node *getContentNode() const { return _content; }

	// The vertical scroll the sheet's `overflow-y` asks for; null until the first style pass.
	ScrollSystem *getScrollSystem() const { return getSystemByType<ScrollSystem>(); }

	uint32_t getBlockCount() const { return _blocks; }

	// --- links ---

	// Registered now, fired by a later milestone: this milestone paints link ranges and records
	// their targets, but has no hit testing of its own yet.
	void setLinkCallback(Function<void(StringView href, StringView title)> &&);

protected:
	// Resolve what only a live scene can answer (the monospace family) and fold it into the
	// inline table. Returns true when the table changed.
	bool updateInlineStyles();

	Rc<document::DocumentMarkdown> _document;
	Rc<MarkdownRegistry> _registry;
	StyleSystem *_styleSystem = nullptr;
	Node *_content = nullptr;
	MarkdownInlineStyles _inlineStyles;
	Function<void(StringView, StringView)> _linkCallback;
	uint32_t _blocks = 0;
	bool _treeDirty = true;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIEW_H_
