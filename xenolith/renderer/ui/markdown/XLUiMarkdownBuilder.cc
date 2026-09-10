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

#include "XLUiMarkdownBuilder.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The node a text run lives in; see document::Node::pushValue.
static constexpr auto s_valueTag = StringView("__value__");

// An inline construct never becomes a node, so this is also the test for "does this child belong
// in the block's own Label". `br` and the transparent wrappers are inline without being styled.
static bool MarkdownBuilder_isInline(StringView tag) {
	if (tag == s_valueTag || tag == "br" || tag == "span" || tag == "abbr" || tag == "img"
			|| tag == "input") {
		return true;
	}
	return getMarkdownInlineForTag(tag) != MarkdownInline::Max;
}

MarkdownBuilder::MarkdownBuilder(NotNull<Node> root, NotNull<MarkdownRegistry> registry,
		const MarkdownInlineStyles &styles)
: _root(root), _registry(registry), _styles(styles) { }

uint32_t MarkdownBuilder::build(const document::Node &page) {
	_blocks = 0;
	buildChildren(_root, page);
	return _blocks;
}

void MarkdownBuilder::applyIdentity(Node *node, StringView type) {
	node->setType(type);
	node->addStyleClass(toString("md-", type));

	/* A z-order of its own, taken from the position the node was just added at.

	Document order is z-order here, and `sortAllChildren` is not stable - so siblings that all sit
	at zero can come back permuted, which they did: a code block and a table drifted to the end of
	the document and everything after them was laid out in the hole they left. Giving each block a
	distinct order makes the ordering total, and the numbers are the order the parser produced. */
	if (auto parent = node->getParent()) {
		node->setLocalZOrder(ZOrder(int16_t(parent->getChildren().size())));
	}

	// Every node a layout places is anchored at its box origin; the engine is Y-up, so that is
	// the bottom-left corner (see ui::ScrollSystem and the layout widgets).
	node->setAnchorPoint(Anchor::BottomLeft);

	// Here rather than where the label is created, because a label reaches the tree by two roads -
	// a tag factory and the implicit run of a loose inline sequence - and both end up here.
	//
	// Code is the exception, and the reason is the same one that puts the system on everything
	// else: it exists to wrap a block at the width it is offered, and a code block must not wrap.
	// Its lines are its own, it scrolls sideways instead, and its height is the count of them -
	// which is what the label answers for itself when nobody imposes a width.
	if (type != "code" && dynamic_cast<basic2d::Label *>(node)
			&& !node->getSystemByType<MarkdownTextSystem>()) {
		node->addSystem(Rc<MarkdownTextSystem>::create());
	}
}

basic2d::Label *MarkdownBuilder::makeLabel(Node *parent, StringView tag) {
	auto label = parent->addChild(Rc<basic2d::Label>::create());
	applyIdentity(label, tag);
	return label;
}

void MarkdownBuilder::buildChildren(Node *parent, const document::Node &source) {
	// A run of consecutive inline children with no block between them is a block in everything
	// but syntax: a tight list item holds its text directly, and so does a table cell. It gets an
	// implicit Label, typed after the block that contains it so a stylesheet can still reach it.
	Vector<const document::Node *> pending;

	auto flush = [&] {
		if (pending.empty()) {
			return;
		}

		TextState state;
		for (auto &it : pending) { collectText(state, *it, MarkdownInline::Text); }
		pending.clear();

		// A run of nothing but whitespace is the separator between two blocks, not a block: a
		// list item whose text the parser put in a `p` still carries the spaces around it, and
		// an empty label for each of them would litter the tree and the source map alike.
		auto trimmed = WideStringView(state.text);
		trimmed.trimChars<WideStringView::WhiteSpace>();
		if (trimmed.empty()) {
			return;
		}

		// `<tag>-text`, never the container's own tag: a Label typed `li` would match the `li`
		// rule, be given `display: flex` by it, and measure as an empty flex container - a label
		// with text and a height of zero. The type still says where the text came from, so a
		// sheet can address it, and the container's inherited text properties still reach it.
		auto label = makeLabel(parent, toString(source.getHtmlName(), "-text"));
		label->addStyleClass("md-text");
		commitText(label, state);
		++_blocks;
	};

	for (auto &it : source.getNodes()) {
		if (MarkdownBuilder_isInline(it->getHtmlName())) {
			pending.emplace_back(it);
		} else {
			flush();
			buildBlock(parent, *it);
		}
	}
	flush();
}

void MarkdownBuilder::buildBlock(Node *parent, const document::Node &source) {
	auto tag = source.getHtmlName();

	MarkdownBuilderContext ctx{this, parent, &source};

	auto factory = _registry->get(tag);
	if (!factory || !factory->create) {
		// An unknown block is transparent rather than fatal: its children still reach the tree,
		// so a parser extension nobody taught the builder about loses its box, not its text.
		buildChildren(parent, source);
		return;
	}

	auto node = factory->create(ctx);
	if (!node) {
		return; // the factory dropped this subtree deliberately
	}

	auto added = parent->addChild(sp::move(node));
	applyIdentity(added, tag);

	// The document's own id and classes: `#id` selectors (and, later, anchors) and whatever the
	// parser attached, e.g. `language-cpp` on a fenced block.
	if (!source.getHtmlId().empty()) {
		added->setName(source.getHtmlId());
	}
	for (auto &cl : source.getClasses()) { added->addStyleClass(cl); }

	++_blocks;

	if (factory->buildContent && factory->buildContent(ctx, added)) {
		return;
	}

	if (factory->textContent) {
		if (auto label = dynamic_cast<basic2d::Label *>(added)) {
			buildText(label, source);
			return;
		}
		log::source().warn("ui::MarkdownBuilder",
				"textContent on a node that is not a Label: ", tag);
	}

	buildChildren(added, source);
}

void MarkdownBuilder::buildText(basic2d::Label *label, const document::Node &source) {
	TextState state;
	for (auto &it : source.getNodes()) { collectText(state, *it, MarkdownInline::Text); }
	commitText(label, state);
}

void MarkdownBuilder::buildRawText(basic2d::Label *label, const document::Node &source) {
	// A code fence has no inline markup by definition, so the walk is the same one without the
	// style ranges - the runs still have to be recorded, because copying a fence is copying it
	// out of the source.
	TextState state;
	for (auto &it : source.getNodes()) { collectText(state, *it, MarkdownInline::Text); }
	state.ranges.clear();
	commitText(label, state);
}

void MarkdownBuilder::collectText(TextState &state, const document::Node &source,
		MarkdownInline kind) {
	auto tag = source.getHtmlName();

	if (tag == s_valueTag) {
		appendValue(state, source);
		return;
	}

	if (tag == "br") {
		// A hard break is a real newline: the label's white-space mode preserves it (the CSS the
		// view ships sets `normal` for prose, which still honours an explicit \n).
		state.text.push_back(u'\n');
		return;
	}

	if (tag == "img") {
		// Images are a later milestone; the alt text keeps the sentence readable meanwhile, and
		// carries no run because it is not a slice of the source.
		auto alt = source.getAttribute("alt");
		if (!alt.empty()) {
			state.text.append(string::toUtf16<Interface>(alt));
		}
		return;
	}

	auto own = getMarkdownInlineForTag(tag);
	auto effective = (own == MarkdownInline::Max) ? kind : own;

	auto start = uint32_t(state.text.size());
	for (auto &it : source.getNodes()) { collectText(state, *it, effective); }
	auto count = uint32_t(state.text.size()) - start;

	if (count == 0 || own == MarkdownInline::Max) {
		return;
	}

	state.ranges.emplace_back(pair(pair(start, count), own));

	if (own == MarkdownInline::Link) {
		state.links.emplace_back(
				MarkdownRunMap::Link{start, count, source.getAttribute("href").str<Interface>(),
					source.getAttribute("title").str<Interface>()});
	}
}

void MarkdownBuilder::appendValue(TextState &state, const document::Node &value) {
	auto text = value.getValue();
	if (text.empty()) {
		return;
	}

	auto start = uint32_t(state.text.size());
	state.text.append(text.data(), text.size());

	auto span = value.getSourceSpan();
	if (span.empty() || span.end() > _source.size()) {
		return; // a run the parser could not place; the text is still shown
	}

	state.runs.emplace_back(MarkdownRunMap::Run{start, uint32_t(text.size()), span.offset,
		span.length, isVerbatim(text, span)});
}

bool MarkdownBuilder::isVerbatim(WideStringView text, document::SourceSpan span) const {
	// The comparison has to be against the DECODED source, because that is what the parser
	// produced: `&amp;` became one character, and smart typography turned quotes, dashes and
	// ellipses into single ones. An ellipsis even keeps the byte count, so comparing lengths
	// would call a substituted run verbatim.
	auto fragment = _source.sub(span.offset, span.length);
	auto decoded = string::toUtf16<Interface>(fragment);
	return WideStringView(decoded) == text;
}

void MarkdownBuilder::commitText(basic2d::Label *label, TextState &state) {
	// The document keeps its text verbatim, newlines and all, because a newline between two words
	// is a space and only the block knows which one it is. Trailing whitespace, though, belongs to
	// no word: it is trimmed once per block, and the last run shrinks with it so the source map
	// still ends where the text does.
	auto trimmed = uint32_t(state.text.size());
	while (trimmed > 0
			&& (state.text[trimmed - 1] == u'\n' || state.text[trimmed - 1] == u'\r'
					|| state.text[trimmed - 1] == u' ' || state.text[trimmed - 1] == u'\t')) {
		--trimmed;
	}
	if (trimmed != state.text.size()) {
		state.text.resize(trimmed);
		while (!state.runs.empty() && state.runs.back().charStart >= trimmed) {
			state.runs.pop_back();
		}
		if (!state.runs.empty()) {
			auto &last = state.runs.back();
			if (last.charStart + last.charCount > trimmed) {
				last.charCount = trimmed - last.charStart;
			}
		}
	}

	// setString clears the style vector, so the string always goes first.
	label->setString(WideStringView(state.text));

	// Ascending by start, and an enclosing range before the range it encloses: a later range wins
	// per parameter, which is what makes `**bold *and italic***` compose instead of fight.
	sprt::sort(state.ranges.begin(), state.ranges.end(), [](const auto &l, const auto &r) {
		if (l.first.first != r.first.first) {
			return l.first.first < r.first.first;
		}
		return l.first.second > r.first.second;
	});

	for (auto &it : state.ranges) {
		auto style = _styles.get(it.second);
		if (style.params.empty()) {
			continue;
		}
		label->setTextRangeStyle(it.first.first, it.first.second, sp::move(style));
	}

	if (!state.runs.empty() || !state.links.empty()) {
		label->setComponent<MarkdownRunMap>(
				MarkdownRunMap{sp::move(state.runs), sp::move(state.links)});
	}
}

} // namespace stappler::xenolith::ui
