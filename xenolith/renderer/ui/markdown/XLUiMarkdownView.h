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
#include "XLUiMarkdownSelection.h"
#include "XLUiStyleSystem.h"
#include "XLUiScrollSystem.h"
#include "XLSelectionSystem.h"
#include "XLClipboard.h"

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

READING A RANGE. The document has a reading order of its own (ui::MarkdownFlow), numbered across
every block, and two questions can be asked of any range in it: what it SAID (`getTextForRange`)
and how it was WRITTEN (`getMarkupForRange`). The second is the one this whole feature exists for -
a range comes back as the original Markdown, closed up wherever an edge cut a construct in half.

SELECTING AND COPYING. A drag, a double or triple click, Ctrl+A or `setSelectionRange` all end in
the same place: a range of reading positions, painted by the labels themselves and copied out as
the markup that produced it. `ui::MarkdownSelectionSystem` holds the gestures and explains how
they are kept away from the two scrolls in this tree.

The view is a `SelectionOwner`, which is what makes "one selection per scene" true for a document
too: selecting a row in a tree elsewhere puts this one out, and the notification comes back here
so the highlight goes with it. */
class SP_PUBLIC MarkdownView : public Node, public SelectionOwner {
public:
	virtual ~MarkdownView() = default;

	virtual bool init() override;
	virtual bool init(StringView markdown);
	virtual bool init(const FileInfo &);

	virtual void handleEnter(Scene *) override;

	/* A stylesheet reload arrives at one of these two, and which one depends on WHOSE sheet moved:
	the view's own (`addStyle`, a watched file) bumps a component on this node, an application's
	sheet on an ancestor bumps one up there.

	The resolver restyles every node of the subtree by itself; what it cannot reach is the style
	RANGES inside the labels, which were resolved when the text was committed. Those are redone -
	and only those, so the tree, the reading order and the selection all survive a reload. */
	virtual void handleComponentsDirty(const ComponentMask &) override;
	virtual void handleAncestorComponentsDirty() override;

	// Deferred, because both hooks above fire while the resolver is walking this subtree, and a
	// probe added or removed under a Label in the middle of that walk would move the ground the
	// walk stands on.
	virtual void update(const UpdateTime &) override;

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

	// --- reading order ---

	// The document in reading order; never null, empty until the first build.
	MarkdownFlow *getFlow() const { return _flow; }

	// The last position of the document: a range is a pair inside `[0, getTextLength()]`.
	uint32_t getTextLength() const { return _flow->getLength(); }

	// The bytes of the source a range of positions covers, edges moved outward wherever the
	// mapping is not exact. `getMarkupForRange` is this plus document::writeMarkdownFragment.
	Pair<uint32_t, uint32_t> getSourceRangeForTextRange(uint32_t begin, uint32_t end) const;

	// The ORIGINAL markup for a range: what a copy puts on the clipboard.
	void writeMarkupForRange(const Callback<void(StringView)> &, uint32_t begin, uint32_t end,
			document::MarkdownMarkup = document::MarkdownMarkup::Normalized) const;
	String getMarkupForRange(uint32_t begin, uint32_t end,
			document::MarkdownMarkup = document::MarkdownMarkup::Normalized) const;

	// The visible text of the same range.
	void writeTextForRange(const Callback<void(StringView)> &, uint32_t begin, uint32_t end) const;
	String getTextForRange(uint32_t begin, uint32_t end) const;

	// --- selection ---

	MarkdownSelectionSystem *getSelectionSystem() const { return _selection; }

	bool hasSelection() const { return _selectionEnd > _selectionBegin; }
	Pair<uint32_t, uint32_t> getSelectionRange() const {
		return pair(_selectionBegin, _selectionEnd);
	}

	/* Show a range as selected. Everything that selects goes through here - the gestures, the
	hotkeys, and a caller driving the widget - so this is also where the scene is told that the
	document now holds the one selection it allows. */
	void setSelectionRange(uint32_t begin, uint32_t end);
	void selectAll();
	void clearSelection();

	String getSelectedText() const;
	String getSelectedMarkup(document::MarkdownMarkup = document::MarkdownMarkup::Normalized) const;

	// The colour every label paints its highlight with; also read from `--md-selection-color`.
	void setSelectionColor(const Color4F &);
	Color4F getSelectionColor() const { return _selectionColor; }

	// --- copying ---

	/* WHAT MAY LEAVE THE WIDGET, which is a policy and therefore the caller's to set. `Both` puts
	the markup on the clipboard first and the plain text after it; `TextOnly` withholds the markup
	from a document shown in confidence; `Nothing` refuses. */
	enum class CopyPolicy {
		Both,
		TextOnly,
		Nothing,
	};

	void setCopyPolicy(CopyPolicy policy) { _copyPolicy = policy; }
	CopyPolicy getCopyPolicy() const { return _copyPolicy; }

	// Put the selection on the clipboard. False when there is nothing to copy, the policy refuses,
	// or the transport cannot carry it.
	bool copy(document::MarkdownMarkup = document::MarkdownMarkup::Normalized);

	// --- SelectionOwner ---

	virtual Node *getSelectionOwnerNode() override { return this; }
	virtual Node *resolveSelectionNode(const SelectionItem &) const override;
	virtual void handleSelectionChanged(SpanView<SelectionItem>) override;

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

	// --- images ---

	/* WHERE THE PICTURES COME FROM.

	The default reads a local file: `src` is resolved against `setImageBase` (which `setSourceFile`
	fills in with the document's own directory), the size is taken from the file's header and the
	decode is left to the render loop. A `src` that is not a local path - a URL above all - draws
	nothing, because this widget depends on neither the network nor a storage backend.

	An application that wants more replaces the whole step. It is handed what the markup said and
	answers with a texture and a box; either may be empty, and an empty box means "show the alt
	text instead". */
	void setImageResolver(MarkdownImageResolver &&);

	// The directory a relative `src` is resolved against. Set for you by setSourceFile, from the
	// document's own location.
	void setImageBase(StringView path, FileCategory = FileCategory::Custom);
	StringView getImageBase() const { return _imageBasePath; }

	// --- extension ---

	void setRegistry(Rc<MarkdownRegistry> &&);
	MarkdownRegistry *getRegistry() const { return _registry; }

	// --- tree ---

	// The node the blocks are built under; the view itself also carries the systems.
	Node *getContentNode() const { return _content; }

	// The vertical scroll the sheet's `overflow-y` asks for; null until the first style pass.
	ScrollSystem *getScrollSystem() const { return getSystemByType<ScrollSystem>(); }

	uint32_t getBlockCount() const { return _blocks; }

	// --- links and anchors ---

	/* Fired by a plain click on a link range that LEFT the document. A drag that begins on a link
	is a selection, not a visit, so only a tap gets here - and a link into the document itself
	(`#fn_1`) is followed by the view before the callback is consulted, so an application handles
	only what it can actually act on. */
	void setLinkCallback(Function<void(StringView href, StringView title)> &&);
	void handleLinkActivated(const MarkdownRunMap::Link &);

	// Every `id` in the document, mapped to the reading position it names. Includes the ids of
	// inline elements - a footnote's reference site is one, and it has no node.
	const Map<String, uint32_t> &getAnchors() const { return _anchors; }

	// The position an `#id` names, or maxOf<uint32_t>() when the document has no such id. A
	// leading '#' is accepted, so an href can be passed straight in.
	uint32_t getAnchorPosition(StringView id) const;

	/* Bring an anchor into view. False when the document has no such id - which is what
	`handleLinkActivated` uses to decide that a link points outward.

	A footnote and its way back are the same operation in both directions, and so is a table of
	contents: any `#id` a heading carries works the same way. */
	bool scrollToAnchor(StringView id);

protected:
	// Resolve what only a live scene can answer (the monospace family) and fold it into the
	// inline table. Returns true when the table changed.
	bool updateInlineStyles();

	// Re-resolve every label's style ranges against the sheet as it stands now.
	void restyleInlines();

	// Read `--md-selection-color` off the view's own resolved style, if the sheet declares it.
	void updateSelectionColor();

	// Note that some sheet in scope moved, and arrange for the ranges to be resolved again.
	void invalidateInlineStyles();

	// A number that changes whenever any stylesheet in scope does: the versions of every
	// StyleSystemState from this node up. One counter cannot do - the view carries its own sheet
	// AND an application may put one above it, and either may move without the other.
	uint32_t getStyleGeneration() const;

	ClipboardSession *acquireClipboard();

	// The built-in resolver: a local file, its extent read from the header.
	MarkdownImageSource resolveImage(const MarkdownImageRequest &);

	// The menu a right click or a long press opens over the document.
	void buildContextMenu();

	Rc<document::DocumentMarkdown> _document;
	Map<String, uint32_t> _anchors;
	MarkdownImageResolver _imageResolver;
	String _imageBasePath;
	FileCategory _imageBaseCategory = FileCategory::Custom;

	// The stylesheet generation the labels' ranges were resolved at; a reload moves it.
	uint32_t _styleVersion = 0;
	bool _inlineStylesDirty = false;
	Rc<MarkdownRegistry> _registry;
	Rc<MarkdownFlow> _flow;
	Rc<ClipboardSession> _clipboard;
	MarkdownSelectionSystem *_selection = nullptr;
	Color4F _selectionColor = Color4F(Color::LightBlue_200);
	uint32_t _selectionBegin = 0;
	uint32_t _selectionEnd = 0;
	CopyPolicy _copyPolicy = CopyPolicy::Both;
	StyleSystem *_styleSystem = nullptr;
	Node *_content = nullptr;
	MarkdownInlineStyles _inlineStyles;
	Function<void(StringView, StringView)> _linkCallback;
	uint32_t _blocks = 0;
	bool _treeDirty = true;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIEW_H_
