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

#include "dock/DockFocusLayout.h"
#include "XLUiDockFrame.h"
#include "XLUiPanel.h"
#include "XLUiStyleResolver.h"
#include "XLSelectionSystem.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float RootWidth = 800.0f;
static constexpr float RootHeight = 400.0f;
static constexpr float RowHeight = 24.0f;
static constexpr size_t RowCount = 4;

static constexpr auto s_css = StringView(R"css(
dock-frame { background-color: #232323; }
dock-tab-bar { background-color: #171717; }
dock-tab { padding: 4px 10px; background-color: #2b2b2b; }
dock-tab > label { color: #c8c8c8; font-size: 13px; }
dock-splitter { background-color: #2a2a2a; }
tree-view { background-color: #303030; }
tree-row { background-color: transparent; height: 24px; }
tree-row:selected { background-color: #5c4a10; }
dock-frame-outline.current { outline-width: 2px; outline-style: solid; outline-color: #fcb400; }
)css");

static Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

static void setPoint(Value &out, StringView key, const Vec2 &world) {
	auto &v = out.emplace(key);
	v.addDouble(world.x);
	v.addDouble(world.y);
}

} // namespace

bool DockFocusLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_root = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(1));
	_root->setAnchorPoint(Anchor::BottomLeft);
	_root->setContentSize(Size2(RootWidth, RootHeight));

	_outer = _root->addSystem(Rc<ui::DockSystem>::create());
	_outer->setFramesSelectable(true);

	ui::DockPanelDescriptor treeDesc;
	treeDesc.id = String("tree");
	treeDesc.title = String("tree");
	treeDesc.builder = [this]() -> Rc<Node> {
		auto tree = Rc<ui::TreeView>::create(makeModel());
		tree->setName("tree");
		tree->setLabelKey("title");
		tree->setRowHeight(RowHeight);
		tree->setSelectCallback([this](size_t, const ui::TreeView::Row &) {
			++_selects;
			if (_tree && _tree->isSelectingFromKeyboard()) {
				++_keyboardSelects;
			}
		});
		tree->setActivateCallback([this](size_t, const ui::TreeView::Row &) { ++_activations; });
		tree->setSelectionOwned(true);
		_tree = tree;
		return tree;
	};
	_outer->registerPanel(sp::move(treeDesc));

	ui::DockPanelDescriptor innerDesc;
	innerDesc.id = String("inner");
	innerDesc.title = String("inner");
	innerDesc.builder = [this]() -> Rc<Node> {
		auto node = Rc<Node>::create();
		node->setName("inner-root");
		node->setAnchorPoint(Anchor::BottomLeft);
		_inner = node->addSystem(Rc<ui::DockSystem>::create());
		_inner->setFramesSelectable(true);

		ui::DockPanelDescriptor canvasDesc;
		canvasDesc.id = String("canvas");
		canvasDesc.title = String("canvas");
		canvasDesc.builder = [this]() -> Rc<Node> {
			auto canvas = Rc<ui::Panel>::create();
			canvas->setPathColor(Color::Teal_700, true);
			canvas->setName("canvas");
			setNodeSelectable(canvas, true);
			_canvas = canvas;
			return canvas;
		};
		_inner->registerPanel(sp::move(canvasDesc));

		ui::DockPanelDescriptor plainDesc;
		plainDesc.id = String("plain");
		plainDesc.title = String("plain");
		plainDesc.builder = [this]() -> Rc<Node> {
			auto plain = Rc<ui::Panel>::create();
			plain->setPathColor(Color::Indigo_700, true);
			plain->setName("plain");
			auto input = plain->addChild(Rc<ui::TextInput>::create(), ZOrder(1));
			input->setName("input");
			input->setAnchorPoint(Anchor::BottomLeft);
			input->setPosition(Vec2(8.0f, 8.0f));
			input->setContentSize(Size2(160.0f, 28.0f));
			_input = input;
			_plain = plain;
			return plain;
		};
		_inner->registerPanel(sp::move(plainDesc));

		using Spec = ui::DockLayoutSpec;
		_inner->setLayout(Spec::vsplit(0.5f,
				Spec::leaf({String("canvas")}, {.name = String("inner-top")}),
				Spec::leaf({String("plain")}, {.name = String("inner-bottom")})));
		return node;
	};
	_outer->registerPanel(sp::move(innerDesc));

	using Spec = ui::DockLayoutSpec;
	_outer->setLayout(Spec::hsplit(0.4f, Spec::leaf({String("tree")}, {.name = String("outer-left")}),
			Spec::leaf({String("inner")}, {.name = String("outer-right")})));

	return true;
}

void DockFocusLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);
	if (auto system = SelectionSystem::acquireForNode(this)) {
		system->setSelectOnPress(true);
	}
}

void DockFocusLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_root->setPosition(Vec2(40.0f, getWorkTop() - 40.0f - RootHeight));
}

Rc<data::Model> DockFocusLayout::makeModel() const {
	data::Model::Value root;
	root.setString("root", "title");
	auto model = Rc<data::Model>::create(sp::move(root));
	for (size_t i = 0; i < RowCount; ++i) {
		data::Model::Value item;
		item.setString(toString("row-", i), "title");
		model->emplaceItem(model->getRoot(), maxOf<size_t>(), sp::move(item));
	}
	return model;
}

Value DockFocusLayout::encodeFrame(ui::DockSystem *dock, StringView name) const {
	Value ret;
	auto frame = dock ? dock->getFrameNode(dock->findFrameByName(name)) : nullptr;
	if (!frame) {
		return ret;
	}
	ret.setBool(frame->isCurrent(), "current");
	if (auto outline = frame->getOutline()) {
		ret.setBool(outline->isVisible(), "outlineVisible");
		auto style = outline->getComponent<ui::PanelStyleComponent>();
		ret.setDouble(style ? style->outlineWidth : 0.0f, "outlineWidth");
		ret.setInteger(outline->getPathColor().a, "outlineFillAlpha");
		ret.setBool(outline->getContentSize() == frame->getContentSize(), "outlineFits");
		ret.setBool(outline->getLocalZOrder() > frame->getBody()->getLocalZOrder(), "outlineAbove");
	}
	return ret;
}

Value DockFocusLayout::encodeState() const {
	Value ret;
	auto system = SelectionSystem::findForNode(const_cast<DockFocusLayout *>(this));
	if (system) {
		auto owner = system->getOwnerNode();
		ret.setString(owner ? owner->getName() : StringView(), "owner");
		auto anchor = system->getAnchorNode();
		ret.setString(anchor ? anchor->getName() : StringView(), "anchor");
		ret.setBool(system->isSelectOnPress(), "selectOnPress");
	}

	auto outerCurrent = _outer ? _outer->getCurrentFrame() : nullptr;
	auto innerCurrent = _inner ? _inner->getCurrentFrame() : nullptr;
	ret.setString(outerCurrent ? outerCurrent->getName() : StringView(), "outerCurrent");
	ret.setString(innerCurrent ? innerCurrent->getName() : StringView(), "innerCurrent");

	for (auto name : {StringView("outer-left"), StringView("outer-right")}) {
		ret.setValue(encodeFrame(_outer, name), name);
	}
	for (auto name : {StringView("inner-top"), StringView("inner-bottom")}) {
		ret.setValue(encodeFrame(_inner, name), name);
	}

	if (_tree) {
		ret.setInteger(_tree->getSelectedRow() < _tree->getRowCount()
						? int64_t(_tree->getSelectedRow())
						: int64_t(-1),
				"row");
	}
	ret.setInteger(int64_t(_selects), "selects");
	ret.setInteger(int64_t(_keyboardSelects), "keyboardSelects");
	ret.setInteger(int64_t(_activations), "activations");
	ret.setBool(_input && _input->isFocused(), "inputFocused");
	return ret;
}

Value DockFocusLayout::encodeProbe() const {
	Value ret;
	if (_tree) {
		const auto size = _tree->getContentSize();
		for (size_t i = 0; i < RowCount; ++i) {
			setPoint(ret, toString("row", i),
					_tree->convertToWorldSpace(
							Vec2(size.width / 2.0f, size.height - (float(i) + 0.5f) * RowHeight)));
		}
		setPoint(ret, "treeEmpty",
				_tree->convertToWorldSpace(Vec2(size.width / 2.0f, RowHeight)));
	}
	for (auto node : {_canvas, _plain}) {
		if (node) {
			const auto size = node->getContentSize();
			setPoint(ret, node->getName(),
					node->convertToWorldSpace(Vec2(size.width / 2.0f, size.height / 2.0f)));
		}
	}
	return ret;
}

void DockFocusLayout::registerCommands() {
	addCommand("state", "The selection, the current frame of both docks and the tree's counters",
			[this](Value &&) { return encodeState(); });

	addCommand("probe", "World points of the tree rows, the tree's empty area and both panels",
			[this](Value &&) { return encodeProbe(); });

	addCommand("press-select", "SelectionSystem::setSelectOnPress: {value}", [this](Value &&args) {
		const Value &req = args;
		auto system = SelectionSystem::findForNode(this);
		if (!system) {
			return ackValue(false);
		}
		system->setSelectOnPress(req.getBool("value"));
		return ackValue(true);
	});

	addCommand("focus-input", "Focus the text input in the plain panel, or blur it: {value}",
			[this](Value &&args) {
		const Value &req = args;
		if (!_input) {
			return ackValue(false);
		}
		if (req.getBool("value")) {
			_input->focus();
		} else {
			_input->blur();
		}
		return ackValue(true);
	});

	addCommand("clear", "Drop the selection", [this](Value &&) {
		auto system = SelectionSystem::findForNode(this);
		return ackValue(system && system->clear());
	});
}

} // namespace stappler::xenolith::app
