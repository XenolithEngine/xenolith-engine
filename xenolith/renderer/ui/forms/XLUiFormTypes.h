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

	// Reachable by Tab, but never collected or validated (e.g. a search box inside the form)
	Transient = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(FormFieldFlags)

struct FormValidationError {
	String name;
	String message;
};

// Callbacks through which the form drives a widget without knowing its type. An empty slot is a
// no-op; a field with no `collect` is absent from the collected value.
struct FormFieldSlots {
	Function<Value()> collect;
	Function<void(const Value &)> assign;
	Function<void()> clear;

	/* Take or release focus in the widget's terms (IME, caret), called after the focus group has
	switched. `backwards` is true only for Shift+Tab navigation; a composite widget uses it to
	enter at its last part. */
	Function<void(bool focused, bool backwards)> setFocused;

	// Enter or Space on a focused field; true when consumed. False lets the form submit
	Function<bool()> activate;

	// Programmatic editing actions (menus, commands); key bindings stay in the widget
	Function<bool()> copy;
	Function<bool()> cut;
	Function<bool()> paste;
	Function<bool()> selectAll;

	// True when the widget writes InteractiveComponent's focus counter itself; otherwise the
	// listener does. Never both: the counter is cumulative
	bool ownsFocusStyle = false;

	// False excludes the widget from the tab ring; re-read on every ring rebuild
	bool focusable = true;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMTYPES_H_
