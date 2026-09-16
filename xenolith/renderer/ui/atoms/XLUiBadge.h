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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUIBADGE_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUIBADGE_H_

#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Status pill: a rounded Panel that hosts a single centred Label. Variant styling (installed /
// not-installed / warning …) is driven by an app-defined style class added via setVariant().
// CSS:
//   badge { background-color:#292929; border-radius:5px; padding:0 12px; height:22px;
//           display:flex; align-items:center; justify-content:center; }
//   badge > label { color:#B8B8B8; font-size:12px; text-align:center; }
//   badge.installed { background-color: rgba(252,180,0,0.2); }
//   badge.installed > label { color:#FCB400; }
class SP_PUBLIC Badge : public Panel {
public:
	virtual ~Badge();

	virtual bool init() override;

	virtual void setText(StringView);
	virtual StringView getText() const;

	// Adds the style class `cls` (and removes the previous variant, if any). The app's CSS keys the
	// colours off this class (e.g. "installed", "not-installed", "warning").
	virtual void setVariant(StringView cls);

protected:
	basic2d::Label *_label = nullptr;
	String _variant;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUIBADGE_H_
