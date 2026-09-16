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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICHIP_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICHIP_H_

#include "XLUiBadge.h"
#include "XLUiButton.h"
#include "XL2dIconSprite.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** One removable element of a set: a ui::Badge with a leading icon and a remove button. setText,
getText and setVariant behave as on a badge.

The node and its label are retyped to `chip`, so `badge` rules do not apply to chips.

The measure callback answers the natural width (padding, icon, shaped label, button); ui::ChipRow
wraps by it and `flex-basis: fit-content` resolves to it.

Selection and the meaning of removal belong to the owner (ui::ChipRow); the chip only shows the
`selected` class it is given.

CSS: type `chip`, class `xl-ui-chip`, states `.selected` and `:disabled`. Children are
`chip > icon` (the leading icon, hidden while it is IconName::None), `chip > label` and
`chip > button` (the remove button, named `remove`).

    chip { height:24px; border-radius:12px; background-color:#333; padding:0 4px 0 8px;
           display:flex; align-items:center; }
    chip.selected { outline-width:1px; outline-color:#FCB400; }
    chip > label { color:#E8E8E8; font-size:13px; }
    chip > button { width:18px; height:18px; } */
class SP_PUBLIC Chip : public Badge, public EditLockTarget {
public:
	using Callback = Function<void(NotNull<Chip>)>;

	virtual ~Chip();

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	/* Places the parts by the resolved `direction`, which is only settled at this phase, not in
	   handleContentSizeDirty. */
	virtual void handleLayoutChildren() override;

	void placeInlineParts();

	// The leading icon. IconName::None hides it, which is also the default.
	virtual void setIcon(IconName);
	IconName getIcon() const;

	// Whether the remove button is shown; a non-removable chip is a fixed member of the set.
	virtual void setRemovable(bool);
	bool isRemovable() const { return _removable; }

	virtual void setRemoveCallback(Callback &&);

	// A tap on the chip itself, not on its remove button; ui::ChipRow selects with this.
	virtual void setTapCallback(Callback &&);

	// Applies the `selected` class; the owner decides which chip is selected.
	virtual void setSelected(bool);
	bool isSelected() const { return _selected; }

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	/* The natural size answered by the measure callback. Shapes the label first: a Label reports
	zero width until its update has run. */
	virtual Size2 measureNatural() const;

	basic2d::IconSprite *getLeadingIcon() const { return _icon; }
	Button *getRemoveButton() const { return _remove; }

protected:
	using Badge::init;

	virtual void updateInteractiveState();

	// True when the point is over the remove button, which has already answered for itself.
	bool isOverRemoveButton(const Vec2 &location) const;

	basic2d::IconSprite *_icon = nullptr;
	Button *_remove = nullptr;
	InputListener *_listener = nullptr;

	Callback _removeCallback;
	Callback _tapCallback;

	bool _removable = true;
	bool _selected = false;

	// Edge trackers for InteractiveComponent's cumulative counters.
	bool _hoverApplied = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICHIP_H_
