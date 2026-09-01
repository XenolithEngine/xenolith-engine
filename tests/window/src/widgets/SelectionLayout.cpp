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

#include "widgets/SelectionLayout.h"
#include "XLAction.h"
#include "XL2dLayer.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

// Registered by the stand rather than reused from the engine, so a rebind here cannot disturb
// anything else in the app
static constexpr auto ChainKeyName = StringView("org.stappler.test.selection.chain");
static constexpr auto SelKeyName = StringView("org.stappler.test.selection.selected-only");

static constexpr float RowHeight = 28.0f;
static constexpr float ListWidth = 220.0f;

} // namespace

// --- SelectionListOwner ----------------------------------------------------

bool SelectionListOwner::init(StringView name, size_t count) {
	if (!Node::init()) {
		return false;
	}

	setName(name);
	setType("list");

	_items.reserve(count);
	_rows.resize(count);

	for (size_t i = 0; i < count; ++i) {
		_items.emplace_back(Rc<SelectionItemRef>::create(i));
	}

	// Everything starts materialized; a phase drops one to make the virtualized case happen
	for (size_t i = 0; i < count; ++i) { setMaterialized(i, true); }

	return true;
}

SelectionItem SelectionListOwner::getItem(size_t index) const {
	if (index >= _items.size()) {
		return SelectionItem();
	}
	return SelectionItem{_items[index].get(), 0};
}

Node *SelectionListOwner::resolveSelectionNode(const SelectionItem &item) const {
	for (size_t i = 0; i < _items.size(); ++i) {
		if (_items[i].get() == item.ref.get()) {
			return _rows[i].get();
		}
	}
	return nullptr;
}

void SelectionListOwner::handleSelectionChanged(SpanView<SelectionItem> items) {
	_lastNotified = Vector<SelectionItem>(items.begin(), items.end());
	++_notifyCount;

	if (_hook) {
		_hook(items);
	}
}

void SelectionListOwner::resetNotifications() {
	_lastNotified.clear();
	_notifyCount = 0;
}

bool SelectionListOwner::isMaterialized(size_t index) const {
	return index < _rows.size() && _rows[index];
}

Node *SelectionListOwner::getRowNode(size_t index) const {
	return index < _rows.size() ? _rows[index].get() : nullptr;
}

void SelectionListOwner::setMaterialized(size_t index, bool value) {
	if (index >= _rows.size()) {
		return;
	}

	if (value) {
		if (_rows[index]) {
			return;
		}
		auto row = addChild(Rc<basic2d::Layer>::create(Color::Grey_200));
		row->setName(toString(getName(), "-row-", index));
		row->setType("row");
		row->setAnchorPoint(Anchor::TopLeft);
		_rows[index] = row;

		// A row exists only while it is materialized, so whatever has to live on one has to be put
		// there now - which is exactly the lifetime a real virtualized list gives its rows
		if (_rowBuilt) {
			_rowBuilt(row, index);
		}
	} else {
		if (auto row = _rows[index]) {
			// Just the node. The identity stays, and so does the selection on it - which is the
			// whole case: a list that recycles a row must not lose what the user picked
			row->removeFromParent();
			_rows[index] = nullptr;
		}
	}
}

// --- SelectionLayout -------------------------------------------------------

bool SelectionLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(R"(
		list { background-color: #fafafa; }
		row  { background-color: #eeeeee; }
		row:selected { background-color: #094771; }
		list:selection-within { background-color: #e3f2fd; }
	)");

	_root = addChild(Rc<Node>::create());
	_root->setName("root");

	_listA = _root->addChild(Rc<SelectionListOwner>::create("list-a", 4));
	_listB = _root->addChild(Rc<SelectionListOwner>::create("list-b", 3));

	_plain = _root->addChild(Rc<basic2d::Layer>::create(Color::Amber_200));
	_plain->setName("plain");
	_plain->setType("plain");
	_plain->setAnchorPoint(Anchor::TopLeft);

	{
		auto reg = HotkeyRegistry::getInstance();
		_chainKey = reg->add(ChainKeyName, HotkeyCombo::parse("Ctrl+J"),
				"Offered to everyone, along the chain and then the ordinary walk");
		_selKey = reg->add(SelKeyName, HotkeyCombo::parse("Ctrl+H"),
				"SelectedOnly: offered only on the chain");
	}

	/* The tree: two categories, each with three items, both collapsed at the start.

	Category A comes FIRST, so expanding it pushes every row of category B down - which is exactly
	the mutation that used to move the selection onto a different element without a word. */
	{
		data::Model::Value root;
		root.setString("root", "title");
		_model = Rc<data::Model>::create(sp::move(root));

		auto makeCategory = [&](StringView title) {
			data::Model::Value own;
			own.setString(title, "title");
			auto node = _model->emplaceCategory(_model->getRoot(), maxOf<size_t>(), sp::move(own));
			for (size_t i = 0; i < 3; ++i) {
				data::Model::Value item;
				item.setString(toString(title, "-item-", i), "title");
				_model->emplaceItem(node, maxOf<size_t>(), sp::move(item));
			}
			return node;
		};

		_categoryA = makeCategory("cat-a");
		_categoryB = makeCategory("cat-b");
	}

	addSystem(Rc<ui::StyleResolver>::create(true));

	_tree = _root->addChild(Rc<ui::TreeView>::create(_model));
	_tree->setName("tree");
	_tree->setLabelKey("title");
	_tree->setRowHeight(24.0f);
	_tree->setSelectCallback([](size_t, const ui::TreeView::Row &) { });

	// The opt-in. Without it the tree keeps a private index, exactly as ui::SearchPicker's own list
	// does, and the scene's selection is none of its business
	_tree->setSelectionOwned(true);

	addSubscriber("tree", _tree, HotkeyFlags::None);

	/* The opt-in focus coupling, in the only form the engine offers: the widget makes the call.

	Note what is NOT here - nothing watches FocusGroup, nothing reconciles the two after the fact,
	and nothing runs at commit time. The engine-enforced version of this cannot be written:
	commitStorage commits EVERY FocusGroup in a frame, so there is no single "the focus" for a
	selection to contain; ui::FormSystem answers `true` for every listener while nothing is focused,
	so "focus is outside the selection" is not even decidable; and the focus swap is deferred past
	the visit, so a selection reacting to it would answer one commit late, every time. */
	{
		auto host = _root->addChild(Rc<Node>::create());
		host->setName("focus-host");
		_focusGroup = host->addSystem(Rc<FocusGroup>::create());
		_focusGroup->setEventMask(FocusGroup::EventMask(EventMaskKeyboard));
		_focusGroup->setFlags(FocusGroup::Flags::SingleFocus);

		_focusNode = host->addChild(Rc<basic2d::Layer>::create(Color::Teal_200));
		_focusNode->setName("focus-node");
		_focusNode->setAnchorPoint(Anchor::TopLeft);

		_focusListener = _focusNode->addSystem(Rc<InputListener>::create());
		_focusListener->setFocusCallback([this](bool focused) {
			if (!focused) {
				// Deliberately asymmetric: LOSING focus does not clear the selection. Dropping
				// focus opens ui::FormSystem's gate for every form listener, so treating it as
				// "nothing is current any more" would clear the selection whenever a field was
				// merely blurred
				return;
			}
			++_focusSelects;
			if (auto system = SelectionSystem::findForNode(_focusNode)) {
				system->selectNode(_focusNode);
			}
		});

		addSubscriber("focus-node", _focusNode, HotkeyFlags::None);

		// A second member of the same SingleFocus group. Moving focus means naming where it goes:
		// a group with listeners in it always has a focused one, so there is no "nobody" to pass
		auto other = host->addChild(Rc<Node>::create());
		other->setName("focus-other");
		_focusOther = other->addSystem(Rc<InputListener>::create());
	}

	/* Subscribers at four different depths, plus one that is never on the chain at all.

	The rows are handled through the owner's build hook because a row only exists while it is
	materialized - which is also the interesting case: the deepest link of the chain comes and goes
	while the selection stands still. */
	addSubscriber("layout", this, HotkeyFlags::None);
	addSubscriber("root", _root, HotkeyFlags::None);
	addSubscriber("list-a", _listA, HotkeyFlags::None);
	addSubscriber("list-b", _listB, HotkeyFlags::None);
	addSubscriber("plain", _plain, HotkeyFlags::None);

	_listA->setRowBuiltCallback([this](Node *row, size_t index) {
		addSubscriber(toString("row-", index), row, HotkeyFlags::None);
	});

	// The rows built by the constructor above ran before the callback was set, so give list-a its
	// subscribers now that there is one
	for (size_t i = 0; i < _listA->getCount(); ++i) {
		if (auto row = _listA->getRowNode(i)) {
			addSubscriber(toString("row-", i), row, HotkeyFlags::None);
		}
	}

	/* Count component-dirty events on the two nodes a within-list move must NOT disturb.
	
	This is the only check in the stand that can tell a correct implementation from one that
	releases the old chain before retaining the new: both end up with the same components in the
	same places, and only the transient matters. A blink shows up here as a dirty event and
	nowhere else. */
	for (auto node : {_root, static_cast<Node *>(_listA)}) {
		_dirty.emplace(node, size_t(0));
		node->setComponentsDirtyCallback([this, node](const ComponentMask &mask) {
			if (mask.find(SelectionComponent::Id.value) != mask.end()) {
				++_dirty[node];
			}
		});
	}

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase5(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase6(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase7(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase8(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase9(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase10(); },
			Rc<DelayTime>::create(0.4f), [this] { runPhase11(); }, Rc<DelayTime>::create(0.4f),
			[this] { runPhase12(); }));

	return true;
}

void SelectionLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	auto top = getWorkTop();

	_root->setPosition(Vec2(0.0f, 0.0f));
	_root->setContentSize(getWorkSize());

	auto placeList = [&](SelectionListOwner *list, float x) {
		list->setAnchorPoint(Anchor::TopLeft);
		list->setPosition(Vec2(x, top));
		list->setContentSize(Size2(ListWidth, RowHeight * float(list->getCount())));

		float y = 0.0f;
		for (size_t i = 0; i < list->getCount(); ++i) {
			if (auto row = list->getRowNode(i)) {
				row->setPosition(Vec2(0.0f, list->getContentSize().height - y));
				row->setContentSize(Size2(ListWidth, RowHeight - 2.0f));
			}
			y += RowHeight;
		}
	};

	placeList(_listA, 24.0f);
	placeList(_listB, 24.0f + ListWidth + 24.0f);

	_plain->setPosition(Vec2(24.0f + (ListWidth + 24.0f) * 2.0f, top));
	_plain->setContentSize(Size2(ListWidth, RowHeight * 2.0f));

	if (_focusNode) {
		_focusNode->setPosition(Vec2(24.0f, top - RowHeight * 6.0f));
		_focusNode->setContentSize(Size2(ListWidth, RowHeight));
	}

	_tree->setAnchorPoint(Anchor::TopLeft);
	_tree->setPosition(Vec2(24.0f + (ListWidth + 24.0f) * 3.0f, top));
	_tree->setContentSize(Size2(ListWidth, 260.0f));
}

void SelectionLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("SelectionTest", phase, ": ", what);
	} else {
		log::source().info("SelectionTest", "  ok   ", what);
	}
}

size_t SelectionLayout::countSelectionWithin(Node *node) const {
	size_t count = 0;
	for (auto p = node; p != nullptr; p = p->getParent()) {
		if (hasSelectionWithin(p)) {
			++count;
		}
	}
	return count;
}

SelectionListOwner *SelectionLayout::findOwner(StringView name) const {
	if (name == "list-a") {
		return _listA;
	}
	if (name == "list-b") {
		return _listB;
	}
	return nullptr;
}

void SelectionLayout::addSubscriber(StringView name, Node *owner, HotkeyFlags flags) {
	auto key = name.str<Interface>();
	_consume.emplace(key, false);

	auto listener = owner->addSystem(Rc<InputListener>::create());

	auto handler = [this, key](HotkeyId, const InputEvent &) {
		_hotkeyLog.emplace_back(key);

		// Declining by default is what makes the ORDER observable: a consuming subscriber would
		// stop the walk at itself and the log would only ever have one entry in it
		auto it = _consume.find(key);
		return it != _consume.end() && it->second;
	};

	listener->addHotkey(_chainKey, HotkeyCallback(handler), flags);
	listener->addHotkey(_selKey, HotkeyCallback(handler), flags | HotkeyFlags::SelectedOnly);
}

// --- phases ----------------------------------------------------------------

void SelectionLayout::runPhase1() {
	// acquireForNode installs the system on the SceneContent, so this is also the assertion that a
	// widget can select without the application having arranged anything
	_selection = SelectionSystem::acquireForNode(this);

	expect(_selection != nullptr, "phase1", "no selection system could be acquired");
	if (!_selection) {
		return;
	}

	expect(SelectionSystem::findForNode(_listA->getRowNode(0)) == _selection, "phase1",
			"a row deep in the tree does not find the scene's system");

	expect(_selection->empty(), "phase1", "the scene started with something selected");
	expect(_selection->getAnchorNode() == nullptr, "phase1", "an empty selection has an anchor");
	expect(!isNodeSelected(_listA->getRowNode(0)), "phase1", "a row started out :selected");
	expect(!hasSelectionWithin(_listA), "phase1", "a list started out :selection-within");
}

void SelectionLayout::runPhase2() {
	auto item = _listA->getItem(1);
	auto row = _listA->getRowNode(1);

	_listA->resetNotifications();
	expect(_selection->select(_listA, makeSpanView(&item, 1)), "phase2", "the selection was refused");

	expect(_selection->getOwner() == _listA, "phase2", "the wrong owner holds the selection");
	expect(_selection->getOwnerNode() == _listA, "phase2", "the owner node is wrong");
	expect(_selection->getItems().size() == 1, "phase2", "the selection has the wrong size");
	expect(_selection->isSelected(item), "phase2", "the selected item does not read as selected");

	expect(_selection->getAnchorNode() == row, "phase2",
			"a single materialized item did not become the anchor");

	expect(isNodeSelected(row), "phase2", "the selected row is not :selected");
	expect(!isNodeSelected(_listA->getRowNode(0)), "phase2", "an unselected row is :selected");

	// The row itself, the list, _root, this layout, and on up through the scene content. What
	// matters is that it does not stop at the widget: a stylesheet may put the rule anywhere above
	expect(hasSelectionWithin(row), "phase2", "the selected row is not :selection-within");
	expect(hasSelectionWithin(_listA), "phase2", "the owning list is not :selection-within");
	expect(hasSelectionWithin(_root), "phase2", "a shared ancestor is not :selection-within");
	expect(!hasSelectionWithin(_listB), "phase2", "an unrelated list is :selection-within");

	expect(_listA->getNotifyCount() == 1, "phase2", "the owner was not told exactly once");
	expect(_listA->getLastNotified().size() == 1, "phase2", "the owner was told the wrong items");

	// Selecting exactly what is already selected must be a no-op, not a second notification. This
	// is what lets a widget call select() on every tap without checking first - and it is also the
	// equality that makes the reentrancy guard terminate rather than merely detect
	_listA->resetNotifications();
	expect(!_selection->select(_listA, makeSpanView(&item, 1)), "phase2",
			"re-selecting the same item reported a change");
	expect(_listA->getNotifyCount() == 0, "phase2",
			"re-selecting the same item notified the owner again");
}

void SelectionLayout::runPhase3() {
	// The counters as they stand with the selection on row 1; everything below is measured against
	// this, so the move itself is the only thing that can move them
	const auto rootDirty = _dirty[_root];
	const auto listDirty = _dirty[static_cast<Node *>(_listA)];

	auto prevRow = _listA->getRowNode(1);
	auto item = _listA->getItem(2);
	auto row = _listA->getRowNode(2);

	expect(_selection->select(_listA, makeSpanView(&item, 1)), "phase3", "the move was refused");

	expect(isNodeSelected(row), "phase3", "the new row is not :selected");
	expect(!isNodeSelected(prevRow), "phase3", "the old row is still :selected");
	expect(!hasSelectionWithin(prevRow), "phase3", "the old row is still :selection-within");
	expect(_selection->getAnchorNode() == row, "phase3", "the anchor did not move");

	/* The property a retain-after-release implementation fails and nothing else catches: the two
	chains share every ancestor above the rows, so those ancestors must go 1 -> 2 -> 1 without ever
	losing the component. A blink would restyle both of them and everything under them. */
	expect(_dirty[_root] == rootDirty, "phase3",
			"a shared ancestor was restyled by a move that never left it");
	expect(_dirty[static_cast<Node *>(_listA)] == listDirty, "phase3",
			"the owning list was restyled by a move within itself");

	// A multi-item selection anchors on the CONTAINER. Anchoring on one of the items would offer
	// that row's own listener a hotkey the other selected rows never see
	SelectionItem pair[2] = {_listA->getItem(2), _listA->getItem(3)};
	expect(_selection->select(_listA, makeSpanView(pair, 2)), "phase3",
			"a multi-item selection was refused");
	expect(_selection->getItems().size() == 2, "phase3", "the multi-item selection lost an item");
	expect(_selection->getAnchorNode() == _listA, "phase3",
			"a multi-item selection did not anchor on its container");
	expect(isNodeSelected(_listA->getRowNode(2)) && isNodeSelected(_listA->getRowNode(3)), "phase3",
			"a multi-item selection did not mark both rows");
}

void SelectionLayout::runPhase4() {
	auto item = _listB->getItem(0);

	_listA->resetNotifications();
	_listB->resetNotifications();

	expect(_selection->select(_listB, makeSpanView(&item, 1)), "phase4",
			"selecting into the second list was refused");

	expect(_selection->getOwner() == _listB, "phase4", "the second list did not take the selection");
	expect(_selection->getItems().size() == 1, "phase4",
			"the previous owner's items survived the move");

	// One selection, scene-wide. The first list must be told, or a view would keep painting a
	// highlight for a selection it no longer holds
	expect(_listA->getNotifyCount() == 1, "phase4", "the losing owner was not told");
	expect(_listA->getLastNotified().empty(), "phase4",
			"the losing owner was told it still has items");
	expect(_listB->getNotifyCount() == 1, "phase4", "the gaining owner was not told");

	expect(!hasSelectionWithin(_listA), "phase4", "the losing list is still :selection-within");
	expect(!isNodeSelected(_listA->getRowNode(2)), "phase4", "a row of the losing list is :selected");
	expect(hasSelectionWithin(_listB), "phase4", "the gaining list is not :selection-within");
	expect(hasSelectionWithin(_root), "phase4",
			"the ancestor shared by both lists lost :selection-within");
}

void SelectionLayout::runPhase5() {
	auto item = _listB->getItem(1);
	expect(_selection->select(_listB, makeSpanView(&item, 1)), "phase5", "the selection was refused");

	auto row = _listB->getRowNode(1);
	expect(_selection->getAnchorNode() == row, "phase5", "the materialized row is not the anchor");

	/* Recycle the row away, as a scrolling list does. The IDENTITY is untouched, so the selection
	must be too - and the chain has to fall back to the owner, because there is no node left to
	anchor on. A selection keyed on Node * has nothing to say here at all. */
	_listB->setMaterialized(1, false);

	// The projection is re-resolved in the visit, so this is the state one frame later; asserting
	// it immediately would be asserting the previous frame's answer
	expect(!_listB->isMaterialized(1), "phase5", "the row was not recycled away");
}

void SelectionLayout::runPhase6() {
	expect(_selection->getItems().size() == 1, "phase6",
			"recycling the row away dropped the selection");
	expect(_selection->isSelected(_listB->getItem(1)), "phase6",
			"the identity lost its selection when its node went away");
	expect(_selection->getAnchorNode() == _listB, "phase6",
			"an unmaterialized selection did not fall back to its owner");
	expect(hasSelectionWithin(_listB), "phase6",
			"the owner of an unmaterialized selection is not :selection-within");

	// Bring it back: the marker must land on the new node without anyone re-selecting anything
	_listB->setMaterialized(1, true);
	handleContentSizeDirty();
}

void SelectionLayout::runPhase7() {
	auto row = _listB->getRowNode(1);
	expect(row != nullptr, "phase7", "the row was not materialized again");
	expect(_selection->getAnchorNode() == row, "phase7",
			"a re-materialized row did not become the anchor again");
	expect(isNodeSelected(row), "phase7", "a re-materialized row did not get its :selected back");

	// A plain node that is its own identity: no owner interface, and the chain is simply itself
	expect(_selection->selectNode(_plain), "phase7", "selectNode was refused");
	expect(_selection->getOwner() == nullptr, "phase7", "selectNode invented an owner interface");
	expect(_selection->getOwnerNode() == _plain, "phase7", "selectNode set the wrong owner node");
	expect(_selection->getAnchorNode() == _plain, "phase7", "selectNode set the wrong anchor");
	expect(isNodeSelected(_plain), "phase7", "the plain node is not :selected");
	expect(hasSelectionWithin(_plain), "phase7", "the plain node is not :selection-within");
	expect(!hasSelectionWithin(_listB), "phase7", "the previous owner kept :selection-within");
	expect(!isNodeSelected(_listB->getRowNode(1)), "phase7", "the previous row kept :selected");
}

void SelectionLayout::runPhase8() {
	/* The reentrancy guard, under the two loads it has to survive.

	FIRST, the legitimate one: an owner that answers "you selected me" by handing the selection on -
	a container delegating to the thing the user really meant. That must settle on the redirected
	target, in one hop, with no recursion. */
	size_t depth = 0;
	size_t maxDepth = 0;

	_listA->setChangeHook([&](SpanView<SelectionItem> items) {
		++depth;
		maxDepth = sprt::max(maxDepth, depth);
		if (!items.empty()) {
			auto other = _listB->getItem(0);
			_selection->select(_listB, makeSpanView(&other, 1));
		}
		--depth;
	});

	auto item = _listA->getItem(0);
	_selection->select(_listA, makeSpanView(&item, 1));

	// A request made from inside a notification is applied by the loop in applyState, after the
	// current delivery returns - never on top of it
	expect(maxDepth <= 1, "phase8", "a selection made from a change callback recursed");
	expect(_selection->getOwner() == _listB, "phase8", "a redirecting owner did not hand it on");
	expect(_selection->getAnchorNode() == _listB->getRowNode(0), "phase8",
			"the redirected selection has the wrong anchor");

	/* SECOND, the pathological one: both owners redirect, so neither request ever equals the
	current state and the equality check cannot break the cycle. The bound has to.

	This deliberately provokes an engine error - one "keeps redirecting the selection" line in the
	log is the EXPECTED outcome here, not a fault. What is checked is that the frame ends at all,
	and ends in a consistent state rather than half-applied. */
	_listB->setChangeHook([&](SpanView<SelectionItem> items) {
		++depth;
		maxDepth = sprt::max(maxDepth, depth);
		if (!items.empty()) {
			auto other = _listA->getItem(1);
			_selection->select(_listA, makeSpanView(&other, 1));
		}
		--depth;
	});

	auto cycle = _listA->getItem(1);
	_selection->select(_listA, makeSpanView(&cycle, 1));

	expect(maxDepth <= 1, "phase8", "the cyclic exchange recursed instead of looping");
	expect(!_selection->empty(), "phase8", "the cyclic exchange ended with nothing selected");
	expect(_selection->getOwner() == _listA || _selection->getOwner() == _listB, "phase8",
			"the cyclic exchange ended on an owner that is neither list");
	expect(_selection->getItems().size() == 1, "phase8",
			"the cyclic exchange left more than one item");

	auto owner = static_cast<SelectionListOwner *>(_selection->getOwner());
	expect(_selection->getAnchorNode() != nullptr
					&& _selection->getAnchorNode()->getParent() == owner,
			"phase8", "the settled selection does not anchor inside its own owner");

	_listA->setChangeHook(nullptr);
	_listB->setChangeHook(nullptr);

	// And clearing takes the whole chain down
	expect(_selection->clear(), "phase8", "clear reported no change");
	expect(_selection->empty(), "phase8", "the selection survived a clear");
	expect(_selection->getAnchorNode() == nullptr, "phase8", "a cleared selection kept its anchor");
	expect(!hasSelectionWithin(_root), "phase8", "a cleared selection left :selection-within behind");
	expect(!isNodeSelected(_listA->getRowNode(0)) && !isNodeSelected(_listB->getRowNode(0)),
			"phase8", "a cleared selection left :selected behind");
	expect(!_selection->clear(), "phase8", "clearing an empty selection reported a change");
}

void SelectionLayout::runPhase9() {
	auto item = _listB->getItem(0);
	expect(_selection->select(_listB, makeSpanView(&item, 1)), "phase9", "the selection was refused");
	expect(hasSelectionWithin(_root), "phase9", "the ancestor did not take :selection-within");

	/* Now take the whole owner out of the scene while it still holds the selection.

	Nothing tells the system: a Node does not announce its removal to a system on a distant
	ancestor, and there is deliberately no subscription for it. The Rc the system holds keeps the
	owner addressable, and the visit notices it stopped running - which is why this is checked a
	frame later rather than on the next line. */
	_detached = _listB;

	/* Without the cleanup, deliberately.

	Node::cleanup() calls removeAllComponents(), and a node's NAME is a component (NodeIdentity) -
	so a plain removeFromParent() would strip the list's identity along with its markers, and what
	is left could not be told apart from a node that never had either. The case being checked here
	is a REPARENTING one: the owner is out of the scene but otherwise intact, which is the harder
	case for the chain, because its markers are still sitting there waiting to be released. */
	_listB->removeFromParent(false);
}

void SelectionLayout::runPhase10() {
	expect(!_listB->isRunning(), "phase10", "the owner is still in the scene");
	expect(_selection->empty(), "phase10", "an owner that left the scene kept the selection");
	expect(_selection->getAnchorNode() == nullptr, "phase10",
			"a dropped selection kept its anchor");

	/* The counters above the detached owner must have come down. This is the case that made the
	chain something the system STORES rather than re-walks: released by a getParent() walk from the
	anchor, the walk would stop at the detached list and _root would keep a count forever, staying
	`:selection-within` with nothing selected anywhere in the scene. */
	expect(!hasSelectionWithin(_root), "phase10",
			"a detached owner stranded :selection-within on its ancestor");
	expect(!hasSelectionWithin(this), "phase10",
			"a detached owner stranded :selection-within on the layout");
	expect(!isNodeSelected(_listB->getRowNode(0)), "phase10",
			"a detached owner's row kept :selected");
	expect(!hasSelectionWithin(_listB), "phase10",
			"a detached owner kept :selection-within on itself");
	expect(!hasSelectionWithin(_listB->getRowNode(0)), "phase10",
			"a detached owner's row kept :selection-within");

	// And the scene still works: putting it back and selecting again must behave exactly as before
	_root->addChild(_detached);
	_detached = nullptr;
	handleContentSizeDirty();

	auto item = _listB->getItem(0);
	expect(_selection->select(_listB, makeSpanView(&item, 1)), "phase10",
			"a re-attached owner could not take the selection");
	expect(_selection->getAnchorNode() == _listB->getRowNode(0), "phase10",
			"a re-attached owner got the wrong anchor");
	expect(hasSelectionWithin(_root), "phase10",
			"the ancestor did not take :selection-within again");

	expect(_selection->clear(), "phase10", "the final clear reported no change");
	expect(!hasSelectionWithin(_root), "phase10", "the final clear left :selection-within behind");
}

void SelectionLayout::runPhase11() {
	expect(_tree != nullptr && _tree->getRowCount() == 2, "phase11",
			"the tree did not start with its two collapsed categories");

	// Open the SECOND category and select its middle item. Category A is still collapsed, so
	// everything in B is about to move when it opens
	// Row 1 is the second category; row 0 is the first, still collapsed
	expect(_tree->expandRow(1), "phase11", "the second category refused to expand");
	_tree->requestRebuildNodes(true);
}

void SelectionLayout::runPhase12() {
	// rows: [cat-a] [cat-b] [cat-b-item-0] [cat-b-item-1] [cat-b-item-2]
	expect(_tree->getRowCount() == 5, "phase12", "the category did not expand");

	const size_t picked = 3; // cat-b-item-1
	_tree->setSelectedRow(picked);

	expect(_tree->getSelectedRow() == picked, "phase12", "the tree did not take the selection");

	// The tree OWNS the scene's selection, so the whole chain is its business now
	expect(_selection->getOwner() == static_cast<SelectionOwner *>(_tree), "phase12",
			"an owned TreeView did not become the scene's selection owner");
	expect(_selection->getOwnerNode() == _tree, "phase12", "the owner node is not the tree");
	expect(hasSelectionWithin(_tree), "phase12", "the owning tree is not :selection-within");
	expect(hasSelectionWithin(_root), "phase12",
			"the tree's ancestor is not :selection-within");

	auto selectedNode = _selection->getAnchorNode();
	expect(selectedNode != nullptr && selectedNode != _tree, "phase12",
			"a materialized row did not become the anchor");
	expect(isNodeSelected(selectedNode), "phase12", "the selected row is not :selected");

	// Remember WHAT was picked, by its data rather than by its number
	auto pickedTitle = _tree->getRows()[picked].getData().getString("title");
	expect(pickedTitle == "cat-b-item-1", "phase12", "the wrong row was picked to begin with");

	/* THE BUG. Expanding the category ABOVE inserts three rows before the selected one, so its
	index moves from 3 to 6. Before the identity remap, _selectedRow stayed at 3 and the highlight
	silently walked onto cat-b - a different element, of a different kind, with nothing anywhere to
	say it had happened. */
	expect(_tree->expandRow(0), "phase12", "the first category refused to expand");
	_tree->requestRebuildNodes(true);

	// The rebuild is deferred to the next visit, so the assertions live in the sign-off below
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.4f), [this, pickedTitle] {
		expect(_tree->getRowCount() == 8, "phase12", "the second category did not expand");

		const auto now = _tree->getSelectedRow();
		expect(now != maxOf<size_t>(), "phase12",
				"expanding a category above the selection dropped it");
		expect(now == 6, "phase12",
				toString("the selection did not follow its row: expected 6, got ", now));
		expect(_tree->getRows()[now].getData().getString("title") == pickedTitle, "phase12",
				"the selection moved to a DIFFERENT element when a category above it expanded");

		// ...and the scene-wide half followed too, onto the row's new node
		expect(_selection->getOwnerNode() == _tree, "phase12",
				"the tree stopped owning the selection across a rebuild");
		expect(isNodeSelected(_selection->getAnchorNode()), "phase12",
				"the anchor is not :selected after the rebuild");

		// Collapsing the row's OWN category takes it off screen entirely. The identity is
		// remembered, so re-expanding brings it back rather than making the user pick again
		// cat-b sits at row 4 now, after cat-a and its three items
		expect(_tree->collapseRow(4), "phase12", "the row's own category refused to collapse");
		_tree->requestRebuildNodes(true);
	}, Rc<DelayTime>::create(0.4f),
			[this, pickedTitle] {
		expect(_tree->getSelectedRow() == maxOf<size_t>(), "phase12",
				"a row inside a collapsed category is still a selected INDEX");

		expect(_tree->expandRow(4), "phase12", "the category refused to re-open");
		_tree->requestRebuildNodes(true);
	},
			Rc<DelayTime>::create(0.4f), [this, pickedTitle] {
		const auto back = _tree->getSelectedRow();
		expect(back != maxOf<size_t>(), "phase12",
				"re-expanding the category did not bring the selection back");
		expect(back != maxOf<size_t>()
						&& _tree->getRows()[back].getData().getString("title") == pickedTitle,
				"phase12", "the selection came back on the wrong row");

		runPhase13();
	}));
}

void SelectionLayout::runPhase13() {
	/* The opt-in focus coupling.

	What is asserted is not that the engine does this - it does not, and must not - but that a
	widget CAN do it in one call, and that doing it terminates. The second half is the whole reason
	the coupling was downgraded from an engine rule to an opt-in: a rule would have had to fight
	FormSystem's deferred commit and would have oscillated across frames with nothing to catch it. */
	const auto before = _focusSelects;

	expect(_focusListener->setFocused(), "phase13", "the widget could not take focus");

	/* A FRAME HAS TO PASS HERE, and that is not a quirk of the test - it is the third of the three
	reasons the engine-enforced version of this coupling could not be written.

	FocusGroup::setFocus only records _nextListener; the swap fires inside commitStorage, after the
	visit. So the callback that does the selecting has not run yet on this line. An engine rule
	reacting to a focus commit by adjusting the selection would therefore publish its answer one
	commit later, every time - a two-frame oscillation with nothing in the tree to catch it. An
	opt-in call has no such problem: it simply happens when the widget's own callback runs. */
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.3f), [this, before] {
		expect(_focusSelects == before + 1, "phase13",
				"taking focus did not run the coupling exactly once");

	expect(_selection->getOwnerNode() == _focusNode, "phase13",
			"taking focus did not select the widget that opted in");
	expect(isNodeSelected(_focusNode), "phase13", "the focused widget is not :selected");

	// A hotkey now reaches it first, which is the point of coupling them at all
	expect(_selection->getChain().size() > 1
					&& _selection->getChain().front().get() == _focusNode,
			"phase13", "the focused widget did not become the head of the chain");

	/* And the other direction stays SEPARATE. Selecting something else does not blur the widget -
	nothing in the engine ties the two - so the focus and the selection are now legitimately on
	different nodes. That is allowed, and it is the honest state: one chain is what was promised,
	not one node. */
	auto item = _listA->getItem(0);
	_selection->select(_listA, makeSpanView(&item, 1));

	expect(_selection->getOwner() == _listA, "phase13",
			"selecting elsewhere was overruled by the focused widget");
	expect(_focusSelects == before + 1, "phase13",
			"selecting elsewhere re-entered the focus coupling");
	expect(!isNodeSelected(_focusNode), "phase13",
			"the widget kept :selected after the selection moved off it");

		// Re-focusing it selects it again - the coupling is not one-shot. The select() above did
		// NOT blur it (nothing ties the two), so focus is handed to the group's other member first
		// to make the re-focus a real transition
		_focusOther->setFocused();
	}, Rc<DelayTime>::create(0.3f),
			[this] { _focusListener->setFocused(); }, Rc<DelayTime>::create(0.3f),
			[this] {
		expect(_selection->getOwnerNode() == _focusNode, "phase13",
				"re-focusing did not select the widget again");

		_selection->clear();

		log::source().info("SelectionTest", "SUMMARY: ", _checks, " checks, ", _failures,
				" failures");
	}));
}

// --- inspector -------------------------------------------------------------

Value SelectionLayout::encodeNode(Node *node) const {
	Value ret;
	ret.setString(node ? node->getName() : StringView(), "name");
	ret.setBool(isNodeSelected(node), "selected");
	ret.setBool(hasSelectionWithin(node), "selection-within");
	return ret;
}

Value SelectionLayout::encodeState() const {
	Value ret;
	if (!_selection) {
		ret.setBool(false, "ready");
		return ret;
	}

	ret.setBool(true, "ready");
	ret.setBool(_selection->empty(), "empty");
	ret.setString(_selection->getOwnerNode() ? _selection->getOwnerNode()->getName() : StringView(),
			"owner");
	ret.setString(_selection->getAnchorNode() ? _selection->getAnchorNode()->getName()
											  : StringView(),
			"anchor");
	ret.setInteger(int64_t(_selection->getItems().size()), "count");

	// The chain as a stylesheet sees it: every ancestor of the anchor carrying :selection-within,
	// deepest first. This is what the dispatcher pass will walk in a later increment
	// Always an ARRAY, including when it is empty: a Value that was only emplaced is still EMPTY
	// and encodes as null, so a driver asking "is the chain gone" would compare null to [] and see
	// a difference that is not there
	Value::ArrayType chainNodes;
	for (auto &node : _selection->getChain()) { chainNodes.emplace_back(Value(node->getName())); }

	auto &chain = ret.emplace("chain");
	chain.setArray(sp::move(chainNodes));

	/* How many times each watched node has been restyled for the selection component.

	Reported so a driver can hold a selection still for a hundred frames and prove the per-frame
	re-resolve is IDEMPOTENT: syncProjection runs on every visit, and a version of it that rewrote
	the markers each time would pass every end-state check in the stand while restyling the whole
	chain sixty times a second. */
	auto &dirty = ret.emplace("dirty");
	for (auto &it : _dirty) { dirty.setInteger(int64_t(it.second), it.first->getName()); }

	/* The chain as the DISPATCHER sees it - read out of the committed frame, not out of the system.

	These two are deliberately allowed to differ by one frame: the system is the live answer, the
	storage is the answer for the frame the user was looking at. A driver that steps frames sees
	them converge; one that reads immediately after a select() sees the lag, which is the contract
	and not a defect. */
	Value::ArrayType publishedNodes;
	if (auto director = getScene() ? getScene()->getDirector() : nullptr) {
		if (auto dispatcher = director->getInputDispatcher()) {
			for (auto &node : dispatcher->getSelectionChain()) {
				publishedNodes.emplace_back(Value(node->getName()));
			}
		}
	}
	auto &published = ret.emplace("published");
	published.setArray(sp::move(publishedNodes));

	ret.setInteger(int64_t(_checks), "checks");
	ret.setInteger(int64_t(_failures), "failures");
	return ret;
}

void SelectionLayout::registerCommands() {
	addCommand("state", "Report the selection: owner, anchor, item count and the marker chain",
			[this](Value &&) { return encodeState(); });

	addCommand("select", "Select an item: {owner, item}; omit item to clear", [this](Value &&args) {
		if (!_selection) {
			return ackValue(false);
		}
		const Value &req = args;
		auto owner = findOwner(req.getString("owner"));
		if (!owner) {
			if (req.getString("owner") == "plain") {
				return ackValue(_selection->selectNode(_plain));
			}
			return ackValue(_selection->clear());
		}
		if (!req.isInteger("item")) {
			return ackValue(_selection->clear());
		}
		auto item = owner->getItem(size_t(req.getInteger("item")));
		return ackValue(_selection->select(owner, makeSpanView(&item, 1)));
	});

	addCommand("materialize", "Build or recycle a row node: {owner, item, value}",
			[this](Value &&args) {
		const Value &req = args;
		auto owner = findOwner(req.getString("owner"));
		if (!owner) {
			return ackValue(false);
		}
		owner->setMaterialized(size_t(req.getInteger("item")), req.getBool("value"));
		handleContentSizeDirty();
		return ackValue(true);
	});

	addCommand("hotkey-log", "Who was offered a hotkey, in the order they were offered it",
			[this](Value &&) {
		Value ret;
		auto &list = ret.emplace("log");
		Value::ArrayType entries;
		for (auto &name : _hotkeyLog) { entries.emplace_back(Value(name)); }
		list.setArray(sp::move(entries));
		return ret;
	});

	addCommand("clear-log", "Drop the hotkey delivery log", [this](Value &&) {
		_hotkeyLog.clear();
		return ackValue(true);
	});

	addCommand("set-consume", "Make a subscriber accept or decline: {subscriber, value}",
			[this](Value &&args) {
		const Value &req = args;
		auto it = _consume.find(String(req.getString("subscriber")));
		if (it == _consume.end()) {
			return ackValue(false);
		}
		it->second = req.getBool("value");
		return ackValue(true);
	});

	addCommand("node", "Report one node's selection state: {owner, item}", [this](Value &&args) {
		const Value &req = args;
		auto name = req.getString("owner");
		if (name == "plain") {
			return encodeNode(_plain);
		}
		if (name == "root") {
			return encodeNode(_root);
		}
		auto owner = findOwner(name);
		if (!owner) {
			return ackValue(false);
		}
		if (!req.isInteger("item")) {
			return encodeNode(owner);
		}
		return encodeNode(owner->getRowNode(size_t(req.getInteger("item"))));
	});
}

} // namespace stappler::xenolith::app
