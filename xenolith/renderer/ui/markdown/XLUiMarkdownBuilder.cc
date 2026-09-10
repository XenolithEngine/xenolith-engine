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

// The tags open at a point of the descent, as the key the cascade is asked and cached on.
static String MarkdownBuilder_chainKey(SpanView<StringView> chain) {
	StringStream out;
	for (size_t i = 0; i < chain.size(); ++i) {
		if (i > 0) {
			out << '>';
		}
		out << chain[i];
	}
	return out.str();
}

MarkdownBuilder::MarkdownBuilder(NotNull<Node> root, NotNull<MarkdownRegistry> registry,
		const MarkdownInlineStyles &styles, font::FontController *controller)
: _root(root)
, _registry(registry)
, _flow(Rc<MarkdownFlow>::alloc())
, _styles(styles)
, _inlineResolver(controller) { }

document::SourceSpan MarkdownBuilder::spanOf(const document::Node &source) {
	auto node = &source;
	while (node) {
		auto span = node->getSourceSpan();
		if (!span.empty()) {
			return span;
		}
		node = node->getParent();
	}
	return document::SourceSpan();
}

uint32_t MarkdownBuilder::registerFlow(Node *node, MarkdownFlowKind kind,
		const document::Node &source, uint32_t textLength) {
	auto index = _flow->emplace(node, kind, spanOf(source), textLength, &source);

	// The first thing readable inside a block is where that block's id points. A list item's `id`
	// is on the item, but the item is a container with no position of its own - this is the
	// moment a position exists for it.
	if (!_pendingAnchors.empty()) {
		auto textBegin = _flow->getEntries()[index].textBegin;
		for (auto &it : _pendingAnchors) { _anchors.emplace(sp::move(it), textBegin); }
		_pendingAnchors.clear();
	}

	return index;
}

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
		commitText(label, state, source);
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
	auto pending = _pendingAnchors.size();
	buildBlockContent(parent, source);

	// An id on a block that produced nothing readable stays unbound rather than drifting onto
	// whatever block comes next.
	if (_pendingAnchors.size() > pending) {
		_pendingAnchors.resize(pending);
	}
}

void MarkdownBuilder::buildBlockContent(Node *parent, const document::Node &source) {
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
		_pendingAnchors.emplace_back(source.getHtmlId().str<Interface>());
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
	commitText(label, state, source);
}

void MarkdownBuilder::buildRawText(basic2d::Label *label, const document::Node &source) {
	// A code fence has no inline markup by definition, so the walk is the same one without the
	// style ranges - the runs still have to be recorded, because copying a fence is copying it
	// out of the source.
	TextState state;
	for (auto &it : source.getNodes()) { collectText(state, *it, MarkdownInline::Text); }
	state.ranges.clear();
	commitText(label, state, source);
}

void MarkdownBuilder::applyInlineStyles(basic2d::Label *label, Vector<TextRange> &ranges) {
	// Ascending by start, and an enclosing range before the range it encloses: a later range wins
	// per parameter, which is what makes `**bold *and italic***` compose instead of fight.
	sprt::sort(ranges.begin(), ranges.end(), [](const TextRange &l, const TextRange &r) {
		if (l.start != r.start) {
			return l.start < r.start;
		}
		return l.count > r.count;
	});

	for (auto &it : ranges) {
		basic2d::Label::Style style;

		/* The stylesheet decides, and its SILENCE decides too.

		A sheet in scope that says nothing about `strong` means the document is not to embolden
		it - so the built-in table is consulted only when no sheet answered at all, which is the
		case of a view built outside any scene. Anything else would make the table a floor no
		stylesheet could lower. */
		if (!_inlineResolver.resolve(label, it.chain, style) && !_inlineResolver.isValid()) {
			style = _styles.get(it.kind);
		}

		if (style.params.empty()) {
			continue;
		}
		label->setTextRangeStyle(it.start, it.count, sp::move(style));
	}
}

void MarkdownBuilder::restyleText(basic2d::Label *label, const document::Node &source) {
	// The same walk as the build, for its POSITIONS only: the text it rebuilds is thrown away,
	// and with it the runs and links, which the Label already carries and which a stylesheet
	// cannot change. A verbatim block has no inline elements to find, so this correctly produces
	// nothing for a code fence.
	TextState state;
	for (auto &it : source.getNodes()) { collectText(state, *it, MarkdownInline::Text); }

	if (state.ranges.empty() && label->getStyles().empty()) {
		return;
	}

	label->clearStyles();
	applyInlineStyles(label, state.ranges);
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
		collectImage(state, source);
		return;
	}

	auto own = getMarkdownInlineForTag(tag);
	auto effective = (own == MarkdownInline::Max) ? kind : own;
	auto inlineElement = (own != MarkdownInline::Max);

	// The DOCUMENT's tag, not the canonical name of the kind: it is what a stylesheet author
	// sees and writes, and the two differ wherever a construct has an alias (`b` for `strong`).
	if (inlineElement) {
		state.chain.emplace_back(tag);
	}

	auto start = uint32_t(state.text.size());
	for (auto &it : source.getNodes()) { collectText(state, *it, effective); }
	auto count = uint32_t(state.text.size()) - start;

	// An id on an inline is the only record that the reference site exists: there is no node for
	// it, and a footnote's way back is exactly such an id.
	if (inlineElement && !source.getHtmlId().empty()) {
		state.anchors.emplace_back(source.getHtmlId().str<Interface>(), start);
	}

	// The chain is captured while this element is still on it, and only then popped.
	if (inlineElement) {
		if (count > 0) {
			state.ranges.emplace_back(
					TextRange{start, count, own, MarkdownBuilder_chainKey(state.chain)});
		}
		state.chain.pop_back();
	}

	if (count == 0 || !inlineElement) {
		return;
	}

	if (own == MarkdownInline::Link) {
		state.links.emplace_back(
				MarkdownRunMap::Link{start, count, source.getAttribute("href").str<Interface>(),
					source.getAttribute("title").str<Interface>()});
	}
}

// A `width`/`height` the author wrote in the markup. MMD puts them in a `style` attribute, which
// the document processor parsed into the node's own style list - so this is the resolved metric,
// and only an absolute one is usable here: a percentage has nothing to be a percentage OF until
// the paragraph is being laid out, which is after the box has to be known.
static float MarkdownBuilder_declaredMetric(const document::Node &source,
		document::ParameterName name) {
	for (auto &it : source.getStyle().get(name)) {
		if (it.value.sizeValue.metric == document::Metric::Units::Px) {
			return it.value.sizeValue.value;
		}
	}
	return 0.0f;
}

void MarkdownBuilder::collectImage(TextState &state, const document::Node &source) {
	MarkdownImageRequest request{source.getAttribute("src"), source.getAttribute("alt"),
		Size2(MarkdownBuilder_declaredMetric(source, document::ParameterName::CssWidth),
				MarkdownBuilder_declaredMetric(source, document::ParameterName::CssHeight))};

	MarkdownImageSource resolved;
	if (_imageResolver) {
		resolved = (*_imageResolver)(request);
	} else if (request.declared.width > 0.0f && request.declared.height > 0.0f) {
		resolved.size = request.declared;
	}

	/* Nothing to show and no size to keep: the picture leaves its alt text behind, which is what
	a reader of a README with a missing badge should see. It is text like any other and takes no
	run, because those characters are not a slice of the source. */
	if (resolved.size.width <= 0.0f || resolved.size.height <= 0.0f) {
		auto alt = request.alt;
		if (!alt.empty()) {
			state.text.append(string::toUtf16<Interface>(alt));
		}
		return;
	}

	/* ONE character stands for the picture, and the formatter is told to leave a box the size of
	that picture where the character is. The character is what keeps the image in the reading
	order: a caret can stand beside it, a selection can contain it, and the run below points it at
	the `![alt](src)` that produced it - so copying a selection that crosses a picture copies the
	picture's markup. */
	auto charIndex = uint32_t(state.text.size());
	state.text.push_back(MarkdownObjectChar);

	auto span = source.getSourceSpan();
	if (!span.empty() && span.end() <= _source.size()) {
		// Not verbatim: one character on screen, a whole `![alt](src)` in the source. The flag is
		// what stops a copy from slicing into the middle of it.
		state.runs.emplace_back(MarkdownRunMap::Run{charIndex, 1, span.offset, span.length, false});
	}

	state.objects.emplace_back(charIndex, request.alt.str<Interface>());
	state.images.emplace_back(
			TextState::Image{charIndex, resolved.size, sp::move(resolved.texture), &source});
}

void MarkdownBuilder::commitImages(basic2d::Label *label, TextState &state) {
	if (state.images.empty()) {
		label->clearInlineObjects();
		return;
	}

	Vector<basic2d::Label::InlineObject> objects;
	objects.reserve(state.images.size());
	for (auto &it : state.images) {
		objects.emplace_back(basic2d::Label::InlineObject{it.charIndex, it.size});
	}
	label->setInlineObjects(sp::move(objects));

	auto system = label->addSystem(Rc<MarkdownImageSystem>::create());

	uint32_t index = 0;
	for (auto &it : state.images) {
		auto sprite = label->addChild(Rc<basic2d::Sprite>::create(), ZOrder(1));
		applyIdentity(sprite, "img");
		if (it.source && !it.source->getHtmlId().empty()) {
			sprite->setName(it.source->getHtmlId());
		}
		if (it.source) {
			for (auto &cl : it.source->getClasses()) { sprite->addStyleClass(cl); }
		}

		// Photographic content, not an icon: the sprite's own default is nearest.
		sprite->setSamplerIndex(core::SamplerIndex::DefaultFilterLinear);
		sprite->setTextureAutofit(Autofit::Contain);
		if (it.texture) {
			sprite->setTexture(Rc<Texture>(it.texture));
		}

		// It has no place in any layout: it stands where the text left a hole for it, and the
		// only thing that may move it is the system that reads that hole back.
		sprite->setComponent<OutOfFlowComponent>();
		sprite->setVisible(false);

		system->addImage(sprite, index);
		++index;
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

void MarkdownBuilder::commitText(basic2d::Label *label, TextState &state,
		const document::Node &source) {
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

	applyInlineStyles(label, state.ranges);

	// After the string and before anything reads the layout: the objects are part of how the text
	// shapes, not decoration over it.
	commitImages(label, state);

	// Always, even with no runs at all: the component is also how a node finds itself in the
	// reading order, and a block whose text the parser could not place still has a place in it.
	label->setComponent<MarkdownRunMap>(
			MarkdownRunMap{sp::move(state.objects), sp::move(state.runs), sp::move(state.links)});

	auto index = registerFlow(label, MarkdownFlowKind::Text, source, uint32_t(state.text.size()));

	// Now that the block has a place in the reading order, the inline ids collected inside it can
	// be turned into document positions.
	if (!state.anchors.empty()) {
		auto textBegin = _flow->getEntries()[index].textBegin;
		for (auto &it : state.anchors) {
			_anchors.emplace(sp::move(it.first), textBegin + it.second);
		}
	}
}

} // namespace stappler::xenolith::ui
