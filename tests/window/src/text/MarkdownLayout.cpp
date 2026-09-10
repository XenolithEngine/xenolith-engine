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

#include "text/MarkdownLayout.h"
#include "XLDirector.h"
#include "XLUiStyleResolver.h"
#include "XLUiLayoutSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

static constexpr auto s_sample = StringView(R"Md(# Markdown view

A paragraph with **bold**, *emphasis*, `inline code`, {--struck out--} and a
[link](https://xenolith.studio) inside it. This sentence is written across two source lines on
purpose: the break must become a space, not a break on screen.

## Lists

- first item
- second item with **bold** in it
    - nested item
    - another nested item
- third item

1. ordered one
2. ordered two

- [ ] an open task
- [x] a finished one

> A quoted line.
>
> And a second paragraph of the same quote.

## Code

```cpp
int main() {
    return 0;
}
```

## Table

| Name | Meaning |
|------|---------|
| `p`  | paragraph |
| `h1` | heading |

---

Term list:

Definition term
: what the term means
)Md");

StringView MarkdownLayout::getSampleSource() { return s_sample; }

bool MarkdownLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// A light ground: the built-in sheet paints dark text and expects to sit on paper.
	_background = addChild(Rc<basic2d::Layer>::create(Color4F(1.0f, 1.0f, 1.0f, 1.0f)), ZOrder(-1));
	_background->setAnchorPoint(Anchor::BottomLeft);

	// No stylesheet on the layout: proving that the view alone is enough to read a document is
	// half of what this stand is for. `markdown.app-style` adds one when the check wants the
	// override routes tested.
	addSystem(Rc<ui::StyleResolver>::create(true));

	_view = addChild(Rc<ui::MarkdownView>::create(s_sample), ZOrder(1));

	// Recorded rather than acted on: what a check needs to know is that the click reached the
	// right link, not what an application would do with it.
	_view->setLinkCallback(
			[this](StringView href, StringView) { _lastLink = href.str<Interface>(); });

	return true;
}

void MarkdownLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto top = getWorkTop();
	const auto size = getWorkSize();

	_background->setPosition(Vec2(0.0f, 0.0f));
	_background->setContentSize(_contentSize);

	auto width = _width > 0.0f ? _width : size.width;

	_view->setAnchorPoint(Anchor::TopLeft);
	_view->setPosition(Vec2((size.width - width) / 2.0f, top));
	_view->setContentSize(Size2(width, size.height));
}

Value MarkdownLayout::encodeNode(const Node *node) const {
	Value ret;
	ret.setString(node->getType(), "type");

	// The document's own `id`, which is also this node's CSS `#id` and what an anchor names.
	if (!node->getName().empty()) {
		ret.setString(node->getName(), "name");
	}

	if (auto classes = node->getStyleClasses()) {
		auto &list = ret.emplace("classes");
		for (auto &it : *classes) { list.addString(it); }
	}

	auto size = node->getContentSize();
	auto &sizeValue = ret.emplace("size");
	sizeValue.addInteger(int64_t(roundf(size.width)));
	sizeValue.addInteger(int64_t(roundf(size.height)));

	if (auto label = dynamic_cast<const basic2d::Label *>(node)) {
		ret.setString(string::toUtf8<Interface>(label->getString()), "text");
		ret.setInteger(int64_t(label->getStyles().size()), "ranges");
		// The width the formatter was told to wrap at; 0 means "never wrap", which is the whole
		// difference between a paragraph and a line that runs off the edge.
		ret.setInteger(int64_t(roundf(label->getWidth())), "wrap");
		ret.setInteger(int64_t(label->getLinesCount()), "lines");
	}

	if (auto map = node->getComponent<ui::MarkdownRunMap>()) {
		auto &runs = ret.emplace("runs");
		for (auto &it : map->runs) {
			Value run;
			run.addInteger(it.charStart);
			run.addInteger(it.charCount);
			run.addInteger(it.srcOffset);
			run.addInteger(it.srcLength);
			run.addBool(it.verbatim);
			runs.addValue(sp::move(run));
		}
		if (!map->links.empty()) {
			auto &links = ret.emplace("links");
			for (auto &it : map->links) { links.addString(it.href); }

			// The same links with their character ranges, which is what a check needs to aim a
			// click at one of them.
			auto &ranges = ret.emplace("linkRanges");
			for (auto &it : map->links) {
				auto &r = ranges.emplace();
				r.addInteger(int64_t(it.charStart));
				r.addInteger(int64_t(it.charCount));
				r.addString(it.href);
			}
		}
	}

	auto &children = ret.emplace("children");
	for (auto &it : node->getChildren()) { children.addValue(encodeNode(it)); }
	if (children.empty()) {
		ret.erase("children");
	}

	return ret;
}

Value MarkdownLayout::encodeTree() const {
	Value ret;
	ret.setInteger(int64_t(_view->getBlockCount()), "blocks");
	ret.setInteger(int64_t(_view->getSource().size()), "sourceLength");

	// The source itself, so a check can take a run's byte range and see for itself that the text
	// in the label is the text in the document.
	ret.setString(_view->getSource(), "source");

	auto size = _view->getContentNode()->getContentSize();
	ret.setInteger(int64_t(roundf(size.width)), "contentWidth");
	ret.setInteger(int64_t(roundf(size.height)), "contentHeight");

	auto viewSize = _view->getContentSize();
	ret.setInteger(int64_t(roundf(viewSize.width)), "viewWidth");
	ret.setInteger(int64_t(roundf(viewSize.height)), "viewHeight");

	// Which systems actually ended up on the view, and what the layout thinks it may overflow.
	// The document's width is decided by exactly these, so a stand that could not report them
	// left every layout question to guesswork.
	if (auto layout = _view->getSystemByType<ui::LayoutSystem>()) {
		ret.setBool(true, "viewLayout");
		ret.setBool(layout->isOverflowX(), "overflowX");
		ret.setBool(layout->isOverflowY(), "overflowY");
		auto extent = layout->getContentExtent();
		auto &e = ret.emplace("viewExtent");
		e.addInteger(int64_t(roundf(extent.width)));
		e.addInteger(int64_t(roundf(extent.height)));
	} else {
		ret.setBool(false, "viewLayout");
	}

	if (auto scroll = _view->getScrollSystem()) {
		auto range = scroll->getScrollRange();
		auto &r = ret.emplace("scrollRange");
		r.addInteger(int64_t(roundf(range.width)));
		r.addInteger(int64_t(roundf(range.height)));
	}

	if (auto bodyLayout = _view->getContentNode()->getSystemByType<ui::LayoutSystem>()) {
		auto extent = bodyLayout->getContentExtent();
		auto &e = ret.emplace("bodyExtent");
		e.addInteger(int64_t(roundf(extent.width)));
		e.addInteger(int64_t(roundf(extent.height)));
	}

	ret.setInteger(int64_t(_view->getTextLength()), "textLength");
	ret.setValue(encodeNode(_view->getContentNode()), "tree");
	return ret;
}

// The document in reading order: one row per entry, in the order a reader reads them.
Value MarkdownLayout::encodeFlow() const {
	Value ret;
	auto flow = _view->getFlow();
	ret.setInteger(int64_t(_view->getTextLength()), "textLength");

	auto &entries = ret.emplace("entries");
	for (auto &it : flow->getEntries()) {
		Value entry;
		entry.setString(it.node ? it.node->getType() : StringView(), "type");
		switch (it.kind) {
		case ui::MarkdownFlowKind::Text: entry.setString("text", "kind"); break;
		case ui::MarkdownFlowKind::Marker: entry.setString("marker", "kind"); break;
		case ui::MarkdownFlowKind::Atomic: entry.setString("atomic", "kind"); break;
		}
		entry.setInteger(int64_t(it.textBegin), "begin");
		entry.setInteger(int64_t(it.textLength), "length");
		entry.setInteger(int64_t(it.span.offset), "srcOffset");
		entry.setInteger(int64_t(it.span.length), "srcLength");
		entries.addValue(sp::move(entry));
	}
	return ret;
}

/* What is selected, and how the document shows it.

The interesting half is `entries`: which labels actually carry a drawn highlight. A range is a
pair of numbers and could be right while nothing is painted, or painted on a block the range never
touched - and neither shows up in a screenshot of a document this dense. */
Value MarkdownLayout::encodeSelection() const {
	Value ret;

	auto range = _view->getSelectionRange();
	ret.setInteger(int64_t(range.first), "begin");
	ret.setInteger(int64_t(range.second), "end");
	ret.setBool(_view->hasSelection(), "hasSelection");
	ret.setString(_view->getSelectedText(), "text");
	ret.setString(_view->getSelectedMarkup(), "markup");

	auto color = _view->getSelectionColor();
	auto &tint = ret.emplace("selectionColor");
	tint.addDouble(color.r);
	tint.addDouble(color.g);
	tint.addDouble(color.b);
	tint.addDouble(color.a);

	auto &entries = ret.emplace("entries");
	for (auto &it : _view->getFlow()->getEntries()) {
		auto label = dynamic_cast<const basic2d::Label *>(it.node.get());
		if (!label) {
			continue;
		}
		auto cursor = label->getSelectionCursor();
		if (cursor == core::TextCursor::InvalidCursor || cursor.length == 0) {
			continue;
		}

		Value entry;
		entry.setString(it.node->getType(), "type");
		entry.setInteger(int64_t(cursor.start), "cursorStart");
		entry.setInteger(int64_t(cursor.length), "cursorLength");

		auto rect = label->getSelectionRect();
		auto &r = entry.emplace("rect");
		r.addInteger(int64_t(roundf(rect.size.width)));
		r.addInteger(int64_t(roundf(rect.size.height)));

		entries.addValue(sp::move(entry));
	}

	// The two carets the handles are supposed to stand on, so a check can say whether a handle is
	// merely visible or actually in the right place.
	auto &carets = ret.emplace("carets");
	for (auto position : {range.first, range.second}) {
		auto point = _view->getFlow()->getPointForPosition(position);
		auto &c = carets.emplace();
		if (point.first.isValid()) {
			c.addInteger(int64_t(roundf(point.first.x)));
			c.addInteger(int64_t(roundf(point.first.y)));
		}
	}

	if (auto selection = _view->getSelectionSystem()) {
		ret.setBool(selection->isTouchMode(), "touchMode");
		ret.setBool(selection->isDragging(), "dragging");

		auto &handles = ret.emplace("handles");
		for (auto start : {true, false}) {
			auto point = selection->getHandlePosition(start);
			auto &h = handles.emplace();
			if (point.isValid()) {
				h.addInteger(int64_t(roundf(point.x)));
				h.addInteger(int64_t(roundf(point.y)));
			}
		}
	}

	// A drag that scrolls the document instead of selecting it is the failure this stand exists
	// to catch, so the offset is reported next to the range.
	if (auto scroll = _view->getScrollSystem()) {
		ret.setInteger(int64_t(roundf(scroll->getScrollPosition().y)), "scrollY");
	}

	ret.setString(_lastLink, "lastLink");
	return ret;
}

void MarkdownLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("dump", "The built tree: type, classes, text, source runs and measured size",
			[this](Value &&) { return encodeTree(); });

	addCommand("flow", "The document in reading order: every entry, its positions and its span",
			[this](Value &&) { return encodeFlow(); });

	/* Every inline image: where its box ended up and whether anything is in it yet. The two are
	deliberately separate - the box is the promise the layout made, the texture is what arrived
	later, and the milestone's whole claim is that the second does not move the first. */
	addCommand("images", "Every image: its box, its source and whether its texture is loaded",
			[this](Value &&) {
		Value ret;
		auto &list = ret.emplace("images");

		for (auto &entry : _view->getFlow()->getEntries()) {
			auto label = dynamic_cast<basic2d::Label *>(entry.node.get());
			if (!label) {
				continue;
			}

			auto system = label->getSystemByType<ui::MarkdownImageSystem>();
			uint32_t index = 0;
			for (auto &object : label->getInlineObjects()) {
				Value one;
				one.setInteger(int64_t(entry.textBegin + object.charIndex), "position");
				one.setInteger(int64_t(object.charIndex), "charIndex");

				auto &size = one.emplace("size");
				size.addInteger(int64_t(roundf(object.size.width)));
				size.addInteger(int64_t(roundf(object.size.height)));

				auto rect = label->getInlineObjectRect(index);
				auto &box = one.emplace("box");
				box.addInteger(int64_t(roundf(rect.origin.x)));
				box.addInteger(int64_t(roundf(rect.origin.y)));
				box.addInteger(int64_t(roundf(rect.size.width)));
				box.addInteger(int64_t(roundf(rect.size.height)));

				if (system && index < system->getImageCount()) {
					auto placed = system->getImageRect(index);
					auto &p = one.emplace("placed");
					p.addInteger(int64_t(roundf(placed.origin.x)));
					p.addInteger(int64_t(roundf(placed.origin.y)));
				}

				list.addValue(sp::move(one));
				++index;
			}
		}
		return ret;
	});

	addCommand("anchors", "Every id in the document and the reading position it names",
			[this](Value &&) {
		Value ret;
		auto &list = ret.emplace("anchors");
		for (auto &it : _view->getAnchors()) {
			Value entry;
			entry.setString(it.first, "id");
			entry.setInteger(int64_t(it.second), "position");
			list.addValue(sp::move(entry));
		}
		ret.setInteger(int64_t(_view->getScrollSystem()
									   ? _view->getScrollSystem()->getScrollPosition().y
									   : 0.0f),
				"scrollY");
		return ret;
	});

	/* What a style RANGE actually carries, which is the only way to see that the cascade reached
	an inline: the tree dump can count ranges but not read them, and a range that resolved to
	nothing is indistinguishable there from one that resolved to the wrong thing. */
	addCommand("inline-styles", "The style the range under a position resolved to: {position}",
			[this](Value &&args) {
		Value ret;
		auto position = uint32_t(args.getInteger("position"));
		auto entry = _view->getFlow()->findByPosition(position);
		if (!entry || !entry->node) {
			ret.setBool(false, "found");
			return ret;
		}

		auto label = dynamic_cast<basic2d::Label *>(entry->node.get());
		if (!label) {
			ret.setBool(false, "found");
			return ret;
		}

		auto local = position - entry->textBegin;
		ret.setBool(true, "found");
		ret.setInteger(int64_t(local), "charIndex");

		auto &list = ret.emplace("styles");
		for (auto &spec : label->getStyles()) {
			if (local < spec.start || local >= spec.start + spec.length) {
				continue;
			}
			for (auto &param : spec.style.params) {
				Value one;
				one.setInteger(int64_t(spec.start), "start");
				one.setInteger(int64_t(spec.length), "length");
				switch (param.name) {
				case basic2d::Label::Style::Name::Color:
					one.setString("color", "name");
					one.setString(toString(int(param.value.color.r), ",", int(param.value.color.g),
										  ",", int(param.value.color.b)),
							"value");
					break;
				case basic2d::Label::Style::Name::FontWeight:
					one.setString("font-weight", "name");
					one.setInteger(int64_t(param.value.fontWeight.get()), "value");
					break;
				case basic2d::Label::Style::Name::FontStyle:
					one.setString("font-style", "name");
					one.setInteger(int64_t(param.value.fontStyle.get()), "value");
					break;
				case basic2d::Label::Style::Name::FontSize:
					one.setString("font-size", "name");
					one.setInteger(int64_t(param.value.fontSize.get()), "value");
					break;
				case basic2d::Label::Style::Name::FontFamily:
					one.setString("font-family", "name");
					one.setInteger(int64_t(param.value.fontFamily), "value");
					break;
				case basic2d::Label::Style::Name::TextDecoration:
					one.setString("text-decoration", "name");
					one.setInteger(int64_t(toInt(param.value.textDecoration)), "value");
					break;
				case basic2d::Label::Style::Name::VerticalAlign:
					one.setString("vertical-align", "name");
					one.setInteger(int64_t(toInt(param.value.verticalAlign)), "value");
					break;
				default: one.setString("other", "name"); break;
				}
				list.addValue(sp::move(one));
			}
		}
		return ret;
	});

	/* Follow the link under a position without synthesizing a click: an anchor jump is a scroll,
	and a scroll started by a synthetic tap would have to be told apart from the tap's own. */
	addCommand("activate-link", "Follow the link under a position: {position}",
			[this](Value &&args) {
		Value ret;
		auto position = uint32_t(args.getInteger("position"));
		auto link = _view->getFlow()->findLink(position);
		ret.setBool(link != nullptr, "found");
		if (link) {
			ret.setString(link->href, "href");
			_lastLink.clear();
			_view->handleLinkActivated(*link);
		}
		ret.setString(_lastLink, "lastLink");
		return ret;
	});

	/* The milestone's whole point, made observable: a range of reading positions, handed back as
	the markup that produced it. `mode` is `normalized` (the edges are closed up) or `raw` (the
	slice as it stands). */
	addCommand("range", "The markup and the text of a range: {begin, end, mode}",
			[this](Value &&args) {
		const Value &in = args;
		auto begin = uint32_t(in.getInteger("begin"));
		auto end = in.hasValue("end") ? uint32_t(in.getInteger("end")) : _view->getTextLength();
		auto mode = (in.getString("mode") == "raw") ? document::MarkdownMarkup::Raw
													: document::MarkdownMarkup::Normalized;

		Value ret;
		ret.setInteger(int64_t(begin), "begin");
		ret.setInteger(int64_t(end), "end");

		auto range = _view->getSourceRangeForTextRange(begin, end);
		ret.setInteger(int64_t(range.first), "srcBegin");
		ret.setInteger(int64_t(range.second), "srcEnd");
		ret.setString(_view->getMarkupForRange(begin, end, mode), "markup");
		ret.setString(_view->getTextForRange(begin, end), "text");
		return ret;
	});

	// --- selection ---

	addCommand("selection", "What is selected, which labels paint it, and where the handles are",
			[this](Value &&) { return encodeSelection(); });

	addCommand("select", "Select a range of reading positions: {begin, end}", [this](Value &&args) {
		_view->setSelectionRange(uint32_t(args.getInteger("begin")),
				uint32_t(args.getInteger("end")));
		return encodeSelection();
	});

	addCommand("select-all", "Select the whole document", [this](Value &&) {
		_view->selectAll();
		return encodeSelection();
	});

	addCommand("clear-selection", "Drop the selection", [this](Value &&) {
		_view->clearSelection();
		return encodeSelection();
	});

	addCommand("position-point", "Where a reading position is on screen: {position}",
			[this](Value &&args) {
		auto point = _view->getFlow()->getPointForPosition(uint32_t(args.getInteger("position")));
		Value ret;
		ret.setBool(point.first.isValid(), "ok");
		if (point.first.isValid()) {
			ret.setDouble(double(point.first.x), "x");
			ret.setDouble(double(point.first.y), "y");
			ret.setDouble(double(point.second), "height");
		}
		return ret;
	});

	addCommand("point", "The reading position under a window point: {x, y}", [this](Value &&args) {
		auto position = _view->getFlow()->getPositionForPoint(
				Vec2(float(args.getDouble("x")), float(args.getDouble("y"))));
		Value ret;
		ret.setInteger(int64_t(position), "position");
		return ret;
	});

	// --- copying ---

	addCommand("copy", "Copy the selection: {mode}", [this](Value &&args) {
		// Through a const reference: a non-const get*() on a missing key asserts, and `mode` is
		// optional here.
		const Value &in = args;
		auto mode = (in.getString("mode") == "raw") ? document::MarkdownMarkup::Raw
													: document::MarkdownMarkup::Normalized;
		Value ret;
		ret.setBool(_view->copy(mode), "ok");
		return ret;
	});

	addCommand("clipboard-read", "Read the clipboard back: {prefer}", [this](Value &&args) {
		if (!_clipboard) {
			_clipboard = Rc<ClipboardSession>::create(_director->getApplication());
		}

		const Value &in = args;
		Vector<String> preference;
		for (auto &it : in.getArray("prefer")) { preference.emplace_back(it.getString()); }
		if (preference.empty()) {
			preference.emplace_back("text/plain");
		}

		Vector<StringView> views;
		for (auto &it : preference) { views.emplace_back(it); }

		auto serial = _clipboard->read(views, [this](const ClipboardSession::Result &result) {
			++_deliveries;
			_lastRead = Value();
			_lastRead.setBool(result.ok(), "ok");
			_lastRead.setString(result.type, "type");
			_lastRead.setString(result.text(), "text");
		}, this);

		Value ret;
		ret.setInteger(int64_t(serial), "serial");
		ret.setInteger(int64_t(_deliveries), "deliveries");
		return ret;
	});

	addCommand("clipboard-state", "What the last clipboard read answered", [this](Value &&) {
		Value ret;
		ret.setInteger(int64_t(_deliveries), "deliveries");
		ret.setValue(_lastRead, "lastRead");
		return ret;
	});

	addCommand("source", "Replace the document: {text}", [this](Value &&args) {
		_view->setSource(args.getString("text"));
		return encodeTree();
	});

	addCommand("file", "Load a document from disk: {path}", [this](Value &&args) {
		_view->setSourceFile(FileInfo{args.getString("path"), FileCategory::Custom});
		return encodeTree();
	});

	addCommand("width", "Constrain the view's width, to exercise re-wrapping: {width}",
			[this](Value &&args) {
		_width = float(args.getDouble("width"));
		handleContentSizeDirty();
		return encodeTree();
	});

	addCommand("style", "Append CSS into the view's OWN sheet: {css}", [this](Value &&args) {
		_view->addStyle(args.getString("css"));
		return encodeTree();
	});

	addCommand("app-style", "Put a sheet on the LAYOUT, above the view's own: {css}",
			[this](Value &&args) {
		setStyleSheet(args.getString("css"));
		return encodeTree();
	});
}

} // namespace stappler::xenolith::app
