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

// One choice. `id` is the identity reported to the callback and collected by a form; `title` is
// presentation and may be localized (as ui::MenuSourceItem's name and title).
struct SP_PUBLIC SelectOption {
	String id;
	String title;
	IconName icon = IconName::None;
	bool enabled = true;
};

/* Options where id == title. Also serves ui::ChipRow (ChipOption is SelectOption); the caller may
edit the result before passing it on. Two overloads because SpanView<StringView> cannot be built
from a Vector<String>. */
SP_PUBLIC Vector<SelectOption> makeSelectOptions(SpanView<StringView>);
SP_PUBLIC Vector<SelectOption> makeSelectOptions(SpanView<String>);

/** A closed control that opens a list: the drop-down.

The closed face is a ui::Panel with an icon, a label and a chevron, styled by the Panel's CSS
appliers. The open list is a ui::MenuSource shown through ui::openMenuForNode.

Closed, the focused control steps the value with Up/Down (this also works where the window system
denies a popup keyboard focus). Open, the list is its own scene and ui::MenuSystem handles the
keys, starting at MenuConfig::highlight.

Keys are answered only while the widget is focused: inside a ui::FormSystem via the focused field's
node, standalone via tap to focus and tap outside to blur. For long lists use ui::SearchPicker.

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

	/* Places the parts by the resolved `direction`. Done here rather than in the content-size
	   phase, where an ancestor's StyleResolver has not yet re-resolved it. See placeInlineParts. */
	virtual void handleLayoutChildren() override;

	void placeInlineParts();

	// Replaces the list. The value is kept if an option still carries it, cleared otherwise.
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

	// Step to the next / previous enabled option. Does not wrap.
	virtual bool step(int32_t delta);

	// Open the list. False when the widget is disabled, has no options, is already open, or has no
	// window to open a surface on.
	virtual bool open();
	virtual void close();
	bool isOpen() const { return _popup != nullptr; }

	// The surface, while it is up. For a test, and for an owner that has to take it down itself.
	SubWindow *getPopup() const { return _popup; }

	// Geometry of the open list. The width defaults to the control's own, resolved at open time.
	virtual void setMenuStyle(const MenuStyle &);
	const MenuStyle &getMenuStyle() const { return _menuStyle; }

	/* The template the list is opened with: title, native preference, optional stylesheet. The
	callbacks, placement and `highlight` are filled in by open(). Without a stylesheet the surface
	inherits the sheet in force at the control (ui::openPopupSurface). */
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

	// The list, rebuilt from the options each time it is opened.
	Rc<MenuSource> makeSource();

	virtual void updateContent();
	virtual void updateInteractiveState();

	bool handleKey(const GestureData &);
	bool handleTap();

	core::RenderServerChannel *getParentWindow() const;

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
