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

/* A Panel answering to a markdown tag.

A derived class rather than a call, because `Panel::registerStyleAppliers` is protected: the
appliers route back through `Panel::setStyleValue`, so whoever claims a type has to BE a Panel.
This is what gives `pre`, `blockquote-body` and `table` a background, an outline and rounded
corners from CSS - a plain Node would take `background-color` as a tint and draw nothing with it. */
class MarkdownPanel : public Panel {
public:
	virtual bool init(StringView type) {
		if (!Panel::init()) {
			return false;
		}

		// Panel::init() made this a `panel`; a sheet's panel rules paint cards and dialogs and
		// have no business reaching a code block.
		removeStyleClass("xl-ui-panel");
		setType(type);
		registerStyleAppliers(type);
		return true;
	}

protected:
	using Panel::init;
};

// A block whose children are all inline: the node IS the Label, so a `p { color: … }` rule lands
// on the node whose text it is meant to colour.
static MarkdownTagFactory makeTextFactory() {
	return MarkdownTagFactory{
		.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<basic2d::Label>::create());
	},
		.textContent = true,
	};
}

// A block that only groups and lays its children out. It paints nothing, so it is a plain Node -
// see MarkdownPanel above for the ones that do.
static MarkdownTagFactory makeContainerFactory() {
	return MarkdownTagFactory{
		.create = [](const MarkdownBuilderContext &) -> Rc<Node> { return Rc<Node>::create(); },
	};
}

static MarkdownTagFactory makePanelFactory(StringView type) {
	return MarkdownTagFactory{
		.create = [type = type.str<Interface>()](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<MarkdownPanel>::create(StringView(type)));
	},
	};
}

// A layer, for a block that is nothing but a painted rectangle (a rule, a table row's ground).
static MarkdownTagFactory makeLayerFactory() {
	return MarkdownTagFactory{
		.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.0f)));
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

	// Counting the siblings before this one is O(items) per item, i.e. quadratic over the list.
	// Acceptable for the lists a document actually contains; a document that is one enormous
	// numbered list is the case to fix, and the fix belongs in the builder's own walk rather than
	// in a cache written back into the parsed document.
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

// The `input[type=checkbox]` a task item carries, or nullptr for an ordinary one. Attribute
// selectors do not exist in this CSS subset, so the state reaches a stylesheet as a class and as
// the checkbox's own `:checked` - never as the attribute it arrived in.
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

	// A rule is read as a break in the document, so it holds a place in the reading order even
	// though there is nothing in it to read. A row does not: its cells speak for it.
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

		// The marker is a sibling, not a pseudo-element: the CSS subset has no `::before`,
		// and a real node is what lets the indent be a layout column instead of arithmetic.
		if (auto box = MarkdownRegistry_taskBox(*ctx.source)) {
			// A task item's marker IS the checkbox, and it is disabled: a document is a document,
			// the state is what the source says, and a click has nowhere to write it back to.
			auto checkbox = node->addChild(Rc<Checkbox>::create());
			MarkdownBuilder::applyIdentity(checkbox, "li-checkbox");
			checkbox->addStyleClass("md-marker");

			// A size to start from. The widget draws a vector image and has no text to measure,
			// so with nothing assigned it has nothing to be; the sheet's `li-checkbox` rule
			// overrides this on the first style pass.
			checkbox->setContentSize(Size2(15.0f, 15.0f));
			LayoutSystem::setItem(checkbox,
					FlexItemInfo{.grow = 0.0f, .shrink = 0.0f, .basis = 15.0f});
			checkbox->setChecked(!box->getAttribute("checked").empty(), true);
			checkbox->setEnabled(false);
			node->addStyleClass("md-task");

			// The checkbox stands for the `[x]` the source holds, and carries its span: a copy
			// that starts on it starts on the marker.
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

			// The builder wrote this text, not the document: it is read, so it is in the flow,
			// and it maps to the item it marks rather than to any bytes of its own.
			builder->registerFlow(marker, MarkdownFlowKind::Marker, *ctx.source,
					uint32_t(marker->getString().size()));
		}

		auto content = node->addChild(Rc<Node>::create());
		MarkdownBuilder::applyIdentity(content, "li-content");

		// A tight item holds its text directly and a loose one wraps it in `p`; the builder's
		// implicit-run rule covers both without the factory knowing which it got.
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
		// `border-left` is consumed by table cells and nothing else in this engine, so the
		// quote bar is a node - stretched to the body's height by the row layout.
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
		// The document shape is `pre > code` for both fenced and indented blocks. The inner
		// node is where the language class sits, and it is the Label - so a `code` rule
		// reaches the text directly.
		const document::Node *code = nullptr;
		for (auto &it : ctx.source->getNodes()) {
			if (it->getHtmlName() == "code") {
				code = it;
				break;
			}
		}

		// The scroll lives on an inner node, not on the `pre` itself. Declaring one overflow
		// axis computes the other to `auto` (the only clip is a rect), and an overflowing
		// container is sized by its content on BOTH axes - so a scrolling `pre` would also
		// stop reporting its own height and scroll vertically inside a box too short for it.
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

	// A `code` node reached on its own (outside a `pre`) is inline markup, and inline markup is a
	// style range, never a node - the builder never dispatches it here. Registered as text so a
	// hand-built document with a block-level `code` still renders.
	ret->set("code", makeTextFactory());

	// --- table ----------------------------------------------------------------------------

	ret->set("table",
			MarkdownTagFactory{
				.create = [](const MarkdownBuilderContext &) -> Rc<Node> {
		return Rc<Node>(Rc<MarkdownPanel>::create(StringView("table")));
	},
				.buildContent = [](const MarkdownBuilderContext &ctx, Node *node) -> bool {
		// `display: table` expects the rows as direct children, so thead/tbody are flattened
		// here and each row keeps a class saying which section it came from - explicit
		// classes rather than :nth-child, which would stripe the header along with the body.
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

				auto rowNode =
						node->addChild(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.0f)));
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

				auto rowNode =
						node->addChild(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.0f)));
				MarkdownBuilder::applyIdentity(rowNode, "tr");
				rowNode->addStyleClass("md-tbody-row");
				ctx.builder->buildChildren(rowNode, *it);
			}
		}

		// The column count is per table, and CSS has no way to say it: a custom property is
		// the per-node channel, and the sheet reads it with var().
		if (columns > 0) {
			setStyleVariable(node, "--md-columns", toString("repeat(", columns, ", auto)"));
		}

		// The table layout publishes collapsed borders as geometry and draws nothing; this is
		// what turns them into a draw. Out of flow, or the table pass counts it as a row.
		auto painter = node->addChild(Rc<TableBorderPainter>::create(), ZOrder(10));
		painter->setComponent<OutOfFlowComponent>();
		return true;
	},
			});

	// --- out of scope for this milestone ---------------------------------------------------

	// An image is a later milestone. Registering it now as a nothing-node keeps a README with a
	// badge from losing the paragraph around it.
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
