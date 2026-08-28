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

/** One element of a set: a label you can take off again.

A ui::Badge SAYS something; a chip is something a person put there and can remove. That difference
is the leading icon and the remove button, and it is the whole of this class - which is why it is a
ui::Badge with two children rather than a second widget that happens to look like one. setText,
getText and setVariant therefore mean exactly what they mean on a badge, in both.

IT RETYPES ITSELF. init() calls Badge::init and then declares itself `chip`: a stylesheet addressing
`badge` must not paint chips, and one addressing `chip` must not have to know that a chip is
implemented as a badge. The inherited label is retyped with it, for the same reason.

IT ANSWERS THE MEASUREMENT PROTOCOL. `MaxContent` is its natural width - the padding, the icon, the
shaped label and the button. That single answer serves two callers: ui::ChipRow wraps by it, and
`flex-basis: fit-content` resolves to it for a chip placed in a flex container by CSS. A chip that
answered only one of them would wrap differently from the way it is drawn.

WHAT IT DOES NOT DECIDE. Selection is the ROW's - a chip only paints the `selected` class it is
told to wear - and so is what removing one means. A lone chip with a remove callback is perfectly
usable, but it does not know about any others.

CSS: type `chip`, class `xl-ui-chip`, states `.selected` and `.disabled`. Children are
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

	// The leading icon. IconName::None hides it, which is also the default.
	virtual void setIcon(IconName);
	IconName getIcon() const;

	/* Whether the remove button is there at all. A chip without one is a label that happens to be
	in a set - which is what a fixed member of a chain looks like. */
	virtual void setRemovable(bool);
	bool isRemovable() const { return _removable; }

	virtual void setRemoveCallback(Callback &&);

	// A tap on the chip ITSELF, not on its button. ui::ChipRow selects with this.
	virtual void setTapCallback(Callback &&);

	// Paints `selected`. The row decides who wears it; the chip only wears it.
	virtual void setSelected(bool);
	bool isSelected() const { return _selected; }

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	/* The natural size, which is what the measurement protocol answers with. Shapes the label
	first: a Label reports zero width until it has been through an update, and a row that wrapped
	by that number would put every chip on one line. */
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
