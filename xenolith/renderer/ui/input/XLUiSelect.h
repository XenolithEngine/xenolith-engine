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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUISELECT_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUISELECT_H_

#include "XLUiPanel.h"
#include "XLUiMenuPopup.h"
#include "XL2dIconSprite.h"
#include "XL2dLabel.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One choice, as DATA.
//
// `id` is the identity: it is what the callback reports, what a form collects, and what a test
// drives the widget by. `title` is presentation and may be localized out from under you - the same
// split as ui::MenuSourceItem's name and title, and for the same reason.
struct SP_PUBLIC SelectOption {
	String id;
	String title;
	IconName icon = IconName::None;
	bool enabled = true;
};

/* The id==title case, which is most of them: a list of names where the name IS the value. An enum
family's members, a set of role names, the twelve things a slot is allowed to be.

FREE rather than a second setOptions overload, for three reasons. setOptions is VIRTUAL, so an
overload doubles the override surface of every subclass. ui::ChipRow takes the same element
(ChipOption IS SelectOption), so one pair of functions serves both widgets instead of two identical
pairs of overloads. And the result is a Vector the caller may still EDIT - disable one entry, hang
an icon on another - before handing it over, which an overload cannot offer.

BOTH SPELLINGS EXIST because SpanView<StringView> is not constructible from a Vector<String>: the
generic SpanView(const T &) needs `T::data()` to give a `const StringView *`, and a Vector<String>
gives a `const String *`. A Vector<String> is what an enum family's members actually are, so the
overload that only took views would serve a literal array and nothing else. */
SP_PUBLIC Vector<SelectOption> makeSelectOptions(SpanView<StringView>);
SP_PUBLIC Vector<SelectOption> makeSelectOptions(SpanView<String>);

/** A closed control that opens a list: the drop-down.

WHAT IT IS MADE OF, and why none of it is new. The closed face is a ui::Panel with an icon, a label
and a chevron - the fill, the outline and the corners are the Panel's CSS appliers. The open list is
a ui::MenuSource shown through ui::openMenuForNode, so it is a real surface with the same placement
arithmetic, the same dismissal rules and the same keyboard as every other menu in this kit. There is
no second list widget here.

THE KEYBOARD IS IN TWO HALVES, and that is a property of where the two halves live. Closed, this
node is the one holding focus and Up/Down step the value in place - which is also the only path that
works where the window system refuses a popup the keyboard focus. Open, the list is a scene of its
own in a window of its own and this node cannot see those keys at all: ui::MenuSystem handles them,
and MenuConfig::highlight is how the list knows to start on the current value.

Keys are answered only while the widget is FOCUSED. Inside a ui::FormSystem that works for the same
reason ui::TextInput's arrows do - the group passes events to listeners at or below the focused
field's node; standalone, a tap takes focus and a tap outside gives it up.

WHAT IT IS NOT. A list of hundreds - every registered component, every member of a large enum - is
not a menu and must not be forced into one. That is a search palette, and it is a different widget.

CSS: type `select`, class `xl-ui-select`; classes `open` while the list is up and `disabled`.
Children: `select > icon` (the chosen option's icon, class `xl-ui-select-icon`), `select > label`,
`select > select-arrow` (the chevron - its own type, because two children typed `icon` under one
rule are indistinguishable). Pseudo-classes `:hover`, `:focus` and `:disabled` come from
InteractiveComponent, as they do for ui::Button.

    select { width:180px; height:34px; background-color:#292929; border-radius:6px;
             outline-width:1px; outline-color:rgba(255,255,255,.15); }
    select:focus { outline-color:#FCB400; }
    select.open { outline-color:#FCB400; }
    select > label { color:#E8E8E8; font-size:14px; }
    select > select-arrow { width:18px; height:18px; color:#9A9AA4; } */
class SP_PUBLIC Select : public Panel, public EditLockTarget {
public:
	// The id of the option now chosen, or empty when the value was cleared.
	using ChangeCallback = Function<void(StringView)>;

	virtual ~Select();

	virtual bool init() override;

	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	/* Replaces the list. A value that names an option that is still there survives; one that does
	not is cleared, because a control showing a choice nobody offers any more is lying. */
	virtual void setOptions(SpanView<SelectOption>);
	SpanView<SelectOption> getOptions() const { return _options; }

	// False when nothing carries that id - the value is left alone.
	virtual bool setValue(StringView id, bool silent = false);
	StringView getValue() const { return _value; }

	// Null when nothing is chosen.
	const SelectOption *getSelectedOption() const;

	// Shown in place of a title when nothing is chosen.
	virtual void setPlaceholder(StringView);
	StringView getPlaceholder() const { return _placeholder; }

	virtual void setChangeCallback(ChangeCallback &&);

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	// Step to the next / previous ENABLED option. Does not wrap: a list is not a dial, and running
	// off its end by holding an arrow down is not a choice anyone made.
	virtual bool step(int32_t delta);

	// Open the list. False when the widget is disabled, has no options, is already open, or has no
	// window to open a surface on.
	virtual bool open();
	virtual void close();
	bool isOpen() const { return _popup != nullptr; }

	// The surface, while it is up. For a test, and for an owner that has to take it down itself.
	SubWindow *getPopup() const { return _popup; }

	/* Geometry of the open list. The width defaults to the control's own, resolved at open time -
	a drop-down narrower than the thing it drops out of looks like a different widget. */
	virtual void setMenuStyle(const MenuStyle &);
	const MenuStyle &getMenuStyle() const { return _menuStyle; }

	/* The template the list is opened with: the stylesheet it carries, the title, whether it
	prefers a native surface. The callbacks, the placement and `highlight` are filled in by open().

	DECLARING THE STYLESHEET IS NOT OPTIONAL for a styled application: a native popup is a scene of
	its own and the application's sheet does not reach it. With none declared the list paints itself
	in the menu's own neutral colours, which is right for a test stand and wrong for a product. */
	virtual void setPopupConfig(MenuConfig &&);
	const MenuConfig &getPopupConfig() const { return _popupConfig; }

	// Focus, in the widget's own terms. The form calls these through FormFieldSlots; standalone,
	// the widget's own listeners do.
	virtual void focus();
	virtual void blur();
	bool isFocused() const { return _focused; }

	basic2d::Label *getLabel() const { return _label; }
	basic2d::IconSprite *getIcon() const { return _icon; }
	basic2d::IconSprite *getArrow() const { return _arrow; }

protected:
	using Panel::init;

	// Index of the option carrying `id`, or -1.
	int32_t indexOf(StringView id) const;

	// The list, rebuilt from the options each time it is opened: the model is the options vector,
	// and a cached MenuSource would be a second copy of it to keep in step.
	Rc<MenuSource> makeSource();

	virtual void updateContent();
	virtual void updateInteractiveState();

	bool handleKey(const GestureData &);
	bool handleTap();

	AppWindow *getAppWindow() const;

	Vector<SelectOption> _options;
	String _value;
	String _placeholder;
	ChangeCallback _changeCallback;

	MenuStyle _menuStyle;
	MenuConfig _popupConfig;
	Rc<SubWindow> _popup;

	basic2d::IconSprite *_icon = nullptr;
	basic2d::Label *_label = nullptr;
	basic2d::IconSprite *_arrow = nullptr;

	InputListener *_listener = nullptr;
	InputListener *_focusListener = nullptr;

	bool _focused = false;

	// Edge trackers for InteractiveComponent's cumulative counters.
	bool _hoverApplied = false;
	bool _focusApplied = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUISELECT_H_
