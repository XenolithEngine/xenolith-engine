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

#include "XLHotkey.h"

#include <sprt/runtime/thread/qonce.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/* The modifier families a chord can be built from, in the order encode() prints them.

   Everything outside this table is dropped by normalize(): the lock states and the mouse buttons
   describe the machine's state rather than the user's intent, LayoutAlternative is a Linux-only
   experiment, and bit 31 is the overloaded ValueTrue/Unmanaged flag.

   Mod5 has no sided variants in InputModifier, so its left/right entries are None. */
struct HotkeyModifierFamily {
	InputModifier base;
	InputModifier left;
	InputModifier right;
	StringView baseName;
	StringView leftName;
	StringView rightName;
};

static constexpr HotkeyModifierFamily HotkeyModifierFamilies[] = {
	{InputModifier::Ctrl, InputModifier::CtrlL, InputModifier::CtrlR, StringView("Ctrl"),
		StringView("CtrlL"), StringView("CtrlR")},
	{InputModifier::Alt, InputModifier::AltL, InputModifier::AltR, StringView("Alt"),
		StringView("AltL"), StringView("AltR")},
	{InputModifier::Shift, InputModifier::ShiftL, InputModifier::ShiftR, StringView("Shift"),
		StringView("ShiftL"), StringView("ShiftR")},
	{InputModifier::Mod3, InputModifier::Mod3L, InputModifier::Mod3R, StringView("Mod3"),
		StringView("Mod3L"), StringView("Mod3R")},
	{InputModifier::Mod4, InputModifier::Mod4L, InputModifier::Mod4R, StringView("Mod4"),
		StringView("Mod4L"), StringView("Mod4R")},
	{InputModifier::Mod5, InputModifier::None, InputModifier::None, StringView("Mod5"),
		StringView(), StringView()},
};

static constexpr InputModifier HotkeyBaseMask = InputModifier::Shift | InputModifier::Ctrl
		| InputModifier::Alt | InputModifier::Mod3 | InputModifier::Mod4 | InputModifier::Mod5;

static constexpr InputModifier HotkeySideMask = InputModifier::ShiftL | InputModifier::ShiftR
		| InputModifier::CtrlL | InputModifier::CtrlR | InputModifier::AltL | InputModifier::AltR
		| InputModifier::Mod3L | InputModifier::Mod3R | InputModifier::Mod4L | InputModifier::Mod4R;

InputModifier HotkeyCombo::normalize(InputModifier mods) {
	auto ret = mods;

	// A side implies its base, so a plain `Ctrl` combination still matches an event that reports
	// which Ctrl it was
	for (auto &it : HotkeyModifierFamilies) {
		if ((it.left != InputModifier::None && hasFlag(mods, it.left))
				|| (it.right != InputModifier::None && hasFlag(mods, it.right))) {
			ret |= it.base;
		}
	}

	return ret & (HotkeyBaseMask | HotkeySideMask);
}

InputModifier HotkeyCombo::baseModifiers(InputModifier mods) { return mods & HotkeyBaseMask; }

bool HotkeyCombo::matchesSides(InputModifier eventModifiers) const {
	for (auto &it : HotkeyModifierFamilies) {
		if (it.left != InputModifier::None && hasFlag(modifiers, it.left)
				&& !hasFlag(eventModifiers, it.left)) {
			return false;
		}
		if (it.right != InputModifier::None && hasFlag(modifiers, it.right)
				&& !hasFlag(eventModifiers, it.right)) {
			return false;
		}
	}
	return true;
}

HotkeyCombo HotkeyCombo::parse(StringView str) {
	HotkeyCombo ret;

	str.trimChars<StringView::WhiteSpace>();
	while (!str.empty()) {
		auto token = str.readUntil<StringView::Chars<'+'>>();
		token.trimChars<StringView::WhiteSpace>();
		if (str.is('+')) {
			++str;
		}
		if (token.empty()) {
			continue;
		}

		bool isModifier = false;
		for (auto &it : HotkeyModifierFamilies) {
			// the sided spellings first: "CtrlL" must not be mistaken for "Ctrl" plus junk
			if (!it.leftName.empty() && token == it.leftName) {
				ret.modifiers |= it.base | it.left;
			} else if (!it.rightName.empty() && token == it.rightName) {
				ret.modifiers |= it.base | it.right;
			} else if (token == it.baseName) {
				ret.modifiers |= it.base;
			} else {
				continue;
			}
			isModifier = true;
			break;
		}
		if (isModifier) {
			continue;
		}

		// Not a modifier, so it must be the key — and there can be only one
		if (ret.keycode != InputKeyCode::Unknown) {
			log::source().error("Hotkey", "More than one key in a combination: ", str);
			return HotkeyCombo();
		}

		for (uint32_t i = 1; i < toInt(InputKeyCode::Max); ++i) {
			auto code = InputKeyCode(i);
			if (core::getInputKeyCodeName(code) == token) {
				ret.keycode = code;
				break;
			}
		}

		if (ret.keycode == InputKeyCode::Unknown) {
			log::source().error("Hotkey", "Unknown key name: ", token);
			return HotkeyCombo();
		}
	}

	ret.modifiers = normalize(ret.modifiers);
	return ret;
}

void HotkeyCombo::encode(const Callback<void(StringView)> &out) const {
	for (auto &it : HotkeyModifierFamilies) {
		// A side is printed instead of its base, not next to it: "CtrlL+K", never "Ctrl+CtrlL+K"
		if (it.left != InputModifier::None && hasFlag(modifiers, it.left)) {
			out << it.leftName << "+";
		} else if (it.right != InputModifier::None && hasFlag(modifiers, it.right)) {
			out << it.rightName << "+";
		} else if (hasFlag(modifiers, it.base)) {
			out << it.baseName << "+";
		}
	}
	out << core::getInputKeyCodeName(keycode);
}

// Handed to the runtime's text-input processor, which has no way to know about hotkeys otherwise.
// It runs on whichever thread the window backend delivers input from, so it may only touch the
// registry through its own lock - which isReserved does.
static bool HotkeyRegistry_reservedKeyFilter(const InputEventData &data) {
	return HotkeyRegistry::getInstance()->isReserved(data);
}

HotkeyRegistry *HotkeyRegistry::getInstance() {
	static sprt::qonce s_once;
	static HotkeyRegistry *s_instance = nullptr;
	s_once([] {
		// Deliberately never destroyed: the registry outlives every scene and every window, and a
		// static destructor would race with whatever is still tearing down
		s_instance = new (sprt::nothrow) HotkeyRegistry();
		if (s_instance) {
			s_instance->_entries.emplace_back(nullptr); // index 0 is the invalid id

			/* Installed here rather than at application startup so that it can not be forgotten:
			   the moment anything registers a hotkey, the text-input processor starts declining
			   the combinations that are now reserved. An application that never uses hotkeys
			   never touches the registry and the filter stays null. */
			core::TextInputProcessor::setReservedKeyFilter(&HotkeyRegistry_reservedKeyFilter);
		}
	});
	return s_instance;
}

uint64_t HotkeyRegistry::comboKey(InputKeyCode keycode, InputModifier mods) {
	return (uint64_t(toInt(keycode)) << 32) | uint64_t(toInt(mods));
}

void HotkeyRegistry::bind(HotkeyId id, HotkeyCombo combo) {
	auto key = comboKey(combo.keycode, combo.modifiers);
	auto it = _byCombo.find(key);
	if (it == _byCombo.end()) {
		it = _byCombo.emplace(key, sprt::__malloc_vector<HotkeyId>()).first;
	}
	it->second.emplace_back(id);
}

void HotkeyRegistry::unbind(HotkeyId id, HotkeyCombo combo) {
	auto it = _byCombo.find(comboKey(combo.keycode, combo.modifiers));
	if (it == _byCombo.end()) {
		return;
	}
	for (auto iit = it->second.begin(); iit != it->second.end(); ++iit) {
		if (*iit == id) {
			it->second.erase(iit);
			break;
		}
	}
	if (it->second.empty()) {
		_byCombo.erase(it);
	}
}

HotkeyId HotkeyRegistry::add(StringView name, HotkeyCombo combo, StringView description,
		HotkeyOptions options) {
	if (name.empty()) {
		log::source().error("Hotkey", "A hotkey must have a name");
		return HotkeyId(0);
	}
	if (!combo.isValid()) {
		log::source().error("Hotkey", "Invalid combination for hotkey '", name, "'");
		return HotkeyId(0);
	}

	combo.modifiers = HotkeyCombo::normalize(combo.modifiers);

	sprt::unique_lock lock(_mutex);

	auto it = _byName.find(name);
	if (it != _byName.end()) {
		// Idempotent by name. A different combination is a rebind, not a duplicate: two modules
		// declaring the same hotkey must agree on the id, and the later configuration wins.
		auto entry = _entries[it->second.get()];
		if (entry->combo != combo) {
			log::source().info("Hotkey", "Rebinding '", name, "' to a different combination");
			unbind(it->second, entry->combo);
			entry->combo = combo;
			bind(it->second, combo);
		}
		if (!description.empty()) {
			entry->description = description.str<mem_std::Interface>();
		}
		entry->options |= options;
		return it->second;
	}

	auto entry = new (sprt::nothrow) Entry{name.str<mem_std::Interface>(),
		description.str<mem_std::Interface>(), combo, options};
	if (!entry) {
		return HotkeyId(0);
	}

	auto id = HotkeyId(uint32_t(_entries.size()));
	_entries.emplace_back(entry);
	_byName.emplace(StringView(entry->name), id);
	bind(id, combo);

	return id;
}

HotkeyId HotkeyRegistry::getId(StringView name) const {
	sprt::unique_lock lock(_mutex);
	auto it = _byName.find(name);
	return (it != _byName.end()) ? it->second : HotkeyId(0);
}

StringView HotkeyRegistry::getName(HotkeyId id) const {
	sprt::unique_lock lock(_mutex);
	if (id.get() == 0 || id.get() >= _entries.size()) {
		return StringView();
	}
	return StringView(_entries[id.get()]->name);
}

StringView HotkeyRegistry::getDescription(HotkeyId id) const {
	sprt::unique_lock lock(_mutex);
	if (id.get() == 0 || id.get() >= _entries.size()) {
		return StringView();
	}
	return StringView(_entries[id.get()]->description);
}

HotkeyCombo HotkeyRegistry::getCombo(HotkeyId id) const {
	sprt::unique_lock lock(_mutex);
	if (id.get() == 0 || id.get() >= _entries.size()) {
		return HotkeyCombo();
	}
	return _entries[id.get()]->combo;
}

HotkeyOptions HotkeyRegistry::getOptions(HotkeyId id) const {
	sprt::unique_lock lock(_mutex);
	if (id.get() == 0 || id.get() >= _entries.size()) {
		return HotkeyOptions::None;
	}
	return _entries[id.get()]->options;
}

bool HotkeyRegistry::setCombo(HotkeyId id, HotkeyCombo combo) {
	if (!combo.isValid()) {
		return false;
	}

	combo.modifiers = HotkeyCombo::normalize(combo.modifiers);

	sprt::unique_lock lock(_mutex);
	if (id.get() == 0 || id.get() >= _entries.size()) {
		return false;
	}

	auto entry = _entries[id.get()];
	if (entry->combo == combo) {
		return true;
	}

	unbind(id, entry->combo);
	entry->combo = combo;
	bind(id, combo);
	return true;
}

void HotkeyRegistry::match(InputKeyCode keycode, InputModifier mods,
		const Callback<bool(HotkeyId)> &cb) const {
	if (keycode == InputKeyCode::Unknown) {
		return;
	}

	auto full = HotkeyCombo::normalize(mods);
	auto base = HotkeyCombo::baseModifiers(full);

	sprt::unique_lock lock(_mutex);

	/* Two buckets, in this order: the sided bindings for exactly the side the event reports,
	   then the ones that named no side at all. When the backend reports no side the two keys
	   coincide and the second probe is skipped — which is also why a sided binding never fires
	   on a backend that does not report sides. */
	auto report = [&](uint64_t key) {
		auto it = _byCombo.find(key);
		if (it == _byCombo.end()) {
			return true;
		}
		for (auto &id : it->second) {
			if (!_entries[id.get()]->combo.matchesSides(full)) {
				continue;
			}
			if (!cb(id)) {
				return false;
			}
		}
		return true;
	};

	auto fullKey = comboKey(keycode, full);
	auto baseKey = comboKey(keycode, base);

	if (!report(fullKey)) {
		return;
	}
	if (fullKey != baseKey) {
		report(baseKey);
	}
}

void HotkeyRegistry::match(const InputEventData &data, const Callback<bool(HotkeyId)> &cb) const {
	if (!data.isKeyEvent()) {
		return;
	}
	match(data.key.keycode, data.getModifiers(), cb);
}

bool HotkeyRegistry::isReserved(const InputEventData &data) const {
	bool found = false;
	match(data, [&, this](HotkeyId id) {
		// match() holds _mutex while it calls this, so the entry is read directly rather than
		// through getOptions(), which would try to take the same lock again
		if (hasFlag(_entries[id.get()]->options, HotkeyOptions::ReserveFromTextInput)) {
			found = true;
			return false;
		}
		return true;
	});
	return found;
}

const EngineHotkeys &EngineHotkeys::get() {
	static sprt::qonce s_once;
	static EngineHotkeys s_hotkeys;
	s_once([] {
		auto reg = HotkeyRegistry::getInstance();
		auto add = [&](StringView name, StringView combo, StringView description) {
			return reg->add(name, HotkeyCombo::parse(combo), description);
		};

		s_hotkeys.back = add("org.stappler.xenolith.app.back", "ESCAPE", "Back / close");
		s_hotkeys.toggleFps =
				add("org.stappler.xenolith.debug.toggle-fps", "F12", "Cycle the FPS widget");

		s_hotkeys.focusNext =
				add("org.stappler.xenolith.focus.next", "TAB", "Focus the next field");
		s_hotkeys.focusPrev =
				add("org.stappler.xenolith.focus.prev", "Shift+TAB", "Focus the previous field");

		s_hotkeys.formSubmit = add("org.stappler.xenolith.form.submit", "ENTER", "Submit the form");
		s_hotkeys.formSubmitKeypad = add("org.stappler.xenolith.form.submit-keypad", "KP_ENTER",
				"Submit the form (keypad)");
		s_hotkeys.formActivate =
				add("org.stappler.xenolith.form.activate", "SPACE", "Activate the focused field");
		s_hotkeys.formReset = add("org.stappler.xenolith.form.reset", "ESCAPE", "Reset the form");

		/* ReserveFromTextInput, and it is not optional: an Alt chord carries a keychar, so the
		runtime's text-input processor would swallow it before the scene ever saw it. */
		s_hotkeys.moveItemUp =
				reg->add("org.stappler.xenolith.list.move-item-up", HotkeyCombo::parse("Alt+UP"),
						"Move the selected item up", HotkeyOptions::ReserveFromTextInput);
		s_hotkeys.moveItemDown = reg->add("org.stappler.xenolith.list.move-item-down",
				HotkeyCombo::parse("Alt+DOWN"), "Move the selected item down",
				HotkeyOptions::ReserveFromTextInput);

		s_hotkeys.textAccept =
				add("org.stappler.xenolith.text-input.accept", "ENTER", "Accept the field's text");
		s_hotkeys.textAcceptKeypad = add("org.stappler.xenolith.text-input.accept-keypad",
				"KP_ENTER", "Accept the field's text (keypad)");

		s_hotkeys.textSelectAll =
				add("org.stappler.xenolith.text-input.select-all", "Ctrl+A", "Select all");
		s_hotkeys.textCopy = add("org.stappler.xenolith.text-input.copy", "Ctrl+C", "Copy");
		s_hotkeys.textCut = add("org.stappler.xenolith.text-input.cut", "Ctrl+X", "Cut");
		s_hotkeys.textPaste = add("org.stappler.xenolith.text-input.paste", "Ctrl+V", "Paste");

		/* `edit`, not `text-input`: the same ids carry a document's history and a project's, and
		the focused handler decides which one answers. See the comment in the header. */
		s_hotkeys.undo = add("org.stappler.xenolith.edit.undo", "Ctrl+Z", "Undo");
		s_hotkeys.redo = add("org.stappler.xenolith.edit.redo", "Ctrl+Y", "Redo");
		s_hotkeys.redoAlt =
				add("org.stappler.xenolith.edit.redo-alt", "Ctrl+Shift+Z", "Redo (alternate)");
	});
	return s_hotkeys;
}

void HotkeyRegistry::enumerate(
		const Callback<bool(HotkeyId, StringView, StringView, HotkeyCombo)> &cb) const {
	sprt::unique_lock lock(_mutex);
	for (uint32_t i = 1; i < _entries.size(); ++i) {
		auto entry = _entries[i];
		if (!cb(HotkeyId(i), StringView(entry->name), StringView(entry->description),
					entry->combo)) {
			return;
		}
	}
}

} // namespace stappler::xenolith
