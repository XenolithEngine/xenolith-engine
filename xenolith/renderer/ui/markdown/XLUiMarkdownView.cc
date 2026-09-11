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

/* A `div` has no layout of its own, and in this widget that means it has NONE: the resolver
builds a flex container for `display: flex` and for nothing else, so an unstyled container
collapses its children onto the origin. The parser wraps footnotes and citations in one, which is
how a document with a footnote used to come out in a heap. */
div, figure {
	display: flex;
	flex-direction: column;
	align-items: stretch;
}

/* The parser writes these blocks after the document body, separated by a rule of their own. */
.footnotes, .citations, .glossary { margin-top: 12px; }

h6.footnotes_header, h6.citations_header, h6.glossary_header {
	font-size: 15px;
	color: #555555;
	margin-bottom: 6px;
}

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

/* THE INLINE CONSTRUCTS.

These are the rules that make a Markdown document restyleable at all. An inline is a style range
inside its block's Label, not a node, so nothing here matches an element that exists - the widget
manufactures a probe for the length of the cascade query and throws it away (ui::MarkdownInlineResolver).
What reaches the range is only what these rules change ABOUT THE BLOCK, which is why `code` may
name a family without also fixing a size: inline code in a heading stays heading-sized.

Only the twelve properties a range style can carry are read here; `white-space` and friends are
properties of a paragraph and are ignored on an inline. */
strong, b { font-weight: bold; }
em, i { font-style: italic; }
del, s { text-decoration: line-through; }
ins { color: #2e7d32; }
mark { color: #333300; }
sub { vertical-align: sub; }
sup { vertical-align: super; }

a { color: #1565c0; text-decoration: underline; }

/* `code` is BOTH the inline construct and the block inside a `pre`. The bare rule is the inline
one; the block overrides the colour back, because a red fence would be unreadable. */
code { font-family: monospace; color: #b71c1c; }

pre code {
	font-size: 13px;
	white-space: pre;
	color: #1a1a1a;
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

	// A sheet an application puts ABOVE the view changes what the inlines resolve to, and only
	// nodes that ask are told about an ancestor's components.
	setWantsAncestorComponents(true);

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

void MarkdownView::handleComponentsDirty(const ComponentMask &mask) {
	Node::handleComponentsDirty(mask);
	invalidateInlineStyles();
}

void MarkdownView::handleAncestorComponentsDirty() {
	Node::handleAncestorComponentsDirty();
	invalidateInlineStyles();
}

void MarkdownView::setVirtualizationThreshold(uint32_t threshold) {
	if (_virtualThreshold == threshold) {
		return;
	}
	_virtualThreshold = threshold;
	if (isRunning()) {
		rebuild();
	}
}

void MarkdownView::update(const UpdateTime &time) {
	Node::update(time);

	if (_virtual.isEnabled()) {
		auto scroll = getScrollSystem();
		auto pending =
				_virtual.update(_contentSize.height, scroll ? scroll->getScrollPosition().y : 0.0f);

		/* Kept scheduled for as long as the document is virtualized, measured or not: the window
		follows the scroll, and a scroll produces frames rather than an event this view can hook
		without taking the scroll's single callback away from whoever else may want it. The pass
		is a walk of the block list and nothing else. */
		(void)pending;
	}

	if (_inlineStylesDirty) {
		_inlineStylesDirty = false;
		unscheduleUpdate();
		restyleInlines();
	}
}

uint32_t MarkdownView::getStyleGeneration() const {
	uint32_t generation = 0;
	for (auto p = static_cast<const Node *>(this); p != nullptr; p = p->getParent()) {
		if (auto state = p->getComponent<StyleSystemState>()) {
			generation = generation * 31 + state->version + 1;
		}
	}
	return generation;
}

void MarkdownView::invalidateInlineStyles() {
	auto generation = getStyleGeneration();
	if (generation == _styleVersion) {
		return;
	}

	_styleVersion = generation;

	// Nothing to restyle before the first build, and a rebuild resolves against the new sheet on
	// its own.
	if (_treeDirty || _blocks == 0) {
		return;
	}

	updateSelectionColor();

	_inlineStylesDirty = true;
	scheduleUpdate();
}

void MarkdownView::updateSelectionColor() {
	// The custom property is read off the view's own resolved style, which is also the moment the
	// inline deltas are decided - the two answer to the same reload.
	auto style = StyleResolver::resolveStyleForNode(this);
	if (!style.valid()) {
		return;
	}

	Color4B color;
	if (!sprt::geom::readColor(style.getCustomProperty("--md-selection-color"), color)) {
		return;
	}

	setSelectionColor(Color4F(color));
}

void MarkdownView::restyleInlines() {
	auto started = sprt::platform::nanoclock(ClockType::Monotonic);

	MarkdownBuilder builder(_content, _registry, _inlineStyles,
			_director ? _director->getApplication()->getExtension<font::FontController>()
					  : nullptr);
	builder.setSource(getSource());

	for (auto &entry : _flow->getEntries()) {
		if (entry.kind != MarkdownFlowKind::Text || !entry.source || !entry.node) {
			continue;
		}
		if (auto label = dynamic_cast<basic2d::Label *>(entry.node.get())) {
			builder.restyleText(label, *entry.source);
		}
	}

	_timings.restyle = sprt::platform::nanoclock(ClockType::Monotonic) - started;
	_timings.probes = builder.getInlineResolver().getProbeCount();
}

void MarkdownView::setSource(StringView markdown) {
	auto start = sprt::platform::nanoclock(ClockType::Monotonic);
	auto doc = Rc<document::DocumentMarkdown>::create(
			BytesView(reinterpret_cast<const uint8_t *>(markdown.data()), markdown.size()),
			StringView("text/markdown"));
	_timings.parse = sprt::platform::nanoclock(ClockType::Monotonic) - start;

	setDocument(sp::move(doc));
}

void MarkdownView::setSourceFile(const FileInfo &file) {
	// A relative `src` in the document means "beside the document", so the document's own
	// directory is the base unless the caller says otherwise afterwards.
	setImageBase(filepath::root(file.path), file.category);

	auto start = sprt::platform::nanoclock(ClockType::Monotonic);
	auto doc = Rc<document::DocumentMarkdown>::create(file, StringView("text/markdown"));
	_timings.parse = sprt::platform::nanoclock(ClockType::Monotonic) - start;

	setDocument(sp::move(doc));
}

void MarkdownView::setImageResolver(MarkdownImageResolver &&resolver) {
	_imageResolver = sp::move(resolver);
	_treeDirty = true;
	if (isRunning()) {
		rebuild();
	}
}

void MarkdownView::setImageBase(StringView path, FileCategory category) {
	_imageBasePath = path.str<Interface>();
	_imageBaseCategory = category;
}

MarkdownImageSource MarkdownView::resolveImage(const MarkdownImageRequest &request) {
	MarkdownImageSource ret;

	if (request.src.empty()) {
		return ret;
	}

	// Not a local file. Fetching it would put a network stack and a cache inside a widget whose
	// job is to draw a document; an application that wants remote images supplies a resolver.
	if (request.src.find("://") != maxOf<size_t>()) {
		return ret;
	}

	auto path = filepath::isAbsolute(request.src)
			? request.src.str<Interface>()
			: filepath::merge<Interface>(_imageBasePath, request.src);

	FileInfo file(path, _imageBaseCategory);

	/* The extent comes from the file's HEADER, not from decoding it - a few bytes read here, and
	the box the paragraph reserves is already the right one. The pixels arrive whenever the loop
	gets to them and find their place kept, which is the whole reason a document does not re-flow
	as its images appear. */
	uint32_t width = 0;
	uint32_t height = 0;
	if (!bitmap::getImageSize(file, width, height) || width == 0 || height == 0) {
		return ret;
	}

	ret.size = Size2(float(width), float(height));

	// What the markup asked for wins, one axis at a time; the other follows the file's own
	// aspect so a lone `width=` does not squash the picture.
	if (request.declared.width > 0.0f && request.declared.height > 0.0f) {
		ret.size = request.declared;
	} else if (request.declared.width > 0.0f) {
		ret.size = Size2(request.declared.width,
				request.declared.width * float(height) / float(width));
	} else if (request.declared.height > 0.0f) {
		ret.size = Size2(request.declared.height * float(width) / float(height),
				request.declared.height);
	}

	if (!_director) {
		return ret;
	}

	// Keyed by the RESOLVED path: the cache is global, and two documents in two directories both
	// referring to `image.png` are not the same image.
	if (auto cache = _director->getResourceCache()) {
		ret.texture = cache->addExternalImage(path,
				core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled), file,
				TimeInterval::seconds(600),
				TemporaryResourceFlags::RemoveOnClear | TemporaryResourceFlags::CompileWhenAdded);
	}

	return ret;
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
	auto started = sprt::platform::nanoclock(ClockType::Monotonic);

	// Every label the selection was painted on is about to be discarded, and the positions it
	// held indexed a document that no longer exists.
	_selectionBegin = _selectionEnd = 0;

	// The flow holds every node it indexes, so it goes first - or a rebuilt document would keep
	// the previous one alive in its own index.
	_flow = Rc<MarkdownFlow>::alloc();
	_anchors.clear();
	_virtual.clear();
	_content->removeAllChildren();
	_blocks = 0;
	_treeDirty = false;

	if (!_document) {
		_timings.build = sprt::platform::nanoclock(ClockType::Monotonic) - started;
		return;
	}

	auto page = _document->getRoot();
	if (!page || !page->getRoot()) {
		log::source().warn("ui::MarkdownView", "document has no root page");
		return;
	}

	MarkdownBuilder builder(_content, _registry, _inlineStyles,
			_director ? _director->getApplication()->getExtension<font::FontController>()
					  : nullptr);
	builder.setSource(_document->getSource());

	// A local `MarkdownImageResolver` so that the builder holds a pointer to something that
	// outlives the build - a Callback owns nothing, and the view's own function is what it wraps.
	MarkdownImageResolver resolver = [this](const MarkdownImageRequest &request) {
		return _imageResolver ? _imageResolver(request) : resolveImage(request);
	};
	builder.setImageResolver(&resolver);
	_blocks = builder.build(*page->getRoot());
	_flow = builder.getFlow();
	_anchors = builder.getAnchors();

	_styleVersion = getStyleGeneration();
	updateSelectionColor();

	// The colour lives on each Label separately, so a fresh tree has to be told again.
	_flow->setSelectionColor(_selectionColor);

	/* Hide everything the reader cannot see, before the first layout rather than after it: a
	document of ten thousand blocks would otherwise shape every one of them to produce a
	screenful. Small documents are left exactly as they were. */
	if (_virtual.init(_content, _virtualThreshold)) {
		scheduleUpdate();
	}

	_timings.build = sprt::platform::nanoclock(ClockType::Monotonic) - started;
	_timings.blocks = _blocks;
	_timings.sourceLength = uint32_t(_document->getSource().size());
	_timings.probes = builder.getInlineResolver().getProbeCount();
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

uint32_t MarkdownView::getAnchorPosition(StringView id) const {
	if (id.starts_with("#")) {
		id = id.sub(1);
	}
	auto it = _anchors.find(id);
	return it != _anchors.end() ? it->second : maxOf<uint32_t>();
}

bool MarkdownView::scrollToAnchor(StringView id) {
	auto position = getAnchorPosition(id);
	if (position == maxOf<uint32_t>()) {
		return false;
	}

	auto entry = _flow->findByPosition(position);
	if (!entry || !entry->node) {
		return false;
	}

	/* A block that is not materialized has no position to scroll to - the layout skipped it, so
	whatever it last reported is meaningless. Its place in the document is known all the same, as
	the sum of the advances above it, and that is what the scroll is set from. */
	if (_virtual.isEnabled()) {
		auto index = _virtual.findBlock(entry->node);
		if (index != maxOf<uint32_t>()) {
			_virtual.ensureMeasuredTo(index);
			auto top = _virtual.getBlockTop(index);
			if (auto scroll = getScrollSystem(); scroll && top != maxOf<float>()) {
				scroll->setScrollPosition(Vec2(0.0f, sprt::max(0.0f, top - 8.0f)));
				return true;
			}
		}
	}

	// Every scroll between the node and here, not just this view's own: a footnote inside a
	// scrolled block has two to satisfy.
	scrollIntoView(entry->node, Padding(8.0f));
	return true;
}

void MarkdownView::handleLinkActivated(const MarkdownRunMap::Link &link) {
	// A link into the document is the document's own business, and answering it here is what
	// makes footnotes work with no application code at all. Only what points outward is offered
	// to the callback.
	if (link.href.starts_with("#") && scrollToAnchor(link.href)) {
		return;
	}

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
