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

#include "XLUiMarkdownView.h"
#include "XLUiContextMenu.h"
#include "XLUiMenuSource.h"
#include "XLUiStyleResolver.h"
#include "XLUiScrollSystem.h"
#include "XLFontController.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* The appearance a document has before an application says anything.

Bare tag selectors, deliberately: specificity 0,0,1 is the lowest a rule can have that still
matches, so `.md-p` from an application sheet outranks every line of this without a fight. The
layout lives here too - `display: flex` is what makes CSS padding and gaps reach a container at
all, and it lets an application reshape the document with rules instead of a subclass.

`white-space: normal` on the prose blocks is not decoration. A Label formats as `pre-wrap` unless
CSS says otherwise, and under `pre-wrap` a newline inside a hard-wrapped source paragraph becomes
a hard break on screen. Markdown means a space there. */
static constexpr auto s_markdownDefaultStyle = StringView(R"css(
/* `align-items: flex-start`, and the body's own `width: 100%` below, are one decision.

A flex container that overflows on an axis sizes that axis by its content, and `stretch` then hands
the item that content size - so a stretched body would be as wide as its widest unwrapped paragraph
and nothing would ever wrap. Aligning to the start leaves the item its own size, and the body says
what that is. */
/* `overflow-y` alone, and it really is alone: the axes are independent (see ui::ScissorAxes), so
the document scrolls vertically and still takes its width from the view - which is what every
paragraph in it wraps to. */
markdown-view {
	overflow-y: auto;
	display: flex;
	flex-direction: column;
	align-items: stretch;
	font-family: sans;
	font-size: 15px;
	line-height: 1.4;
	color: #1a1a1a;
}

markdown-body {
	display: flex;
	flex-direction: column;
	align-items: stretch;
	padding: 16px;
}

p, dd, dt, td, th, caption, figcaption, li-marker {
	white-space: normal;
}

/* The implicit block a run of loose inline text becomes; it is typed `<tag>-text` so it never
matches the layout rule of the container it sits in. */
.md-text { white-space: normal; }

p { margin-bottom: 10px; }

h1, h2, h3, h4, h5, h6 {
	white-space: normal;
	font-weight: bold;
	margin-top: 18px;
	margin-bottom: 10px;
}

h1 { font-size: 28px; }
h2 { font-size: 23px; }
h3 { font-size: 19px; }
h4 { font-size: 17px; }
h5 { font-size: 15px; }
h6 { font-size: 14px; color: #555555; }

ul, ol, dl {
	display: flex;
	flex-direction: column;
	align-items: stretch;
	margin-bottom: 10px;
}

li {
	display: flex;
	flex-direction: row;
	align-items: flex-start;
	column-gap: 8px;
	margin-bottom: 3px;
}

li-checkbox {
	width: 15px;
	height: 15px;
	background-color: #ffffff;
	border-radius: 3px;
	outline-color: #9e9e9e;
	outline-width: 1px;
	outline-style: solid;
}

li-checkbox:checked { background-color: #1565c0; outline-color: #1565c0; }

li-marker {
	min-width: 18px;
	text-align: right;
	color: #666666;
}

li-content {
	display: flex;
	flex-direction: column;
	align-items: stretch;
	flex-grow: 1;
	flex-shrink: 1;
	flex-basis: 0px;
}

blockquote {
	display: flex;
	flex-direction: row;
	align-items: stretch;
	margin-bottom: 12px;
}

blockquote-bar {
	width: 3px;
	background-color: #c8c8c8;
}

blockquote-body {
	display: flex;
	flex-direction: column;
	align-items: stretch;
	flex-grow: 1;
	flex-shrink: 1;
	flex-basis: 0px;
	padding: 6px 12px;
	background-color: #f5f5f5;
}

pre {
	display: flex;
	flex-direction: column;
	align-items: stretch;
	padding: 10px 12px;
	margin-bottom: 12px;
	background-color: #f2f2f2;
	border-radius: 4px;
}

/* The code's own box, kept apart from the painted one: this is the half that scrolls sideways,
and the `pre` above is the half that paints. Splitting them is what lets the panel be as tall as
the code while the code itself is as wide as its longest line. */
pre-scroll {
	display: flex;
	flex-direction: column;
	align-items: flex-start;
	overflow-x: auto;
}

code {
	font-family: monospace;
	font-size: 13px;
	white-space: pre;
}

hr {
	height: 1px;
	background-color: #d8d8d8;
	margin-top: 14px;
	margin-bottom: 14px;
}

table {
	display: table;
	grid-template-columns: var(--md-columns, auto);
	border-collapse: collapse;
	margin-bottom: 12px;
}

tr { display: table-row; }

th, td {
	display: table-cell;
	vertical-align: top;
	margin: 4px 8px;
	border-width: 1px;
	border-style: solid;
	border-color: #d0d0d0;
}

th { font-weight: bold; }

dt { font-weight: bold; }
dd { margin-left: 20px; margin-bottom: 8px; }
)css");

StringView MarkdownView::getDefaultStyleSheet() { return s_markdownDefaultStyle; }

bool MarkdownView::init() {
	if (!Node::init()) {
		return false;
	}

	setType("markdown-view");
	addStyleClass("xl-ui-markdown-view");
	setAnchorPoint(Anchor::BottomLeft);

	_registry = MarkdownRegistry::createDefault();
	_flow = Rc<MarkdownFlow>::alloc();

	// The view's own sheet, plus the one resolver that styles the whole produced subtree. One
	// recursive resolver, never one per node: it publishes itself on the frame stack and every
	// descendant delivers its events back to it.
	_styleSystem = addSystem(Rc<StyleSystem>::create(s_markdownDefaultStyle));
	addSystem(Rc<StyleResolver>::create(true));

	// A separate content node so that rebuilding is one removeAllChildren, and so the systems
	// above (and, later, a selection overlay) are never swept away with the document.
	_content = addChild(Rc<Node>::create());
	_content->setType("markdown-body");
	_content->addStyleClass("md-body");
	_content->setAnchorPoint(Anchor::BottomLeft);

	// After the content node, so the handles it may create sit above the document; its system
	// priority is what puts it ahead of the scroll the sheet asks for.
	_selection = addSystem(Rc<MarkdownSelectionSystem>::create(this));

	return true;
}

bool MarkdownView::init(StringView markdown) {
	if (!init()) {
		return false;
	}
	setSource(markdown);
	return true;
}

bool MarkdownView::init(const FileInfo &file) {
	if (!init()) {
		return false;
	}
	setSourceFile(file);
	return true;
}

void MarkdownView::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	// Not in init(): a node has no scene to hang a menu coordinator on until it is in one.
	buildContextMenu();

	updateInlineStyles();

	if (_treeDirty) {
		rebuild();
	}
}

void MarkdownView::setSource(StringView markdown) {
	setDocument(Rc<document::DocumentMarkdown>::create(
			BytesView(reinterpret_cast<const uint8_t *>(markdown.data()), markdown.size()),
			StringView("text/markdown")));
}

void MarkdownView::setSourceFile(const FileInfo &file) {
	setDocument(Rc<document::DocumentMarkdown>::create(file, StringView("text/markdown")));
}

void MarkdownView::setDocument(Rc<document::DocumentMarkdown> &&doc) {
	_document = sp::move(doc);
	_treeDirty = true;

	// Before enter there is no font controller, so the inline table cannot be completed and the
	// tree cannot be built; handleEnter picks it up.
	if (isRunning()) {
		rebuild();
	}
}

StringView MarkdownView::getSource() const {
	return _document ? _document->getSource() : StringView();
}

void MarkdownView::rebuild() {
	// Every label the selection was painted on is about to be discarded, and the positions it
	// held indexed a document that no longer exists.
	_selectionBegin = _selectionEnd = 0;

	// The flow holds every node it indexes, so it goes first - or a rebuilt document would keep
	// the previous one alive in its own index.
	_flow = Rc<MarkdownFlow>::alloc();
	_content->removeAllChildren();
	_blocks = 0;
	_treeDirty = false;

	if (!_document) {
		return;
	}

	auto page = _document->getRoot();
	if (!page || !page->getRoot()) {
		log::source().warn("ui::MarkdownView", "document has no root page");
		return;
	}

	MarkdownBuilder builder(_content, _registry, _inlineStyles);
	builder.setSource(_document->getSource());
	_blocks = builder.build(*page->getRoot());
	_flow = builder.getFlow();

	// The colour lives on each Label separately, so a fresh tree has to be told again.
	_flow->setSelectionColor(_selectionColor);
}

Pair<uint32_t, uint32_t> MarkdownView::getSourceRangeForTextRange(uint32_t begin,
		uint32_t end) const {
	return _flow->getSourceRange(begin, end);
}

void MarkdownView::writeMarkupForRange(const Callback<void(StringView)> &out, uint32_t begin,
		uint32_t end, document::MarkdownMarkup mode) const {
	if (!_document) {
		return;
	}
	auto range = _flow->getSourceRange(begin, end);
	document::writeMarkdownFragment(out, *_document, range.first, range.second, mode);
}

String MarkdownView::getMarkupForRange(uint32_t begin, uint32_t end,
		document::MarkdownMarkup mode) const {
	String ret;
	writeMarkupForRange([&](StringView str) { ret.append(str.data(), str.size()); }, begin, end,
			mode);
	return ret;
}

void MarkdownView::writeTextForRange(const Callback<void(StringView)> &out, uint32_t begin,
		uint32_t end) const {
	_flow->writeText(out, begin, end);
}

String MarkdownView::getTextForRange(uint32_t begin, uint32_t end) const {
	String ret;
	writeTextForRange([&](StringView str) { ret.append(str.data(), str.size()); }, begin, end);
	return ret;
}

bool MarkdownView::addStyle(StringView css) {
	return _styleSystem ? _styleSystem->addStyle(css) : false;
}

bool MarkdownView::addStyle(const FileInfo &file) {
	return _styleSystem ? _styleSystem->addStyle(file) : false;
}

void MarkdownView::setInlineStyles(const MarkdownInlineStyles &styles) {
	if (_inlineStyles == styles) {
		return;
	}
	_inlineStyles = styles;
	if (isRunning()) {
		rebuild();
	} else {
		_treeDirty = true;
	}
}

void MarkdownView::setRegistry(Rc<MarkdownRegistry> &&registry) {
	if (!registry) {
		return;
	}
	_registry = sp::move(registry);
	if (isRunning()) {
		rebuild();
	} else {
		_treeDirty = true;
	}
}

void MarkdownView::setLinkCallback(Function<void(StringView, StringView)> &&cb) {
	_linkCallback = sp::move(cb);
}

void MarkdownView::handleLinkActivated(const MarkdownRunMap::Link &link) {
	if (_linkCallback) {
		_linkCallback(link.href, link.title);
	}
}

// --- selection ---------------------------------------------------------------------------------

void MarkdownView::setSelectionRange(uint32_t begin, uint32_t end) {
	auto length = getTextLength();
	begin = sprt::min(begin, length);
	end = sprt::min(end, length);
	if (begin > end) {
		sprt::swap(begin, end);
	}

	if (_selectionBegin == begin && _selectionEnd == end) {
		return;
	}

	_selectionBegin = begin;
	_selectionEnd = end;

	_flow->applySelection(begin, end);
	if (_selection) {
		_selection->updateHandles();
	}

	/* One selection per scene, and the document is now holding it.

	Through `select()` with an item rather than `selectNode()`, and the difference is the whole
	reason to be a SelectionOwner: an owner installed by `selectNode` is never told that the
	selection moved elsewhere, so its highlight would stay on screen next to somebody else's. */
	if (auto system = SelectionSystem::acquireForNode(this)) {
		if (end > begin) {
			SelectionItem item{Rc<Ref>(this), 0};
			system->select(this, makeSpanView(&item, 1));
		} else if (system->getOwner() == this) {
			system->clear();
		}
	}
}

void MarkdownView::selectAll() { setSelectionRange(0, getTextLength()); }

void MarkdownView::clearSelection() { setSelectionRange(0, 0); }

Node *MarkdownView::resolveSelectionNode(const SelectionItem &) const {
	// The document is one item and it is this node: there are no rows to materialize.
	return const_cast<MarkdownView *>(this);
}

void MarkdownView::handleSelectionChanged(SpanView<SelectionItem> items) {
	if (!items.empty()) {
		return;
	}

	// The scene's selection went to somebody else. Drop the highlight WITHOUT touching the system
	// again: it is in the middle of notifying, and this is the losing half of that call.
	_selectionBegin = _selectionEnd = 0;
	_flow->applySelection(0, 0);
	if (_selection) {
		_selection->updateHandles();
	}
}

String MarkdownView::getSelectedText() const {
	return getTextForRange(_selectionBegin, _selectionEnd);
}

String MarkdownView::getSelectedMarkup(document::MarkdownMarkup mode) const {
	return getMarkupForRange(_selectionBegin, _selectionEnd, mode);
}

void MarkdownView::setSelectionColor(const Color4F &color) {
	if (_selectionColor == color) {
		return;
	}
	_selectionColor = color;
	_flow->setSelectionColor(color);
}

// --- copying -----------------------------------------------------------------------------------

ClipboardSession *MarkdownView::acquireClipboard() {
	if (!_clipboard && _director) {
		_clipboard = Rc<ClipboardSession>::create(_director->getApplication());
	}
	return _clipboard;
}

bool MarkdownView::copy(document::MarkdownMarkup mode) {
	if (!hasSelection() || _copyPolicy == CopyPolicy::Nothing) {
		return false;
	}

	auto clipboard = acquireClipboard();
	if (!clipboard) {
		return false;
	}

	auto plain = getSelectedText();
	if (_copyPolicy == CopyPolicy::TextOnly) {
		return clipboard->writeText(plain) == Status::Ok;
	}

	/* Both representations, markup first.

	The order is the advertisement's preference, not the answer: a reader negotiates its own list
	against this one. What it buys is that an application that understands Markdown is OFFERED it,
	while anything asking for plain text still gets something readable. The offer copies the bytes,
	so these locals may die here. */
	ClipboardOffer offer;
	offer.setLabel("Markdown fragment")
			.addText(getSelectedMarkup(mode), "text/markdown")
			.addText(plain, "text/plain");

	return clipboard->write(sp::move(offer), this) == Status::Ok;
}

void MarkdownView::buildContextMenu() {
	if (getContextMenu(this)) {
		return;
	}

	setContextMenu(this, [this](const ContextMenuRequest &) -> Rc<MenuSource> {
		auto source = Rc<MenuSource>::create();

		// The two copies are offered only when there is something to copy: a menu item that
		// cannot act is worse than a menu without it.
		if (hasSelection() && _copyPolicy != CopyPolicy::Nothing) {
			source->addButton("copy", "Copy", [this](NotNull<MenuSourceButton>) { copy(); });
			if (_copyPolicy == CopyPolicy::Both) {
				source->addButton("copy-source", "Copy as Markdown",
						[this](NotNull<MenuSourceButton>) { copy(document::MarkdownMarkup::Raw); });
			}
			source->addSeparator("copy-end");
		}

		source->addButton("select-all", "Select all",
				[this](NotNull<MenuSourceButton>) { selectAll(); });
		return source;
	});
}

bool MarkdownView::updateInlineStyles() {
	if (!_director) {
		return false;
	}

	auto controller = _director->getApplication()->getExtension<font::FontController>();
	if (!controller) {
		return false;
	}

	// An index, not a name: a range style carries the family as the controller's own id. An
	// unknown family stays at the sentinel, and inline code then renders in the block's face
	// rather than in a wrong one.
	auto family = controller->getFamilyIndex(StringView("monospace"));
	if (family == _inlineStyles.monospaceFamily) {
		return false;
	}

	_inlineStyles.monospaceFamily = family;
	_treeDirty = true;
	return true;
}

} // namespace stappler::xenolith::ui
