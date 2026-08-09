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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUICHECKBOX_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUICHECKBOX_H_

#include "XLUiPanel.h"
#include "XLInputListener.h"
#include "XL2dIconSprite.h" // the check mark held below

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// A two-state toggle: a rounded box (Panel) hosting a check-mark icon. Tapping flips the state,
// toggles the "checked" style class (so CSS can swap fill/icon colour), and fires the callback.
// CSS:
//   checkbox { width:17px; height:17px; border-radius:4px;
//              background-color:#292929; border:1px solid rgba(255,255,255,0.3); }
//   checkbox.checked { background-color:#FCB400; border:1px solid #FCB400; }
//   checkbox > icon { width:14px; height:14px; color:#1A1A1A; }
class SP_PUBLIC Checkbox : public Panel {
public:
	using Callback = Function<void(bool)>;

	virtual ~Checkbox();

	virtual bool init() override;

	virtual void setChecked(bool c, bool silent = false);
	virtual bool isChecked() const { return _checked; }

	virtual void setEnabled(bool e);
	virtual bool isEnabled() const { return _enabled; }

	virtual void setCallback(Callback &&cb) { _callback = sp::move(cb); }

protected:
	bool _checked = false;
	bool _enabled = true;
	Callback _callback;
	InputListener *_listener = nullptr;
	basic2d::IconSprite *_check = nullptr;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUICHECKBOX_H_
