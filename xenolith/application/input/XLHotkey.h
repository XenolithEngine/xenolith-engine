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

#ifndef XENOLITH_APPLICATION_INPUT_XLHOTKEY_H_
#define XENOLITH_APPLICATION_INPUT_XLHOTKEY_H_

#include "XLInput.h"

#include <sprt/cxx/unordered_map>
#include <sprt/cxx/vector>
#include <sprt/runtime/thread/qmutex.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/*
	Global hotkeys.

	A hotkey is a named key combination in a process-wide registry, subscribed to by any
	InputListener (see InputListener::addHotkey). InputDispatcher delivers it ahead of the ordinary
	key route, without key recognizers, key masks or pointer hit-testing.

	The name (reverse-DNS, `org.stappler.xenolith.<area>.<action>` for engine hotkeys) is the stable
	identity; the combination can be rebound at runtime (setCombo) and the id survives a rebind.
	Registration is idempotent by name and normally done once, at startup:

		auto reg = HotkeyRegistry::getInstance();
		auto save = reg->add("org.example.editor.save", HotkeyCombo::parse("Ctrl+S"), "Save");
		listener->addHotkey(save, [](HotkeyId, const InputEvent &) { return doSave(); });
*/

// 0 is never handed out and means "no hotkey"
using HotkeyId = ValueWrapper<uint32_t, class HotkeyIdFlag>;

struct SP_PUBLIC HotkeyCombo {
	InputKeyCode keycode = InputKeyCode::Unknown;
	InputModifier modifiers = InputModifier::None; // always normalized

	/* Adds the base bit for every sided modifier present (CtrlL implies Ctrl) and drops everything
	   that is not part of a chord: lock states, mouse buttons, LayoutAlternative and bit 31
	   (ValueTrue/Unmanaged). Sided bits are kept, so a combination may demand a specific side. */
	static InputModifier normalize(InputModifier);

	// The same set with every sided bit stripped
	static InputModifier baseModifiers(InputModifier);

	/* True when an event carrying `eventModifiers` satisfies this combination's side
	   constraints: for every modifier family the combination names a side of, the event must
	   report that same side. A combination that names no side accepts either. */
	bool matchesSides(InputModifier eventModifiers) const;

	/* "Ctrl+Shift+P", "F12", "Alt+LEFT", "CtrlR+K". Unparsable input yields an empty (invalid)
	   combo. Modifier spellings follow InputModifier: Shift, Ctrl, Alt, Mod3 (a.k.a.
	   Command/Meta/Win), Mod4, Mod5 — each of the first five also in a sided form (`CtrlL`,
	   `ShiftR`, …). Key names follow InputKeyCode (see getInputKeyCodeName).

	   A sided combination fires only where the backend reports the side: Windows, macOS, Android
	   and both Linux backends do, wasm does not. */
	static HotkeyCombo parse(StringView);

	void encode(const Callback<void(StringView)> &) const;

	bool isValid() const { return keycode != InputKeyCode::Unknown; }

	bool operator==(const HotkeyCombo &) const = default;
	bool operator!=(const HotkeyCombo &) const = default;
};

/* Properties of the hotkey itself, as opposed to HotkeyFlags, which describe one subscription. */
enum class HotkeyOptions : uint32_t {
	None = 0,

	/* Decline this combination in the runtime's text-input processor, so it reaches the scene
	   even while a field holds the IME. Opt-in, because some keys belong to the IME (Escape,
	   Backspace, Delete); use it for chords that must win over typing, e.g. Alt or Super chords
	   that carry a keychar. */
	ReserveFromTextInput = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(HotkeyOptions)

enum class HotkeyFlags : uint32_t {
	None = 0,

	/* Deliver only while this listener is entitled to keyboard events in its focus group, as tested
	   by FocusGroup::canHandleEventWithListener, not `isFocused()` (ui::TextInput's listener never
	   holds focus; its FormInputListener does). A listener with no focus group always qualifies. */
	FocusedOnly = 1 << 0,

	/* Deliver even when an Exclusive focus group has scoped the walk to itself. For the few
	   bindings that must survive a modal dialog — quitting, switching windows. */
	BypassExclusive = 1 << 1,

	// Also fire on key auto-repeat, not just on the initial press
	Repeatable = 1 << 2,

	/* Deliver only while this listener's owner is on the committed selection chain (see
	   SelectionSystem and InputDispatcher::handleHotkey). It narrows who is offered the chord; a
	   handler with nothing to do must still return false. */
	SelectedOnly = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(HotkeyFlags)

/* Delivery state that a binding's flags are tested against. */
struct SP_PUBLIC HotkeyContext {
	// This listener is entitled to keyboard events in its focus group. Not isFocused(): see
	// HotkeyFlags::FocusedOnly
	bool focused = false;

	// A key auto-repeat rather than the initial press
	bool repeated = false;

	// An Exclusive focus group has scoped the walk to itself, and this listener is outside it
	bool exclusiveScoped = false;

	// This listener's owner is on the committed selection chain
	bool inSelection = false;
};

// Return true to consume the hotkey: the dispatcher stops the walk and the ordinary key route
// does not run. The id says which hotkey matched, since one callback may serve several.
using HotkeyCallback = Function<bool(HotkeyId, const InputEvent &)>;

class SP_PUBLIC HotkeyRegistry final {
public:
	static HotkeyRegistry *getInstance();

	/* Registers a hotkey, or returns the id of the one already registered under this name.
	   Re-registering an existing name with a different combo rebinds it (and logs), so the
	   last configuration wins rather than silently doing nothing. An empty name or an invalid
	   combo is rejected with a zero id. */
	HotkeyId add(StringView name, HotkeyCombo, StringView description = StringView(),
			HotkeyOptions = HotkeyOptions::None);

	HotkeyId getId(StringView name) const;
	StringView getName(HotkeyId) const;
	StringView getDescription(HotkeyId) const;
	HotkeyCombo getCombo(HotkeyId) const;
	HotkeyOptions getOptions(HotkeyId) const;

	// Rebinds an already-registered hotkey; the id is unaffected
	bool setCombo(HotkeyId, HotkeyCombo);

	/* Reports every hotkey bound to this event's combination (sided and base bindings), in
	   registration order; return false from the callback to stop. One combination may carry
	   several meanings (Escape: `…form.reset` and `…app.back`); listener visit order decides. */
	void match(const InputEventData &, const Callback<bool(HotkeyId)> &) const;
	void match(InputKeyCode, InputModifier, const Callback<bool(HotkeyId)> &) const;

	/* True when a hotkey carrying ReserveFromTextInput is bound to this event's combination —
	   that is, when the combination must not be turned into text. Safe to call from another
	   thread; this is what the runtime's reserved-key filter is wired to. */
	bool isReserved(const InputEventData &) const;

	void enumerate(
			const Callback<bool(HotkeyId, StringView name, StringView description, HotkeyCombo)> &)
			const;

protected:
	struct Entry {
		mem_std::String name;
		mem_std::String description;
		HotkeyCombo combo;
		HotkeyOptions options = HotkeyOptions::None;
	};

	HotkeyRegistry() = default;

	// combo -> ids; both expect _mutex to be held
	void bind(HotkeyId, HotkeyCombo);
	void unbind(HotkeyId, HotkeyCombo);

	/* The bucket a combination is filed under: base modifiers for an unsided combination,
	   base+side for a sided one. Lookup probes the event's full and base modifiers.
	   The keycode must stay in the low bits: keys are hashed by identity into power-of-two
	   buckets. */
	static uint64_t comboKey(InputKeyCode, InputModifier);

	mutable sprt::qmutex _mutex;

	/* Entries are heap-allocated and the vector holds pointers, so an entry's address — and
	   therefore the StringView that _byName keys on — survives the vector growing. Index 0 is a
	   null placeholder, which is what keeps HotkeyId(0) invalid. The registry is never
	   destroyed, so nothing is ever freed. */
	sprt::__malloc_vector<Entry *> _entries;
	sprt::__malloc_unordered_map<StringView, HotkeyId> _byName;
	sprt::__malloc_unordered_map<uint64_t, sprt::__malloc_vector<HotkeyId>> _byCombo;
};

/* The hotkeys the engine itself binds, with their stable names in one place. Registered on
   first access, not at static init, so an application can rebind them after the registry exists. */
struct SP_PUBLIC EngineHotkeys {
	HotkeyId back; // Escape — SceneContent's back/close
	HotkeyId toggleFps; // F12 — the basic2d FPS widget

	// Focus navigation is not form-specific: a standalone text field moves focus on Tab too
	HotkeyId focusNext; // Tab
	HotkeyId focusPrev; // Shift+Tab

	HotkeyId formSubmit; // Enter
	HotkeyId formSubmitKeypad; // KP_Enter — a separate combination, same meaning
	HotkeyId formActivate; // Space
	HotkeyId formReset; // Escape — shares the combination with `back`, and wins when focused

	/* Enter as a text field's accept; shares the combination with formSubmit and is offered first
	   so the field's accept wins over the form's submit. */
	HotkeyId textAccept; // Enter
	HotkeyId textAcceptKeypad; // KP_Enter

	/* Move the selected list element one place; the keyboard path for reordering, since a
	   virtualized list may have no node to drop on. */
	HotkeyId moveItemUp; // Alt+Up
	HotkeyId moveItemDown; // Alt+Down

	HotkeyId textSelectAll; // Ctrl+A
	HotkeyId textCopy; // Ctrl+C
	HotkeyId textCut; // Ctrl+X
	HotkeyId textPaste; // Ctrl+V

	/* Undo and redo, answered by whoever owns the focused thing; a handler with nothing to undo
	   must return false. Redo has two ids for its two combinations (Ctrl+Y, Ctrl+Shift+Z). No
	   ReserveFromTextInput needed: the text-input processor declines Ctrl chords without Alt. */
	HotkeyId undo; // Ctrl+Z
	HotkeyId redo; // Ctrl+Y
	HotkeyId redoAlt; // Ctrl+Shift+Z — the same action, the other habit

	static const EngineHotkeys &get();
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLHOTKEY_H_ */
