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

#include "css/StateLayout.h"

#include "XLUiStyleResolver.h"
#include "XLInteractiveComponent.h"
#include "XLFocusWithin.h"
#include "XLSelection.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {
// The lock's reason is a CODE now, registered once as a constant of this stand - the registry takes
// literals only, and a message assembled per call would hand a reader a new number for the same
// sentence. The script names the text it expects; the command only chooses whether to lock.
static const uint32_t s_lockReason = diagnostic::registerMessage("owned by a wire");


/* One class per PAIR, so two states never compete for the same property on the same node: a
required read-write field would otherwise be painted by two rules at equal specificity and the check
would be about the cascade rather than about the state.

A second rule paints `color` from the same state, so the script can watch one state through two
properties - which is how it tells "the rule matched" apart from "the applier happened to run". */
static constexpr auto s_stateCss = StringView(R"css(
.st { background-color: #616161; color: #101010; }

.s-valid:valid { background-color: #43a047; }
.s-valid:invalid { background-color: #e53935; }
:invalid { color: #ff00ff; }

.s-ro:read-write { background-color: #1e88e5; }
.s-ro:read-only { background-color: #8e24aa; }

.s-req:optional { background-color: #6d4c41; }
.s-req:required { background-color: #fb8c00; }

.s-ind:indeterminate { background-color: #00897b; }

.s-def:default { background-color: #c0ca33; }

.s-focus:focus { background-color: #3949ab; }
.s-focus:focus-visible { background-color: #d81b60; }

.s-within:focus-within { background-color: #00acc1; }

.s-sel:selected { background-color: #8e24aa; }
.s-selw:selection-within { background-color: #00897b; }
)css");

static int64_t encodeColor(const Color4B &c) {
	return (int64_t(c.r) << 16) | (int64_t(c.g) << 8) | int64_t(c.b);
}

} // namespace

void StateLayout::addSample(StringView name, Node *node) {
	node->setName(name);
	_samples.emplace_back(Sample{name.str<Interface>(), node});
}

Value StateLayout::encodeSample(const Sample &sample) const {
	Value ret;

	uint32_t flags = 0;
	if (auto ic = sample.node->getComponent<InteractiveComponent>()) {
		flags = uint32_t(ic->state);
	}
	if (hasFocusWithin(sample.node)) {
		flags |= uint32_t(InteractiveState::FocusWithin);
	}
	if (hasSelectionWithin(sample.node)) {
		flags |= uint32_t(InteractiveState::SelectionWithin);
	}
	if (isNodeSelected(sample.node)) {
		flags |= uint32_t(InteractiveState::Selected);
	}
	ret.setInteger(int64_t(flags), "flags");

	// The resolved style, not the painted colour: the claim is that a rule matched
	auto style = ui::StyleResolver::resolveStyleForNode(sample.node);
	ret.setInteger(encodeColor(style.background().backgroundColor), "background");
	auto c = style.color();
	ret.setInteger(encodeColor(Color4B(c.r, c.g, c.b, 255)), "color");

	// Where to tap. An ADDRESS, not an expectation - the script asserts on states, and a
	// hand-copied coordinate would only make it brittle when the layout moves.
	auto center = sample.node->convertToWorldSpace(Vec2(sample.node->getContentSize().width / 2.0f,
			sample.node->getContentSize().height / 2.0f));
	ret.setInteger(int64_t(std::lround(center.x)), "x");
	ret.setInteger(int64_t(std::lround(center.y)), "y");

	return ret;
}

bool StateLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_stateCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_form = addSystem(Rc<ui::FormSystem>::create());

	// Explicit, distinct z-orders: the tab ring is document order, and document order is z-order
	// with an unstable sort behind it (see FormLayout for the same note).
	_req = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_req->setCaretBlink(false);
	_req->addStyleClass("st");
	_req->addStyleClass("s-valid");
	addSample("req", _req);
	ui::addFormField(_req, StringView("req"), ui::FormFieldFlags::Required);

	_opt = addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_opt->setCaretBlink(false);
	_opt->addStyleClass("st");
	_opt->addStyleClass("s-req");
	addSample("opt", _opt);
	ui::addFormField(_opt, StringView("opt"));

	// The same widget as `opt`, with the flag: the pair differs by nothing else
	auto required = addChild(Rc<ui::TextInput>::create(), ZOrder(3));
	required->setCaretBlink(false);
	required->addStyleClass("st");
	required->addStyleClass("s-req");
	addSample("required", required);
	ui::addFormField(required, StringView("required"), ui::FormFieldFlags::Required);

	_check = addChild(Rc<ui::Checkbox>::create(), ZOrder(4));
	_check->addStyleClass("st");
	_check->addStyleClass("s-focus");
	addSample("check", _check);
	ui::addFormField(_check, StringView("check"));

	_submit = addChild(Rc<ui::Button>::create(), ZOrder(5));
	_submit->setString("Submit");
	_submit->addStyleClass("st");
	_submit->addStyleClass("s-def");
	addSample("submit", _submit);
	ui::addFormButton(_submit, ui::FormFieldRole::Submit);

	// Outside the form: read-only is a property of the widget, not of a form
	_rw = addChild(Rc<ui::TextInput>::create(), ZOrder(6));
	_rw->setCaretBlink(false);
	_rw->addStyleClass("st");
	_rw->addStyleClass("s-ro");
	addSample("rw", _rw);

	_ro = addChild(Rc<ui::TextInput>::create(), ZOrder(7));
	_ro->setCaretBlink(false);
	_ro->addStyleClass("st");
	_ro->addStyleClass("s-ro");
	_ro->setReadOnly(true);
	addSample("ro", _ro);

	// The second source of the same bit: a lock owns this one's value
	_locked = addChild(Rc<ui::TextInput>::create(), ZOrder(8));
	_locked->setCaretBlink(false);
	_locked->addStyleClass("st");
	_locked->addStyleClass("s-ro");
	addSample("locked", _locked);

	_bar = addChild(Rc<ui::ProgressBar>::create(), ZOrder(9));
	_bar->addStyleClass("st");
	_bar->addStyleClass("s-ind");
	_bar->setProgress(0.5f);
	addSample("bar", _bar);

	// The focus-within pair, and a field deep enough that BOTH ancestors must take the state
	_outer = addChild(Rc<ui::Panel>::create(), ZOrder(10));
	_outer->addStyleClass("st");
	_outer->addStyleClass("s-within");
	addSample("outer", _outer);

	_inner = _outer->addChild(Rc<ui::Panel>::create(), ZOrder(1));
	_inner->addStyleClass("st");
	_inner->addStyleClass("s-within");
	addSample("inner", _inner);

	_nested = _inner->addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_nested->setCaretBlink(false);
	_nested->addStyleClass("st");
	_nested->addStyleClass("s-focus");
	addSample("nested", _nested);
	ui::addFormField(_nested, StringView("nested"));

	// The selection trio, deliberately shaped like the focus-within one above: two ancestors that
	// must BOTH take `:selection-within`, and a leaf that takes `:selected` as well. They are plain
	// ui::Panels with no InteractiveComponent of their own - which is the point, since a marker
	// component is the only way they could carry either state without also claiming `:enabled`.
	_selOuter = addChild(Rc<ui::Panel>::create(), ZOrder(11));
	_selOuter->addStyleClass("st");
	_selOuter->addStyleClass("s-selw");
	addSample("sel-outer", _selOuter);

	_selInner = _selOuter->addChild(Rc<ui::Panel>::create(), ZOrder(1));
	_selInner->addStyleClass("st");
	_selInner->addStyleClass("s-selw");
	addSample("sel-inner", _selInner);

	_selLeaf = _selInner->addChild(Rc<ui::Panel>::create(), ZOrder(1));
	_selLeaf->addStyleClass("st");
	// `s-sel` only: wearing `s-selw` as well would make two rules of EQUAL specificity match this
	// node - one class and one pseudo-class each - and source order would hand the leaf the
	// ancestor colour. Its `:selection-within` is asserted through the flags instead.
	_selLeaf->addStyleClass("s-sel");
	addSample("sel-leaf", _selLeaf);

	// A second leaf under the SAME inner panel: moving the selection between the two is what
	// proves the shared ancestors do not blink
	_selLeaf2 = _selInner->addChild(Rc<ui::Panel>::create(), ZOrder(2));
	_selLeaf2->addStyleClass("st");
	_selLeaf2->addStyleClass("s-sel");
	addSample("sel-leaf2", _selLeaf2);

	return true;
}

void StateLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	auto top = getWorkTop() - 16.0f;
	auto place = [&](Node *node, const Size2 &size) {
		if (node) {
			node->setAnchorPoint(Vec2(0.0f, 1.0f));
			node->setContentSize(size);
			node->setPosition(Vec2(48.0f, top));
			top -= size.height + 12.0f;
		}
	};

	place(_req, Size2(220.0f, 36.0f));
	place(_opt, Size2(220.0f, 36.0f));
	if (_samples.size() > 2) {
		place(_samples[2].node, Size2(220.0f, 36.0f)); // `required`
	}
	place(_check, Size2(28.0f, 28.0f));
	place(_submit, Size2(120.0f, 32.0f));
	place(_rw, Size2(220.0f, 36.0f));
	place(_ro, Size2(220.0f, 36.0f));
	place(_locked, Size2(220.0f, 36.0f));
	place(_bar, Size2(220.0f, 12.0f));

	if (_outer) {
		_outer->setAnchorPoint(Vec2(0.0f, 1.0f));
		_outer->setContentSize(Size2(320.0f, 120.0f));
		_outer->setPosition(Vec2(420.0f, getWorkTop() - 16.0f));
	}
	if (_inner) {
		_inner->setAnchorPoint(Vec2(0.0f, 1.0f));
		_inner->setContentSize(Size2(280.0f, 80.0f));
		_inner->setPosition(Vec2(20.0f, 100.0f));
	}
	if (_nested) {
		_nested->setAnchorPoint(Vec2(0.0f, 1.0f));
		_nested->setContentSize(Size2(240.0f, 36.0f));
		_nested->setPosition(Vec2(20.0f, 60.0f));
	}
}

Node *StateLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	for (auto &it : _samples) {
		if (it.name == name) {
			return it.node;
		}
	}
	return nullptr;
}

Node *StateLayout::selectionSample(StringView name) const {
	for (auto &it : _samples) {
		if (it.name == name) {
			return it.node;
		}
	}
	return nullptr;
}

void StateLayout::registerCommands() {
	/* Drive the two markers DIRECTLY, with no SelectionSystem in sight.

	That is the whole point of this group: it asserts that the component, the two InteractiveFlags
	bits, the selector parser and the five fold-in sites agree with each other - a claim that has to
	hold before anything is built on top, and one that a system in the middle would only blur. */
	addCommand("select", "Move the selection marker: {leaf} (empty clears)", [this](Value &&args) {
		auto leaf = static_cast<const Value &>(args).getString("leaf");
		auto next = leaf.empty() ? nullptr : selectionSample(leaf);

		// Leaf bit off the old one FIRST, then the chain move, then the leaf bit on the new one.
		// The chain release is what may take the component away, and it refuses to while `selected`
		// is still set - so clearing the bit first is what lets a node that is leaving the
		// selection entirely actually lose its component.
		if (_selected != next) {
			// The chains are COLLECTED and kept, not re-walked from the anchors: what has to be
			// released is what was retained, which stops being derivable the moment a node leaves
			// the graph. Nothing here detaches anything, but the API takes the chain so that a
			// caller which does cannot get it wrong
			Vector<Rc<Node>> chain;
			buildSelectionChain(next, chain);

			setNodeSelected(_selected, false);
			updateSelectionChain(_chain, chain);
			setNodeSelected(next, true);

			_chain = sp::move(chain);
			_selected = next;
		}

		Value ret;
		ret.setString(leaf, "selected");
		return ret;
	});

	addCommand("state", "Every sample: its interactive flags and its RESOLVED background/color",
			[this](Value &&) {
		Value ret;
		for (auto &it : _samples) { ret.setValue(encodeSample(it), it.name); }
		if (auto def = _form->getDefaultButton()) {
			ret.setString(def->getFieldName(), "defaultButton");
		}

		// The ring itself: `:default` is defined in terms of it, and a button missing from it is a
		// different failure than a button the form declined to make default
		Value ring;
		for (auto &it : _form->getTabRing()) {
			Value entry;
			entry.setString(it->getFieldName(), "name");
			entry.setInteger(int64_t(toInt(it->getRole())), "role");
			ring.addValue(sp::move(entry));
		}
		ret.setValue(sp::move(ring), "tabRing");
		if (auto focused = _form->getFocusedField()) {
			ret.setString(focused->getFieldName(), "focusedField");
		}
		ret.setBool(_form->isFocusVisible(), "focusVisible");
		return ret;
	});

	addCommand("submit", "Submit the form: an empty required field is rejected and marked",
			[this](Value &&) {
		Value ret;
		ret.setBool(_form->submit(), "submitted");
		return ret;
	});

	addCommand("set-text", "Type into a field programmatically: {target, text}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto input = dynamic_cast<ui::TextInput *>(getTarget(a))) {
			input->setText(a.getString("text"));
			ret.setBool(true, "applied");
		}
		return ret;
	});

	addCommand("set-lock", "Lock or unlock a control: {target, value, reason}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto node = getTarget(a)) {
			if (a.getBool("value")) {
				ui::setEditLock(node, s_lockReason);
			} else {
				ui::clearEditLock(node);
			}
			ret.setBool(true, "applied");
		}
		return ret;
	});

	addCommand("set-readonly", "The widget's OWN read-only mode: {target, value}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto input = dynamic_cast<ui::TextInput *>(getTarget(a))) {
			input->setReadOnly(a.getBool("value"));
			ret.setBool(true, "applied");
		}
		return ret;
	});

	addCommand("set-progress", "Progress, or nothing at all: {value} - omit for indeterminate",
			[this](Value &&args) {
		const Value &a = args;
		_bar->setProgress(a.hasValue("value") ? float(a.getDouble("value")) : nan());

		Value ret;
		ret.setBool(_bar->isIndeterminate(), "indeterminate");
		return ret;
	});

	addCommand("focus", "Focus a field as a DIRECT request - the path a tap takes: {target}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		auto node = getTarget(a);
		for (auto &it : _form->getFields()) {
			if (it->getOwner() == node) {
				ret.setBool(_form->focusField(it), "applied");
				break;
			}
		}
		return ret;
	});

	addCommand("set-enabled", "Enable or disable a control: {target, value}", [this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto button = dynamic_cast<ui::Button *>(getTarget(a))) {
			button->setEnabled(a.getBool("value"));
			ret.setBool(true, "applied");
		}
		return ret;
	});
}

} // namespace stappler::xenolith::app
