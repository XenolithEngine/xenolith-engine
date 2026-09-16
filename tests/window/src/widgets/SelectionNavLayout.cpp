/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "widgets/SelectionNavLayout.h"
#include "XLAction.h"
#include "XL2dLayer.h"
#include "XLFocusGroup.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

static constexpr float CellWidth = 80.0f;
static constexpr float CellHeight = 40.0f;
static constexpr float CellPitchX = 100.0f;
static constexpr float CellPitchY = 60.0f;
static constexpr float Margin = 24.0f;
static constexpr float TreeRowHeight = 24.0f;

static bool parseDirection(StringView name, SelectionDirection &out) {
	if (name == "left") {
		out = SelectionDirection::Left;
	} else if (name == "right") {
		out = SelectionDirection::Right;
	} else if (name == "up") {
		out = SelectionDirection::Up;
	} else if (name == "down") {
		out = SelectionDirection::Down;
	} else {
		return false;
	}
	return true;
}

} // namespace

bool SelectionNavLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(R"(
		tree-view { background-color: #fafafa; }
		tree-row { background-color: transparent; height: var(--tree-row-h); }
		tree-row:selected { background-color: #bbdefb; }
		#tree-rtl { direction: rtl; }
	)");

	addSystem(Rc<ui::StyleResolver>::create(true));

	for (size_t r = 0; r < 3; ++r) {
		for (size_t c = 0; c < 3; ++c) {
			auto cell = addChild(Rc<basic2d::Layer>::create(Color::Grey_300));
			cell->setName(toString("cell-", r, "-", c));
			cell->setAnchorPoint(Anchor::TopLeft);
			setNodeSelectable(cell, true);
			_grid[r][c] = cell;
		}
	}

	// Registered, but not drawn: absent from the hit-test registry, so never a candidate
	_hidden = addChild(Rc<basic2d::Layer>::create(Color::Amber_200));
	_hidden->setName("cell-hidden");
	_hidden->setAnchorPoint(Anchor::TopLeft);
	_hidden->setVisible(false);
	setNodeSelectable(_hidden, true);

	auto makeTree = [&](StringView name, size_t items) {
		auto tree = addChild(Rc<ui::TreeView>::create(makeModel(name, items)));
		tree->setName(name);
		tree->setLabelKey("title");
		tree->setRowHeight(TreeRowHeight);
		tree->setAnchorPoint(Anchor::TopLeft);
		tree->setSelectCallback([](size_t, const ui::TreeView::Row &) { });
		tree->setSelectionOwned(true);
		return tree;
	};

	_tree = makeTree("tree", 6);
	_treeRtl = makeTree("tree-rtl", 2);

	// Takes the arrows through the ordinary key route while `_eat` is set, as a focused slider does
	{
		_eater = addChild(Rc<Node>::create());
		_eater->setName("eater");

		InputKeyMask keys;
		keys.set(toInt(InputKeyCode::LEFT));
		keys.set(toInt(InputKeyCode::RIGHT));
		keys.set(toInt(InputKeyCode::UP));
		keys.set(toInt(InputKeyCode::DOWN));

		auto listener = _eater->addSystem(Rc<InputListener>::create());
		listener->addKeyRecognizer([this](const GestureData &data) {
			if (data.event == GestureEvent::Began) {
				++_eaten;
			}
			return true;
		}, InputKeyInfo{sp::move(keys)});
		listener->setTouchFilter(
				[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
			if (event.data.isKeyEvent()) {
				return _eat;
			}
			return cb(event);
		});
	}

	// A modal scope: while visible, its Exclusive group keeps keys away from everything outside
	{
		_modal = addChild(Rc<Node>::create());
		_modal->setName("modal");
		_modal->setVisible(false);

		auto group = _modal->addSystem(Rc<FocusGroup>::create());
		group->setEventMask(FocusGroup::EventMask(EventMaskKeyboard));
		group->setFlags(FocusGroup::Flags::Exclusive);

		auto member = _modal->addChild(Rc<Node>::create());
		member->addSystem(Rc<InputListener>::create());
	}

	_box = addChild(Rc<Node>::create());
	_box->setName("box");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runChecks(); }));

	return true;
}

void SelectionNavLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 20.0f;

	for (size_t r = 0; r < 3; ++r) {
		for (size_t c = 0; c < 3; ++c) {
			_grid[r][c]->setPosition(
					Vec2(Margin + float(c) * CellPitchX, top - float(r) * CellPitchY));
			_grid[r][c]->setContentSize(Size2(CellWidth, CellHeight));
		}
	}

	_hidden->setPosition(Vec2(Margin, top - 3.0f * CellPitchY));
	_hidden->setContentSize(Size2(CellWidth, CellHeight));

	const float treeX = Margin + 3.0f * CellPitchX + 40.0f;

	_tree->setPosition(Vec2(treeX, top));
	_tree->setContentSize(Size2(200.0f, TreeRowHeight * 5.0f));

	_treeRtl->setPosition(Vec2(treeX, top - 240.0f));
	_treeRtl->setContentSize(Size2(200.0f, TreeRowHeight * 4.0f));
}

Rc<data::Model> SelectionNavLayout::makeModel(StringView prefix, size_t items) const {
	data::Model::Value root;
	root.setString(prefix, "title");
	auto model = Rc<data::Model>::create(sp::move(root));

	for (auto name : {StringView("cat-a"), StringView("cat-b")}) {
		data::Model::Value own;
		own.setString(name, "title");
		auto node = model->emplaceCategory(model->getRoot(), maxOf<size_t>(), sp::move(own));
		for (size_t i = 0; i < items; ++i) {
			data::Model::Value item;
			item.setString(toString(name, "-", i), "title");
			model->emplaceItem(node, maxOf<size_t>(), sp::move(item));
		}
	}
	return model;
}

void SelectionNavLayout::expect(bool cond, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("SelectionNavTest", what);
	} else {
		log::source().info("SelectionNavTest", "  ok   ", what);
	}
}

void SelectionNavLayout::runChecks() {
	const Rect from(100.0f, 100.0f, 50.0f, 50.0f);
	double score = 0.0;

	expect(getSelectionDirectionScore(SelectionDirection::Right, from, Rect(160, 100, 50, 50), score),
			"a rect to the right scores for Right");
	expect(!getSelectionDirectionScore(SelectionDirection::Left, from, Rect(160, 100, 50, 50),
				   score),
			"a rect to the right does not score for Left");
	expect(getSelectionDirectionScore(SelectionDirection::Up, from, Rect(100, 160, 50, 50), score),
			"Up is towards larger y");
	expect(!getSelectionDirectionScore(SelectionDirection::Down, from, Rect(120, 120, 50, 50),
				   score),
			"an overlapping rect is in no direction");

	double inBeam = 0.0;
	double outOfBeam = 0.0;
	getSelectionDirectionScore(SelectionDirection::Right, from, Rect(400, 100, 50, 50), inBeam);
	getSelectionDirectionScore(SelectionDirection::Right, from, Rect(160, 300, 50, 50), outOfBeam);
	expect(inBeam < outOfBeam, "a far rect in the beam beats a near one outside it");

	auto system = SelectionSystem::acquireForNode(this);
	expect(system != nullptr, "the scene has a selection system");
	if (system) {
		system->clear();
		expect(!system->moveSelection(SelectionDirection::Down), "an empty selection does not move");
	}

	log::source().info("SelectionNavTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

ui::TreeView *SelectionNavLayout::findTree(StringView name) const {
	if (name == "tree") {
		return _tree;
	} else if (name == "tree-rtl") {
		return _treeRtl;
	}
	return nullptr;
}

Value SelectionNavLayout::encodeTree(ui::TreeView *tree) const {
	Value ret;
	const auto index = tree->getSelectedRow();
	if (auto row = tree->getRow(index)) {
		ret.setInteger(int64_t(index), "selected");
		ret.setString(row->getData().getString("title"), "label");
		ret.setBool(row->expanded, "expanded");
	}
	ret.setInteger(int64_t(tree->getRowCount()), "rows");
	ret.setDouble(tree->getScroll() ? tree->getScroll()->getScrollPosition() : 0.0, "scroll");
	return ret;
}

Value SelectionNavLayout::encodeState() const {
	Value ret;
	auto system = SelectionSystem::findForNode(const_cast<SelectionNavLayout *>(this));
	ret.setBool(system != nullptr, "ready");
	if (system) {
		ret.setString(system->getOwnerNode() ? system->getOwnerNode()->getName() : StringView(),
				"owner");
		auto anchor = system->getAnchorNode();
		ret.setString(anchor ? anchor->getName() : StringView(), "anchor");
		ret.setString(anchor ? anchor->getType() : StringView(), "anchorType");
		ret.setInteger(int64_t(system->getItems().size()), "count");

		Value::ArrayType chain;
		for (auto &node : system->getChain()) { chain.emplace_back(Value(node->getName())); }
		ret.emplace("chain").setArray(sp::move(chain));
	}
	ret.setValue(encodeTree(_tree), "tree");
	ret.setValue(encodeTree(_treeRtl), "tree-rtl");
	ret.setInteger(int64_t(_eaten), "eaten");
	return ret;
}

void SelectionNavLayout::registerCommands() {
	addCommand("state", "Report the selection, both trees and the arrow listener's count",
			[this](Value &&) { return encodeState(); });

	addCommand("select", "Select {name} (a cell), {tree, row}, or clear with neither",
			[this](Value &&args) {
		const Value &req = args;
		auto system = SelectionSystem::acquireForNode(this);
		if (!system) {
			return ackValue(false);
		}
		if (auto tree = findTree(req.getString("tree"))) {
			tree->setSelectedRow(size_t(req.getInteger("row")));
			return ackValue(true);
		}
		auto name = req.getString("name");
		for (auto node : getChildren()) {
			if (node->getName() == name && getNodeSelectable(node)) {
				return ackValue(system->selectNode(node));
			}
		}
		return ackValue(system->clear());
	});

	addCommand("move", "SelectionSystem::moveSelection: {dir: left|right|up|down}",
			[this](Value &&args) {
		const Value &req = args;
		SelectionDirection dir;
		auto system = SelectionSystem::findForNode(this);
		if (!system || !parseDirection(req.getString("dir"), dir)) {
			return ackValue(false);
		}
		return ackValue(system->moveSelection(dir));
	});

	addCommand("eat", "Let the ordinary key route take the arrows: {value}", [this](Value &&args) {
		const Value &req = args;
		_eat = req.getBool("value");
		return ackValue(true);
	});

	addCommand("modal", "Show or hide the Exclusive focus group: {value}", [this](Value &&args) {
		const Value &req = args;
		_modal->setVisible(req.getBool("value"));
		return ackValue(true);
	});

	addCommand("reparent", "Move cell-2-2 into the box and back: {value}", [this](Value &&args) {
		const Value &req = args;
		Rc<Node> cell = _grid[2][2];
		Node *target = req.getBool("value") ? _box : this;
		if (cell->getParent() == target) {
			return ackValue(false);
		}
		cell->removeFromParent(false);
		target->addChild(cell);
		return ackValue(true);
	});

	addCommand("hidden", "Show or hide the selectable node under the grid: {value}",
			[this](Value &&args) {
		const Value &req = args;
		_hidden->setVisible(req.getBool("value"));
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
