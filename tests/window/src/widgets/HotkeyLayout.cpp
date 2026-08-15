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

#include "XLCommon.h"

#include "widgets/HotkeyLayout.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

// The stand registers its own hotkeys rather than reusing the engine's, so that a rebind test can
// not disturb Tab or Escape for the rest of the app
static constexpr auto ActionName = StringView("org.stappler.test.hotkey.action");
static constexpr auto SharedName = StringView("org.stappler.test.hotkey.shared");
static constexpr auto BypassName = StringView("org.stappler.test.hotkey.bypass");
static constexpr auto SidedName = StringView("org.stappler.test.hotkey.sided");
static constexpr auto ReservedName = StringView("org.stappler.test.hotkey.reserved");

} // namespace

void HotkeyLayout::setupHotkeys() {
	auto reg = HotkeyRegistry::getInstance();

	_action = reg->add(ActionName, HotkeyCombo::parse("Ctrl+K"), "The ordinary case");
	_shared = reg->add(SharedName, HotkeyCombo::parse("F7"), "Bound by several subscribers");
	_bypass = reg->add(BypassName, HotkeyCombo::parse("Ctrl+Q"), "Survives an exclusive group");

	// Same key as _action: an event from the left Ctrl matches both, and the sided one is
	// reported first
	_sided = reg->add(SidedName, HotkeyCombo::parse("CtrlL+K"), "Left Ctrl only");

	// Alt+F carries a keychar on every backend, so without the reserved-key filter the text-input
	// processor would type it instead of letting it through
	_reserved = reg->add(ReservedName, HotkeyCombo::parse("Alt+F"), "Reserved against text input",
			HotkeyOptions::ReserveFromTextInput);
}

HotkeyLayout::Subscriber *HotkeyLayout::addSubscriber(StringView name, Node *owner,
		HotkeyFlags flags, bool consume) {
	auto index = _subscribers.size();
	_subscribers.emplace_back(Subscriber{name});

	auto listener = owner->addSystem(Rc<InputListener>::create());
	_subscribers[index].listener = listener;

	// Every subscriber takes all three, so the log shows which of them was even offered the key
	for (auto id : {_action, _shared, _bypass, _sided, _reserved}) {
		listener->addHotkey(id, [this, index](HotkeyId id, const InputEvent &) {
			auto &sub = _subscribers[index];
			if (!sub.enabled) {
				return false;
			}
			note(sub.name, id);
			return sub.consume;
		}, flags);
	}

	_subscribers[index].consume = consume;
	return &_subscribers[index];
}

bool HotkeyLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setupHotkeys();

	// Fixed capacity up front: the callbacks index into this vector, so it must never reallocate
	_subscribers.reserve(5);

	/* `declining` is in the pre-scene band (positive priority), so it is always the first
	   candidate the walk offers the key to — which is what makes "it declined and the next one
	   got it" observable rather than a coincidence of visit order. */
	auto declining = addSubscriber(StringView("declining"), this, HotkeyFlags::None, false);
	declining->listener->setPriority(1);

	// No focus group above this node, so it is always eligible
	addSubscriber(StringView("global"), this, HotkeyFlags::None, true);

	// Counts the plain key events that were NOT swallowed by a hotkey. F8 is bound to nothing, so
	// it must always land here; Ctrl+K must land here only when every subscriber declined.
	auto fallthrough = addSystem(Rc<InputListener>::create());
	fallthrough->setPriority(-1);
	fallthrough->addKeyRecognizer(
			[this](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			++_fallthroughCount;
		}
		return true;
	},
			InputKeyInfo{makeKeyMask(
					{InputKeyCode::K, InputKeyCode::F7, InputKeyCode::F8, InputKeyCode::Q})});
	// A key event carries the pointer position and this node may not be under it
	fallthrough->setTouchFilter(
			[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return true;
		}
		return cb(event);
	});

	_field = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_field->setName("field");
	_field->setPlaceholder("Type here");
	_field->setCaretBlink(false);

	auto focusNode = addChild(Rc<Node>::create());
	focusNode->setName("focus-node");
	_focusGroup = focusNode->addSystem(Rc<FocusGroup>::create());
	_focusGroup->setEventMask(FocusGroup::EventMask(EventMaskKeyboard));
	_focusGroup->setFlags(FocusGroup::Flags::SingleFocus);

	/* Two FocusedOnly subscribers in the SAME group, so that "not focused" can be tested as the
	   group actually models it - present in the walk, but not the one holding focus - rather than
	   as "absent from the scene". */
	auto focused = addSubscriber(StringView("focused"), focusNode, HotkeyFlags::FocusedOnly, true);
	addSubscriber(StringView("sibling"), focusNode, HotkeyFlags::FocusedOnly, true);
	focused->listener->setFocused();

	/* The exclusive group lives on a node that starts hidden: an invisible node is not visited, so
	   neither the group nor its listener reach the dispatcher's storage at all. Toggling the node
	   is therefore the whole switch. */
	_exclusiveNode = addChild(Rc<Node>::create());
	_exclusiveNode->setName("exclusive-node");
	_exclusiveNode->setVisible(false);
	_exclusiveGroup = _exclusiveNode->addSystem(Rc<FocusGroup>::create());
	_exclusiveGroup->setEventMask(FocusGroup::EventMask(EventMaskKeyboard));
	_exclusiveGroup->setFlags(FocusGroup::Flags::Exclusive);

	addSubscriber(StringView("exclusive"), _exclusiveNode, HotkeyFlags::None, true);

	// Ctrl+Q must reach `global` even while the exclusive group is up
	_subscribers[1].listener->addHotkey(_bypass, [this](HotkeyId id, const InputEvent &) {
		note(StringView("global-bypass"), id);
		return true;
	}, HotkeyFlags::BypassExclusive);

	return true;
}

void HotkeyLayout::handleContentSizeDirty() { TestLayout::handleContentSizeDirty(); }

void HotkeyLayout::note(StringView subscriber, HotkeyId id) {
	Value entry;
	entry.setString(subscriber, "subscriber");
	entry.setString(HotkeyRegistry::getInstance()->getName(id), "hotkey");
	_log.emplace_back(sp::move(entry));
}

Value HotkeyLayout::encodeLog() const {
	Value ret;
	// newArray, not emplace: an EMPTY value encodes as null, and a test that asserts "nothing was
	// delivered" would then have to special-case the empty case
	auto &log = ret.newArray("log");
	for (auto &it : _log) { log.addValue(it); }
	ret.setInteger(_fallthroughCount, "fallthrough");
	return ret;
}

Value HotkeyLayout::encodeRegistry() const {
	Value ret;
	auto &list = ret.newArray("hotkeys");
	HotkeyRegistry::getInstance()->enumerate(
			[&](HotkeyId id, StringView name, StringView description, HotkeyCombo combo) {
		Value entry;
		entry.setInteger(id.get(), "id");
		entry.setString(name, "name");
		entry.setString(description, "description");

		StringStream combined;
		combo.encode([&](StringView str) { combined << str; });
		entry.setString(combined.str(), "combo");

		list.addValue(sp::move(entry));
		return true;
	});
	return ret;
}

void HotkeyLayout::registerCommands() {
	addCommand("log", "Report every hotkey delivery so far, plus the fallthrough count",
			[this](Value &&) { return encodeLog(); });

	addCommand("clear", "Drop the delivery log and reset the fallthrough count", [this](Value &&) {
		_log.clear();
		_fallthroughCount = 0;
		return ackValue(true);
	});

	addCommand("list", "Enumerate the whole hotkey registry",
			[this](Value &&) { return encodeRegistry(); });

	addCommand("rebind", "Rebind a hotkey by name: {name, combo}", [](Value &&args) {
		const Value &req = args;
		auto reg = HotkeyRegistry::getInstance();
		auto id = reg->getId(req.getString("name"));
		if (id.empty()) {
			return ackValue(false);
		}
		return ackValue(reg->setCombo(id, HotkeyCombo::parse(req.getString("combo"))));
	});

	addCommand("set-consume", "Make a subscriber accept or decline: {subscriber, value}",
			[this](Value &&args) {
		const Value &req = args;
		auto name = req.getString("subscriber");
		for (auto &it : _subscribers) {
			if (it.name == name) {
				it.consume = req.getBool("value");
				return ackValue(true);
			}
		}
		return ackValue(false);
	});

	addCommand("set-enabled", "Silence a subscriber entirely: {subscriber, value}",
			[this](Value &&args) {
		const Value &req = args;
		auto name = req.getString("subscriber");
		for (auto &it : _subscribers) {
			if (it.name == name) {
				it.enabled = req.getBool("value");
				return ackValue(true);
			}
		}
		return ackValue(false);
	});

	addCommand("focus", "Give a subscriber the keyboard focus: {subscriber}", [this](Value &&args) {
		auto name = static_cast<const Value &>(args).getString("subscriber");
		for (auto &it : _subscribers) {
			if (it.name == name) {
				return ackValue(it.listener->setFocused());
			}
		}
		return ackValue(false);
	});

	addCommand("focus-field", "Give the text field the IME, or release it: {value}",
			[this](Value &&args) {
		if (!_field) {
			return ackValue(false);
		}
		if (static_cast<const Value &>(args).getBool("value")) {
			_field->focus();
		} else {
			_field->blur();
		}
		return ackValue(true);
	});

	addCommand("field-state", "Report the text field: focus and current text", [this](Value &&) {
		Value ret;
		ret.setBool(_field && _field->isFocused(), "focused");
		ret.setString(_field ? _field->getText() : StringView(), "text");
		return ret;
	});

	addCommand("set-exclusive", "Raise or drop the exclusive focus group: {value}",
			[this](Value &&args) {
		if (!_exclusiveNode) {
			return ackValue(false);
		}
		_exclusiveNode->setVisible(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
