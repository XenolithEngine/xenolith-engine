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

#ifndef XENOLITH_RENDERER_UI_FORMS_XLUIFORMTYPES_H_
#define XENOLITH_RENDERER_UI_FORMS_XLUIFORMTYPES_H_

#include "XLUiConfig.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

enum class FormFieldRole {
	Field,
	Submit,
	Reset,
};

// How FormSystem::collect() shapes its result. Nested reads a dotted field name as a path, so
// "user.name" becomes { "user": { "name": ... } }; Flat takes the name verbatim as one key.
enum class FormValueMode {
	Flat,
	Nested,
};

enum class FormFieldFlags : uint32_t {
	None = 0,

	// Rejected by submit() when its collected value is null, an empty string or an empty container
	Required = 1 << 0,

	// Reachable by Tab, but never collected and never validated - a search box standing inside the
	// form's subtree without being part of what the form submits
	Transient = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(FormFieldFlags)

struct FormValidationError {
	String name;
	String message;
};

// What a widget node hands its FormInputListener so the form can drive it.
//
// This is the whole seam between a form and a widget: the form knows nothing about TextInput or
// Checkbox, only about these callbacks. A node fills in what it can actually do - an empty slot is
// a no-op, and a field with no `collect` never appears in the collected value at all. That is what
// lets a foreign widget join a form without either side knowing the other's type.
struct FormFieldSlots {
	Function<Value()> collect;
	Function<void(const Value &)> assign;
	Function<void()> clear;

	/* Take or release keyboard focus in the widget's own terms: raise the IME, show the caret.
	Called AFTER the focus group has already switched, never instead of it - the group's focus is
	what decides who gets keys, and this only tells the widget to catch up.

	`backwards` is the direction of the NAVIGATION that caused this, and false whenever the cause
	was anything else - a tap, a programmatic focusField(), the field leaving the ring. A simple
	widget ignores it; a COMPOSITE one cannot, because it has to decide which of its parts the
	focus landed on: Shift+Tab arriving at a row of number fields means the last one, and a field
	that always enters at its first part makes backwards navigation walk forwards inside it. */
	Function<void(bool focused, bool backwards)> setFocused;

	// Enter or Space on a focused field. Return true when the widget consumed it: a Checkbox
	// toggles, a Button fires. A single-line TextInput returns false, and the form submits instead
	Function<bool()> activate;

	// The standard editing actions. The key bindings for them live in the widget, so that a
	// ui::TextInput outside a form behaves identically; these slots are the programmatic entry
	// point - for a context menu, a toolbar, an inspector command, or the form itself
	Function<bool()> copy;
	Function<bool()> cut;
	Function<bool()> paste;
	Function<bool()> selectAll;

	// The widget writes InteractiveComponent's focus counter itself (ui::TextInput does, because
	// its focus is the IME's and only the echo knows when it really changed). When false the
	// listener writes it, so CSS `:focus` also works for widgets that do not. Never both: the
	// counter is cumulative and a double write leaves it stuck.
	bool ownsFocusStyle = false;

	// Never reachable by Tab - a read-only or disabled widget. Read once per frame while the tab
	// ring is rebuilt, so a widget may flip it at any time
	bool focusable = true;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMTYPES_H_
