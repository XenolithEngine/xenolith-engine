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

#ifndef TESTS_WINDOW_SRC_WIDGETS_SELECTIONLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SELECTIONLAYOUT_H_

#include "app/TestLayout.h"
#include "XLSelectionSystem.h"
#include "XLHotkey.h"
#include "XLInputListener.h"
#include "XLUiTreeView.h"
#include "XLUiStyleResolver.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// One item's identity, as an owner hands it out: a Ref allocated once and never reused, exactly
// like the data::Model::Node a TreeView will use for the same job. Deliberately NOT a Node - the
// point of the whole design is that an identity outlives the node that happens to show it
class SelectionItemRef : public Ref {
public:
	virtual ~SelectionItemRef() = default;

	bool init(size_t index) {
		_index = index;
		return true;
	}

	size_t getIndex() const { return _index; }

protected:
	size_t _index = 0;
};

/* A container that owns a selection: a VIRTUALIZED list, in miniature.

Test-local on purpose. ui::TreeView does not implement SelectionOwner until a later increment, and
even once it does, a stand built on it could not answer the question this one exists to ask -
"what happens when the selected item has no node" - without scrolling a real list to a computed
offset and hoping. Here materialization is a switch.

It is exactly as much of a list as the system can tell apart: identities that persist, row nodes
that come and go under them, and an answer to resolveSelectionNode that is allowed to be null. */
class SelectionListOwner : public Node, public SelectionOwner {
public:
	virtual ~SelectionListOwner() = default;

	virtual bool init(StringView name, size_t count);

	virtual Node *getSelectionOwnerNode() override { return this; }
	virtual Node *resolveSelectionNode(const SelectionItem &) const override;
	virtual void handleSelectionChanged(SpanView<SelectionItem>) override;

	// The opaque identity of item `index`, which is what a caller selects with
	SelectionItem getItem(size_t index) const;

	// Build or drop the row node for an item WITHOUT touching the selection - the recycling a
	// scrolling list does, and the case a selection keyed on Node * cannot survive
	void setMaterialized(size_t index, bool);
	bool isMaterialized(size_t index) const;

	Node *getRowNode(size_t index) const;

	size_t getCount() const { return _items.size(); }

	// What handleSelectionChanged was last told, and how many times it was told anything
	SpanView<SelectionItem> getLastNotified() const { return _lastNotified; }
	size_t getNotifyCount() const { return _notifyCount; }
	void resetNotifications();

	// Runs inside handleSelectionChanged, which is how the reentrancy guard is put under load
	void setChangeHook(Function<void(SpanView<SelectionItem>)> &&hook) { _hook = sp::move(hook); }

	// Called with each row node as it is built, so the stand can hang a hotkey subscriber on it.
	// A row is created and destroyed as the list virtualizes, so there is no other moment to do it
	void setRowBuiltCallback(Function<void(Node *, size_t)> &&cb) { _rowBuilt = sp::move(cb); }

protected:
	Vector<Rc<SelectionItemRef>> _items;
	Vector<Rc<Node>> _rows; // null where the item is not materialized

	Vector<SelectionItem> _lastNotified;
	size_t _notifyCount = 0;

	Function<void(SpanView<SelectionItem>)> _hook;
	Function<void(Node *, size_t)> _rowBuilt;
};

/* Verification layout for the scene-wide selection (XLSelectionSystem.h + XLSelection.h).
 *
 * Two owners side by side under one shared ancestor, plus a plain node that is its own identity.
 * That arrangement is what makes the invariants observable rather than merely plausible:
 *
 *  - ONE selection. Selecting into the second owner must take it away from the first, and the
 *    first must be TOLD, so a view can drop its highlight without watching the system;
 *
 *  - the identity outlives the node. An item whose row is recycled away keeps the selection, and
 *    the chain falls back to the owner - which is the entire reason the owner is recorded;
 *
 *  - the chain does not blink. Moving the selection between two rows of the same list must not
 *    dirty their shared ancestors at all, which is checked by counting component-dirty events on
 *    them rather than by looking at a colour: a retain-after-release implementation passes every
 *    end-state check and fails only this one;
 *
 *  - a multi-item selection anchors on the CONTAINER, not on an arbitrary one of its items;
 *
 *  - and the reentrancy guard: an owner that selects something else from inside its own change
 *    notification must terminate, with the last request winning and no recursion.
 */
class SelectionLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();
	void runPhase5();
	void runPhase6();
	void runPhase7();
	void runPhase8();
	void runPhase9();
	void runPhase10();
	void runPhase11();
	void runPhase12();
	void runPhase13();

	// Subscribes `name` to the stand's two hotkeys on `owner`, logging every delivery
	void addSubscriber(StringView name, Node *owner, HotkeyFlags);

	// How many ancestors of `node`, walking to the root, carry :selection-within
	size_t countSelectionWithin(Node *node) const;

	Value encodeState() const;
	Value encodeNode(Node *) const;

	SelectionListOwner *findOwner(StringView) const;

	SelectionSystem *_selection = nullptr;

	// The shared ancestor of both lists: what must NOT be restyled when the selection moves
	// between two rows of the same one
	Node *_root = nullptr;

	SelectionListOwner *_listA = nullptr;
	SelectionListOwner *_listB = nullptr;

	// Its own identity, with no owner interface behind it - the selectNode() case
	Node *_plain = nullptr;

	/* A REAL ui::TreeView, opted in as a selection owner.
	
	The two lists above are miniatures built to exercise the system; this one is the widget an
	application actually uses, and it is here for the one property a miniature cannot have: rows
	whose INDEX moves when a category above them is expanded. That is the bug the identity remap
	fixes, and it is invisible in every other kind of test - the selection is still "row 5", it is
	simply the wrong row 5. */
	/* A widget that OPTS IN to "taking focus also selects me".

	This is the whole of the focus coupling: one call, in the widget's own focus-in path. There is
	deliberately no engine rule doing it, and no engine widget opts in - the coupling is here to be
	demonstrated and to prove it terminates, not to be switched on for everybody. See
	SelectionSystem's header for why an enforced version is not expressible. */
	Node *_focusNode = nullptr;
	InputListener *_focusListener = nullptr;
	FocusGroup *_focusGroup = nullptr;
	InputListener *_focusOther = nullptr; // the group's other member, to move focus AWAY
	size_t _focusSelects = 0;

	ui::TreeView *_tree = nullptr;
	Rc<data::Model> _model;
	data::Model::Node *_categoryA = nullptr; // above the selected row; expanded to shift it
	data::Model::Node *_categoryB = nullptr; // holds the row the selection is put on

	// Held across the teardown phase so the detached list is not destroyed the moment the system
	// lets go of it - the check afterwards has to be able to look at it
	Rc<Node> _detached;

	// Component-dirty counters, filled by a callback on each node
	Map<Node *, size_t> _dirty;

	/* Hotkey subscribers along the chain, and the log of who was offered what.
	
	The delivery ORDER is the claim - deepest first - and it cannot be seen from any one delivery,
	only from the sequence. Every subscriber declines by default, so one press walks the whole
	chain and the log is the complete order rather than just its first element. */
	struct Subscriber {
		String name;
		InputListener *listener = nullptr;
		bool consume = false;
	};

	Vector<Rc<Ref>> _subscriberRefs; // keeps the row subscribers' state alive across recycling
	Vector<String> _hotkeyLog;
	Map<String, bool> _consume;

	HotkeyId _chainKey; // Ctrl+J - plain, offered to everyone
	HotkeyId _selKey; // Ctrl+H - SelectedOnly

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SELECTIONLAYOUT_H_
