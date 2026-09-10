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

	ret.setValue(encodeNode(_view->getContentNode()), "tree");
	return ret;
}

void MarkdownLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("dump", "The built tree: type, classes, text, source runs and measured size",
			[this](Value &&) { return encodeTree(); });

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
