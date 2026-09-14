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

// Whether a child belongs in the block's own Label. `br` and the transparent wrappers are inline
// without being styled.
static bool MarkdownBuilder_isInline(StringView tag) {
	if (tag == s_valueTag || tag == "br" || tag == "span" || tag == "abbr" || tag == "img"
			|| tag == "input") {
		return true;
	}
	return getMarkdownInlineForTag(tag) != MarkdownInline::Max;
}

// The open tags as the key the cascade is queried and cached on.
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

	// A block's id points at the first readable entry inside it; containers like a list item have
	// no position of their own.
	if (!_pendingAnchors.empty()) {
		auto textBegin = _flow->getEntries()[index].textBegin;
		for (auto &it : _pendingAnchors) { _anchors.emplace(sp::move(it), textBegin); }
		_pendingAnchors.clear();
	}

	return index;
}

uint32_t MarkdownBuilder::build(const document::Node &page) {
	_blocks = 0;

	// Blocks are added one at a time; without bulk mode the root re-sorts on each and the build
	// is quadratic.
	Node::BulkChildren bulk(_root);

	buildChildren(_root, page);
	return _blocks;
}

void MarkdownBuilder::applyIdentity(Node *node, StringView type) {
	node->setType(type);
	node->addStyleClass(toString("md-", type));

	// Document order is z-order here, and `sortAllChildren` is not stable, so each block takes a
	// distinct order from its insertion position.
	if (auto parent = node->getParent()) {
		node->setLocalZOrder(ZOrder(int16_t(parent->getChildren().size())));
	}

	// Layout places nodes by their box origin, which is bottom-left in the Y-up engine.
	node->setAnchorPoint(Anchor::BottomLeft);

	// Labels arrive from a tag factory or an implicit inline run; both end here. Code is excluded:
	// it must not wrap, scrolls sideways, and measures its own height from its lines.
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
	// Consecutive inline children with no block between them get an implicit Label, typed after
	// the containing block (a tight list item, a table cell).
	Vector<const document::Node *> pending;

	auto flush = [&] {
		if (pending.empty()) {
			return;
		}

		TextState state;
		for (auto &it : pending) { collectText(state, *it, MarkdownInline::Text); }
		pending.clear();

		// A whitespace-only run separates blocks and gets no label.
		auto trimmed = WideStringView(state.text);
		trimmed.trimChars<WideStringView::WhiteSpace>();
		if (trimmed.empty()) {
			return;
		}

		// `<tag>-text`, never the container's tag: a Label typed `li` would match the `li` rule,
		// become a flex container and measure zero height. Inherited text properties still apply.
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

	// An id on a block that produced nothing readable stays unbound.
	if (_pendingAnchors.size() > pending) {
		_pendingAnchors.resize(pending);
	}
}

void MarkdownBuilder::buildBlockContent(Node *parent, const document::Node &source) {
	auto tag = source.getHtmlName();

	MarkdownBuilderContext ctx{this, parent, &source};

	auto factory = _registry->get(tag);
	if (!factory || !factory->create) {
		// An unknown block is transparent: its children are still built.
		buildChildren(parent, source);
		return;
	}

	auto node = factory->create(ctx);
	if (!node) {
		return; // the factory dropped this subtree deliberately
	}

	auto added = parent->addChild(sp::move(node));
	applyIdentity(added, tag);

	// The document's own id and classes, e.g. `language-cpp` on a fenced block.
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
	// A code fence has no inline markup, but its runs are still recorded so a copy maps back to
	// the source.
	TextState state;
	for (auto &it : source.getNodes()) { collectText(state, *it, MarkdownInline::Text); }
	state.ranges.clear();
	commitText(label, state, source);
}

void MarkdownBuilder::applyInlineStyles(basic2d::Label *label, Vector<TextRange> &ranges) {
	// Ascending by start, enclosing ranges first: a later range wins per parameter, so nested
	// constructs compose.
	sprt::sort(ranges.begin(), ranges.end(), [](const TextRange &l, const TextRange &r) {
		if (l.start != r.start) {
			return l.start < r.start;
		}
		return l.count > r.count;
	});

	for (auto &it : ranges) {
		basic2d::Label::Style style;

		// A sheet in scope that says nothing about a construct leaves it unstyled; the built-in
		// table is used only when no sheet is in scope at all.
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
	// The build walk repeated for range positions only; the rebuilt text, runs and links are
	// discarded. A verbatim block yields nothing.
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
		// A hard break is a real newline, which `white-space: normal` still honours.
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

	// The document's tag, not the kind's canonical name: stylesheets see aliases (`b`).
	if (inlineElement) {
		state.chain.emplace_back(tag);
	}

	auto start = uint32_t(state.text.size());
	for (auto &it : source.getNodes()) { collectText(state, *it, effective); }
	auto count = uint32_t(state.text.size()) - start;

	// An inline id is the only record of its site (e.g. a footnote reference).
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

// A `width`/`height` declared in the markup's `style` attribute. Only absolute metrics are used:
// the box is needed before the paragraph is laid out, so a percentage has no base.
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

	// Nothing to show: the alt text is inserted instead, with no run, since it is not a slice of
	// the source.
	if (resolved.size.width <= 0.0f || resolved.size.height <= 0.0f) {
		auto alt = request.alt;
		if (!alt.empty()) {
			state.text.append(string::toUtf16<Interface>(alt));
		}
		return;
	}

	// One character stands for the picture and the formatter reserves a box there. The character
	// keeps the image in reading order, and its run maps a copy back to the `![alt](src)` markup.
	auto charIndex = uint32_t(state.text.size());
	state.text.push_back(MarkdownObjectChar);

	auto span = source.getSourceSpan();
	if (!span.empty() && span.end() <= _source.size()) {
		// Not verbatim: one character on screen, a whole `![alt](src)` in the source, so a copy
		// never slices into it.
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

		// Positioned only by the image system, over the hole the text reserved.
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
	// Compare against the decoded source (entities, smart typography); lengths alone cannot tell,
	// an ellipsis keeps the byte count. Decoded lazily while comparing: this runs once per run.
	auto fragment = _source.sub(span.offset, span.length);
	size_t at = 0;
	size_t pos = 0;

	while (pos < fragment.size()) {
		uint8_t consumed = 0;
		auto ch =
				sprt::unicode::utf8Decode32(fragment.data() + pos, fragment.size() - pos, consumed);
		if (consumed == 0) {
			return false;
		}
		pos += consumed;

		char16_t buf[2] = {0, 0};
		auto units = sprt::unicode::utf16EncodeBuf(buf, 2, ch);
		for (uint8_t i = 0; i < units; ++i) {
			if (at >= text.size() || text[at] != buf[i]) {
				return false;
			}
			++at;
		}
	}

	return at == text.size();
}

void MarkdownBuilder::commitText(basic2d::Label *label, TextState &state,
		const document::Node &source) {
	// Trailing whitespace is trimmed once per block, and the last run shrinks with it so the source
	// map ends where the text does.
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

	// After the string and before layout: objects affect shaping.
	commitImages(label, state);

	// Always, even with no runs: the component is also how a node finds its reading-order entry.
	label->setComponent<MarkdownRunMap>(
			MarkdownRunMap{sp::move(state.objects), sp::move(state.runs), sp::move(state.links)});

	auto index = registerFlow(label, MarkdownFlowKind::Text, source, uint32_t(state.text.size()));

	// Resolve inline ids into document positions now that the block has a place in the order.
	if (!state.anchors.empty()) {
		auto textBegin = _flow->getEntries()[index].textBegin;
		for (auto &it : state.anchors) {
			_anchors.emplace(sp::move(it.first), textBegin + it.second);
		}
	}
}

} // namespace stappler::xenolith::ui
