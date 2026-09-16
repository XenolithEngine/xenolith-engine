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
#include "XLUiMarkdownVirtual.h"
#include "XLUiStyleSystem.h"
#include "XLUiScrollSystem.h"
#include "XLSelectionSystem.h"
#include "XLClipboard.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A Markdown document, rendered as ordinary scene nodes and styled by ordinary CSS.

	auto view = addChild(Rc<ui::MarkdownView>::create());
	view->setSourceFile(FileInfo{"README.md", FileCategory::Bundled});

Every block becomes a node typed with its html tag (`p`, `h1`, `li`, `td`) and classed
`md-<tag>`; every inline construct is a style range inside the block's Label, so a line break can
fall inside a bold phrase. See ui::MarkdownBuilder and ui::MarkdownRegistry.

The built-in stylesheet uses bare tag selectors and is the nearest sheet, so an application rule
of equal specificity loses. Override with a class (`.md-p { … }`) or `view->addStyle(…)`;
`getStyleSystem()->setStyleSheet(…)` replaces the built-in sheet.

Positions follow ui::MarkdownFlow's reading order. A range can be read as text
(`getTextForRange`) or as the original Markdown (`getMarkupForRange`), closed up where an edge
cuts a construct. Selection gestures live in ui::MarkdownSelectionSystem; as a `SelectionOwner`
the view shares the scene's single selection with other owners. */
class SP_PUBLIC MarkdownView : public Node, public SelectionOwner {
public:
	virtual ~MarkdownView() = default;

	virtual bool init() override;
	virtual bool init(StringView markdown);
	virtual bool init(const FileInfo &);

	virtual void handleEnter(Scene *) override;

	/* A stylesheet reload: the first for the view's own sheet, the second for an ancestor's.
	Only the labels' inline style ranges are re-resolved; the tree, reading order and selection
	survive. */
	virtual void handleComponentsDirty(const ComponentMask &) override;
	virtual void handleAncestorComponentsDirty() override;

	// Restyling is deferred to here: the hooks above fire during the resolver's walk of this
	// subtree, which must not have probes added or removed under it.
	virtual void update(const UpdateTime &) override;

	// The owner sets the view's size; it never measures itself from content, or paragraphs
	// would stop wrapping.
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

	// Discard the node tree and build it again.
	void rebuild();

	// --- reading order ---

	// The document in reading order; never null, empty until the first build.
	MarkdownFlow *getFlow() const { return _flow; }

	// The last position of the document: a range is a pair inside `[0, getTextLength()]`.
	uint32_t getTextLength() const { return _flow->getLength(); }

	// The bytes of the source a range of positions covers, edges moved outward wherever the
	// mapping is not exact. `getMarkupForRange` is this plus document::writeMarkdownFragment.
	Pair<uint32_t, uint32_t> getSourceRangeForTextRange(uint32_t begin, uint32_t end) const;

	// The original markup for a range: what a copy puts on the clipboard.
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

	// Show a range as selected. All selection paths go through here, and it claims the scene's
	// selection for the document.
	void setSelectionRange(uint32_t begin, uint32_t end);
	void selectAll();
	void clearSelection();

	String getSelectedText() const;
	String getSelectedMarkup(document::MarkdownMarkup = document::MarkdownMarkup::Normalized) const;

	// The colour every label paints its highlight with; also read from `--md-selection-color`.
	void setSelectionColor(const Color4F &);
	Color4F getSelectionColor() const { return _selectionColor; }

	// --- copying ---

	/* What a copy may put on the clipboard. `Both`: markup first, then plain text; `TextOnly`:
	plain text only; `Nothing`: copying is refused. */
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

	// --- long documents ---

	// Only the visible part is laid out and shaped (see ui::MarkdownVirtualizer); enabled past
	// the virtualization threshold.
	const MarkdownVirtualizer &getVirtualizer() const { return _virtual; }

	// Block count at which virtualization starts; maxOf disables it.
	void setVirtualizationThreshold(uint32_t);
	uint32_t getVirtualizationThreshold() const { return _virtualThreshold; }

	// --- cost ---

	// Cost of the last document: times in nanoseconds, plus counts.
	struct Timings {
		uint64_t parse = 0; // the markdown parser, over the whole source
		uint64_t build = 0; // the node tree, from the parsed document
		uint64_t restyle = 0; // re-resolving the inline ranges after a stylesheet reload

		uint32_t blocks = 0;
		uint32_t sourceLength = 0;

		// Cascade probes the last build resolved; cached, so far fewer than the range count.
		uint32_t probes = 0;
	};

	const Timings &getTimings() const { return _timings; }

	// --- styling ---

	// The built-in stylesheet text.
	static StringView getDefaultStyleSheet();

	StyleSystem *getStyleSystem() const { return _styleSystem; }

	// Append into the view's own sheet; later source order wins ties against built-in rules.
	bool addStyle(StringView css);
	bool addStyle(const FileInfo &);

	// --- inline appearance ---

	const MarkdownInlineStyles &getInlineStyles() const { return _inlineStyles; }
	void setInlineStyles(const MarkdownInlineStyles &);

	// --- images ---

	/* Replaces image resolution. The default reads a local file relative to `setImageBase`, sizing
	it from the header; a non-local `src` (a URL) draws nothing. A resolver returns a texture and
	a box, either may be empty; an empty box shows the alt text. */
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

	// Fired by a tap on a link that points outside the document; in-document anchors (`#fn_1`)
	// are followed by the view itself. A drag starting on a link selects instead.
	void setLinkCallback(Function<void(StringView href, StringView title)> &&);
	void handleLinkActivated(const MarkdownRunMap::Link &);

	// Every `id` in the document, mapped to the reading position it names. Includes the ids of
	// inline elements - a footnote's reference site is one, and it has no node.
	const Map<String, uint32_t> &getAnchors() const { return _anchors; }

	// The position an `#id` names, or maxOf<uint32_t>() when the document has no such id. A
	// leading '#' is accepted, so an href can be passed straight in.
	uint32_t getAnchorPosition(StringView id) const;

	// Bring an anchor into view. False when the document has no such id, which
	// `handleLinkActivated` treats as an outward link.
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
	// StyleSystemState from this node up, since the own and ancestor sheets change independently.
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

	Timings _timings;
	MarkdownVirtualizer _virtual;
	uint32_t _virtualThreshold = MarkdownVirtualizer::kMinBlocks;

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
