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

#include "XLUiMarkdownRegistry.h"
#include "XLUiMarkdownBuilder.h"
#include "XLUiPanel.h"
#include "XLUiCheckbox.h"
#include "XLUiTableBorderPainter.h"
#include "XLUiLayoutSystem.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

/* A Panel answering to a markdown tag. A subclass because `Panel::registerStyleAppliers` is
protected; it gives `pre`, `blockquote-body` and `table` CSS background, outline and rounded
corners, which a plain Node would treat as a tint. */
class MarkdownPanel : public Panel {
public:
	virtual bool init(StringView type) {
		if (!Panel::init()) {
			return false;
		}

		// Drop Panel::init()'s class so a sheet's panel rules do not reach document blocks.
		removeStyleClass("xl-ui-panel");
		setType(type);
		registerStyleAppliers(type);
		return true;
	}

protected:
	using Panel::init;
};

// A block whose children are all inline: the node is the Label, so `p { color: … }` reaches its
// text.
static MarkdownTagFactory makeTextFactory() {
	return MarkdownTagFactory{
		.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<basic2d::Label>::create());
	},
		.textContent = true,
	};
}

// A block that only groups and lays out its children; it paints nothing, so a plain Node.
static MarkdownTagFactory makeContainerFactory() {
	return MarkdownTagFactory{
		.create = [](const MarkdownBuilderContext &) -> Rc<Node> { return Rc<Node>::create(); },
	};
}

/* A ground for a block's content that paints only when a stylesheet gives it a background. A
Node's alpha and rgb cascade into descendants, so the colour is white and both opacity and colour
cascades are off; otherwise the ground would fade or tint its content. */
static Rc<basic2d::Layer> makeGroundLayer() {
	auto layer = Rc<basic2d::Layer>::create(Color4F(1.0f, 1.0f, 1.0f, 0.0f));
	layer->setCascadeColorEnabled(false);
	layer->setCascadeOpacityEnabled(false);
	return layer;
}

// A layer, for a block that is nothing but a painted rectangle (a rule, a table row's ground).
static MarkdownTagFactory makeLayerFactory() {
	return MarkdownTagFactory{
		.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(makeGroundLayer());
	},
	};
}

// The 1-based position of a list item among its siblings, plus whatever `start` the parser read
// off an ordered list.
static uint32_t MarkdownRegistry_itemIndex(const document::Node &item) {
	auto parent = item.getParent();
	if (!parent) {
		return 1;
	}

	// Counting preceding siblings is quadratic over a list; acceptable for typical documents. A
	// fix belongs in the builder's walk, not in a cache written into the parsed document.
	uint32_t index = 0;
	for (auto &it : parent->getNodes()) {
		if (it->getHtmlName() == "li") {
			++index;
			if (it == &item) {
				break;
			}
		}
	}

	uint32_t start = 1;
	auto attr = parent->getAttribute("start");
	if (!attr.empty()) {
		start = uint32_t(attr.readInteger(10).get(1));
	}
	return start + index - 1;
}

static bool MarkdownRegistry_isOrdered(const document::Node &item) {
	auto parent = item.getParent();
	return parent && parent->getHtmlName() == "ol";
}

// How deep the enclosing list chain is, so nested bullets can differ without `::before` or a
// depth variable.
static uint32_t MarkdownRegistry_listDepth(const document::Node &item) {
	uint32_t depth = 0;
	auto walker = item.getParent();
	while (walker) {
		auto name = walker->getHtmlName();
		if (name == "ul" || name == "ol") {
			++depth;
		}
		walker = walker->getParent();
	}
	return depth == 0 ? 0 : depth - 1;
}

// The `input[type=checkbox]` a task item carries, or nullptr. The CSS subset has no attribute
// selectors, so the state reaches a stylesheet as a class and as the checkbox's `:checked`.
static const document::Node *MarkdownRegistry_taskBox(const document::Node &item) {
	for (auto &it : item.getNodes()) {
		if (it->getHtmlName() == "input" && it->getAttribute("type") == "checkbox") {
			return it;
		}
	}
	return nullptr;
}

static StringView MarkdownRegistry_bullet(uint32_t depth) {
	switch (depth % 3) {
	case 0: return StringView("•");
	case 1: return StringView("◦");
	default: return StringView("▪");
	}
}

} // namespace

Rc<MarkdownRegistry> MarkdownRegistry::createDefault() {
	auto ret = Rc<MarkdownRegistry>::alloc();

	// --- blocks that are their own text ---------------------------------------------------

	for (auto &tag : {StringView("p"), StringView("h1"), StringView("h2"), StringView("h3"),
			 StringView("h4"), StringView("h5"), StringView("h6"), StringView("dt"),
			 StringView("th"), StringView("td"), StringView("caption"), StringView("figcaption")}) {
		ret->set(tag, makeTextFactory());
	}

	// --- structural containers ------------------------------------------------------------

	for (auto &tag : {StringView("ul"), StringView("ol"), StringView("dl"), StringView("dd"),
			 StringView("div"), StringView("figure"), StringView("thead"), StringView("tbody")}) {
		ret->set(tag, makeContainerFactory());
	}

	// A column definition carries no content of its own; the table reads it for the track count.
	ret->set("colgroup",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> { return nullptr; },
			});

	// --- painted blocks -------------------------------------------------------------------

	// A rule holds a place in the reading order though it has nothing to read; a row does not.
	ret->set("hr",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.0f)));
	},
				.buildContent = [](const MarkdownBuilderContext &ctx, Node *node) -> bool {
		ctx.builder->registerFlow(node, MarkdownFlowKind::Atomic, *ctx.source);
		return true;
	},
			});
	ret->set("tr", makeLayerFactory());

	// --- list item ------------------------------------------------------------------------

	ret->set("li",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>::create();
	},
				.buildContent = [](const MarkdownBuilderContext &ctx, Node *node) -> bool {
		auto builder = ctx.builder;
		auto ordered = MarkdownRegistry_isOrdered(*ctx.source);
		auto depth = MarkdownRegistry_listDepth(*ctx.source);

		// The marker is a sibling node (no `::before` in the CSS subset), so the indent is a layout
		// column.
		if (auto box = MarkdownRegistry_taskBox(*ctx.source)) {
			// A task item's marker is the checkbox, disabled: the state has nowhere to be written.
			auto checkbox = node->addChild(Rc<Checkbox>::create());
			MarkdownBuilder::applyIdentity(checkbox, "li-checkbox");
			checkbox->addStyleClass("md-marker");

			// An initial size: the widget has no text to measure; the sheet's `li-checkbox` rule
			// overrides it on the first style pass.
			checkbox->setContentSize(Size2(15.0f, 15.0f));
			LayoutSystem::setItem(checkbox,
					FlexItemInfo{.grow = 0.0f, .shrink = 0.0f, .basis = 15.0f});
			checkbox->setChecked(!box->getAttribute("checked").empty(), true);
			checkbox->setEnabled(false);
			node->addStyleClass("md-task");

			// Stands for the source's `[x]` and carries its span.
			builder->registerFlow(checkbox, MarkdownFlowKind::Atomic, *box);
		} else {
			auto marker = builder->makeLabel(node, "li-marker");
			marker->removeStyleClass("md-li-marker");
			marker->addStyleClass("md-marker");
			if (ordered) {
				marker->setString(toString(MarkdownRegistry_itemIndex(*ctx.source), "."));
				marker->addStyleClass("md-marker-ordered");
			} else {
				marker->setString(MarkdownRegistry_bullet(depth));
				marker->addStyleClass("md-marker-bullet");
			}

			// Builder-written text: in the flow, mapped to the item rather than to source bytes.
			builder->registerFlow(marker, MarkdownFlowKind::Marker, *ctx.source,
					uint32_t(marker->getString().size()));
		}

		auto content = node->addChild(Rc<Node>::create());
		MarkdownBuilder::applyIdentity(content, "li-content");

		// Tight items hold text directly, loose ones wrap it in `p`; the implicit-run rule covers
		// both.
		builder->buildChildren(content, *ctx.source);
		return true;
	},
			});

	// --- blockquote -----------------------------------------------------------------------

	ret->set("blockquote",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>::create();
	},
				.buildContent = [](const MarkdownBuilderContext &ctx, Node *node) -> bool {
		// `border-left` applies only to table cells here, so the quote bar is a node stretched by
		// the row layout.
		auto bar = node->addChild(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.0f)));
		MarkdownBuilder::applyIdentity(bar, "blockquote-bar");

		auto body = node->addChild(Rc<MarkdownPanel>::create(StringView("blockquote-body")));
		MarkdownBuilder::applyIdentity(body, "blockquote-body");

		ctx.builder->buildChildren(body, *ctx.source);
		return true;
	},
			});

	// --- code -----------------------------------------------------------------------------

	ret->set("pre",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<MarkdownPanel>::create(StringView("pre")));
	},
				.buildContent = [](const MarkdownBuilderContext &ctx, Node *node) -> bool {
		// Both fenced and indented blocks are `pre > code`; the inner `code` carries the language
		// class and is the Label.
		const document::Node *code = nullptr;
		for (auto &it : ctx.source->getNodes()) {
			if (it->getHtmlName() == "code") {
				code = it;
				break;
			}
		}

		// The scroll lives on an inner node: one overflow axis computes the other to `auto`, and an
		// overflowing container is content-sized on both axes, so a scrolling `pre` would lose its
		// height.
		auto scroll = node->addChild(Rc<Node>::create());
		MarkdownBuilder::applyIdentity(scroll, "pre-scroll");

		auto label = ctx.builder->makeLabel(scroll, "code");
		label->addStyleClass("md-code-block");
		if (code) {
			for (auto &cl : code->getClasses()) { label->addStyleClass(cl); }
			ctx.builder->buildRawText(label, *code);
		} else {
			ctx.builder->buildRawText(label, *ctx.source);
		}
		return true;
	},
			});

	// The builder never dispatches inline `code` here; registered as text so a block-level
	// `code` in a hand-built document still renders.
	ret->set("code", makeTextFactory());

	// --- table ----------------------------------------------------------------------------

	ret->set("table",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<MarkdownPanel>::create(StringView("table")));
	},
				.buildContent = [](const MarkdownBuilderContext &ctx, Node *node) -> bool {
		// `display: table` expects rows as direct children, so thead/tbody are flattened; each row
		// gets a section class (not :nth-child, which would stripe the header too).
		uint32_t columns = 0;
		auto addRows = [&](const document::Node &section, StringView cls) {
			for (auto &row : section.getNodes()) {
				if (row->getHtmlName() != "tr") {
					continue;
				}
				uint32_t count = 0;
				for (auto &cell : row->getNodes()) {
					auto name = cell->getHtmlName();
					if (name == "td" || name == "th") {
						++count;
					}
				}
				columns = sprt::max(columns, count);

				auto rowNode = node->addChild(makeGroundLayer());
				MarkdownBuilder::applyIdentity(rowNode, "tr");
				rowNode->addStyleClass(cls);
				ctx.builder->buildChildren(rowNode, *row);
			}
		};

		for (auto &it : ctx.source->getNodes()) {
			auto name = it->getHtmlName();
			if (name == "thead") {
				addRows(*it, "md-thead-row");
			} else if (name == "tbody") {
				addRows(*it, "md-tbody-row");
			} else if (name == "tr") {
				// a table without sections
				uint32_t count = 0;
				for (auto &cell : it->getNodes()) {
					auto cellName = cell->getHtmlName();
					if (cellName == "td" || cellName == "th") {
						++count;
					}
				}
				columns = sprt::max(columns, count);

				auto rowNode = node->addChild(makeGroundLayer());
				MarkdownBuilder::applyIdentity(rowNode, "tr");
				rowNode->addStyleClass("md-tbody-row");
				ctx.builder->buildChildren(rowNode, *it);
			}
		}

		// The column count reaches the sheet through a custom property read with var().
		if (columns > 0) {
			setStyleVariable(node, "--md-columns", toString("repeat(", columns, ", auto)"));
		}

		// The table layout publishes collapsed borders as geometry only; this draws them. Out of
		// flow, or the table would count it as a row.
		auto painter = node->addChild(Rc<TableBorderPainter>::create(), ZOrder(10));
		painter->setComponent<OutOfFlowComponent>();
		return true;
	},
			});

	// --- tags that produce no node ---------------------------------------------------------

	// Images are inline and built by the builder; a block-level `img` produces nothing.
	ret->set("img",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> { return nullptr; },
			});

	// A task item's checkbox is drawn as the item's marker (see the `li` factory), so the node it
	// arrived in renders nothing of its own.
	ret->set("input",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> { return nullptr; },
			});

	return ret;
}

void MarkdownRegistry::set(StringView tag, MarkdownTagFactory &&factory) {
	auto it = _tags.find(tag);
	if (it != _tags.end()) {
		it->second = sp::move(factory);
	} else {
		_tags.emplace(tag.str<Interface>(), sp::move(factory));
	}
}

const MarkdownTagFactory *MarkdownRegistry::get(StringView tag) const {
	auto it = _tags.find(tag);
	if (it != _tags.end()) {
		return &it->second;
	}
	return nullptr;
}

} // namespace stappler::xenolith::ui
