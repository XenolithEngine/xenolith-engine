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

	A hotkey is a named key combination, registered once in a process-wide registry and
	subscribed to by any InputListener (see InputListener::addHotkey). Delivery happens in
	InputDispatcher, ahead of the ordinary key route, so a subscriber needs neither a key
	recognizer nor a key mask nor a touch filter — in particular it is NOT hit-tested against
	the pointer position, which is what every hand-rolled key binding in the engine has had to
	work around.

	The name is the stable identity — reverse-DNS, `org.stappler.xenolith.<area>.<action>` for
	engine-owned ones. The combination is data: it can be rebound at runtime from a keymap
	(setCombo), and the numeric id stays valid across a rebind.

	A registration is idempotent by name, so two modules that both declare the same hotkey get
	the same id back. Registration is normally done once, at startup:

		auto reg = HotkeyRegistry::getInstance();
		auto save = reg->add("org.example.editor.save", HotkeyCombo::parse("Ctrl+S"), "Save");
		listener->addHotkey(save, [](HotkeyId, const InputEvent &) { return doSave(); });
*/

// 0 is never handed out and means "no hotkey"
using HotkeyId = ValueWrapper<uint32_t, class HotkeyIdFlag>;

struct SP_PUBLIC HotkeyCombo {
	InputKeyCode keycode = InputKeyCode::Unknown;
	InputModifier modifiers = InputModifier::None; // always normalized

	/* Adds the base bit for every sided modifier present (CtrlL implies Ctrl) and drops
	   everything that is not part of a chord: the lock states (CapsLock, NumLock, ScrollLock),
	   the mouse buttons, LayoutAlternative, and the overloaded bit 31 (ValueTrue/Unmanaged).

	   The sided bits are KEPT, because a combination is allowed to demand one side specifically
	   (see below). What normalization removes is the ambiguity in the other direction: a plain
	   `Ctrl` combination matches whichever Ctrl the user pressed, and NumLock being on never
	   changes whether anything matches. */
	static InputModifier normalize(InputModifier);

	// The same set with every sided bit stripped — the part two combinations must agree on
	// before their side constraints are even compared
	static InputModifier baseModifiers(InputModifier);

	/* True when an event carrying `eventModifiers` satisfies this combination's side
	   constraints: for every modifier family the combination names a side of, the event must
	   report that same side. A combination that names no side accepts either. */
	bool matchesSides(InputModifier eventModifiers) const;

	/* "Ctrl+Shift+P", "F12", "Alt+LEFT", "CtrlR+K". Unparsable input yields an empty (invalid)
	   combo. Modifier spellings follow InputModifier: Shift, Ctrl, Alt, Mod3 (a.k.a.
	   Command/Meta/Win), Mod4, Mod5 — each of the first five also in a sided form (`CtrlL`,
	   `ShiftR`, …). Key names follow InputKeyCode (see getInputKeyCodeName).

	   A sided combination only fires where the window backend reports which side was pressed:
	   Windows, macOS, Android and both Linux backends do. wasm does not — the browser knows
	   (KeyboardEvent.location) but the backend does not pass it on — so a sided binding is never
	   matched there. */
	static HotkeyCombo parse(StringView);

	void encode(const Callback<void(StringView)> &) const;

	bool isValid() const { return keycode != InputKeyCode::Unknown; }

	bool operator==(const HotkeyCombo &) const = default;
	bool operator!=(const HotkeyCombo &) const = default;
};

/* Properties of the hotkey itself, as opposed to HotkeyFlags, which describe one subscription. */
enum class HotkeyOptions : uint32_t {
	None = 0,

	/* Decline this combination in the runtime's text-input processor, so that it reaches the
	   scene even while a field holds the IME.

	   Opt-in on purpose. The processor claims a key for text unless it recognizes it as a
	   command, and some of what it claims is genuinely the IME's: Escape releases input,
	   Backspace and Delete edit. Reserving every registered combination would take those away
	   from the field the moment the engine registered `…app.back` on Escape. Set this only for a
	   combination that must win over typing — typically an Alt or Super chord, which the
	   processor would otherwise swallow because it carries a keychar. */
	ReserveFromTextInput = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(HotkeyOptions)

enum class HotkeyFlags : uint32_t {
	None = 0,

	/* Deliver only while this listener is entitled to keyboard events in its focus group.

	   That is deliberately not `isFocused()`: ui::TextInput's own listener never holds focus —
	   the FormInputListener above it does — so the test that actually works is the one the
	   dispatcher already uses, FocusGroup::canHandleEventWithListener. For a plain SingleFocus
	   group that means "this is the focused listener"; for ui::FormSystem it means "inside the
	   focused field's subtree". A listener with no focus group above it always qualifies. */
	FocusedOnly = 1 << 0,

	/* Deliver even when an Exclusive focus group has scoped the walk to itself. For the few
	   bindings that must survive a modal dialog — quitting, switching windows. */
	BypassExclusive = 1 << 1,

	// Also fire on key auto-repeat, not just on the initial press
	Repeatable = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(HotkeyFlags)

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

	/* Reports every hotkey bound to this event's combination, in registration order; the callback
	   is not called at all when the event matches nothing. Return false from it to stop.

	   It reports a set, not a single id, because one combination legitimately carries several
	   meanings at once: Escape is both `…form.reset` and `…app.back`. Which of them fires is
	   decided by the order in which listeners are visited, not here.

	   The callback form is what lets a sided binding and a base one both be reported for the same
	   event: they live in different buckets, so there is no single stored span to hand back. */
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

	/* The bucket a combination is filed under. A combination with no side constraint is filed
	   under its base modifiers alone; a sided one under base+side. Lookup therefore probes two
	   buckets — the event's full modifiers and its base ones — which is exactly "the sided
	   bindings for this side, plus every binding that does not care". */
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

/* The hotkeys the engine itself binds. Kept in one place so that the names — which are the
   stable identity an application rebinds against — are visible together rather than scattered
   across the widgets that happen to use them.

   Registered on first access, not at static-init: an application that rebinds them wants the
   registry to exist first, and one that never uses forms or text input pays nothing. */
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

	/* Enter also means "accept" to a text field. It shares the combination with formSubmit and is
	   offered first, because a field's own accept callback must win over the form's submit — the
	   walk order does that, exactly as the recognizer dispatch order used to. */
	HotkeyId textAccept; // Enter
	HotkeyId textAcceptKeypad; // KP_Enter

	/* Move the selected element of a list one place, without a pointer.

	Reordering by drag is unreachable from the keyboard in a virtualized list - the row you want to
	drop on may not exist as a node - and for an editor whose order IS the data, that makes the
	keyboard path the primary one rather than an accommodation. */
	HotkeyId moveItemUp; // Alt+Up
	HotkeyId moveItemDown; // Alt+Down

	HotkeyId textSelectAll; // Ctrl+A
	HotkeyId textCopy; // Ctrl+C
	HotkeyId textCut; // Ctrl+X
	HotkeyId textPaste; // Ctrl+V

	static const EngineHotkeys &get();
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLHOTKEY_H_ */
