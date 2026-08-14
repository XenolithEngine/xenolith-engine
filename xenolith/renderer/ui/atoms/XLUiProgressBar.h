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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUIPROGRESSBAR_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUIPROGRESSBAR_H_

#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A determinate progress bar built out of two Panels: the widget itself is the track, and it owns
one child that is the filled part.

Two Panels rather than one node painted from code, because that is what keeps the colours in the
stylesheet. basic2d::LinearProgress takes its palette through setLineColor()/setBarColor(), so an
application using it has to name colours in C++; here C++ writes exactly one number - the fraction -
and everything visible comes from CSS.

The widget places its own child, so it carries SystemManagedLayout: a stylesheet must not add a
second writer of that geometry. Give the track its size the way you would any other atom (a fixed
height plus flex-grow, a width, a grid cell).

CSS: the widget is type "progress-bar" and the fill is a child of type "progress-fill" (both
Panels, so both take background-color / outline / border-radius - and note that a Panel with no
fill declared is an opaque WHITE surface, so both need a colour). An indeterminate bar carries the
`indeterminate` style class and draws NO fill at all: there is no honest fraction to show, and a
full-looking bar would claim one. Style the track itself for that state.

  progress-bar   { height:4px; border-radius:2px; background-color:#292929; }
  progress-fill  { border-radius:2px; background-color:#FCB400; }
  progress-bar.indeterminate { background-color:#3a3a3a; } */
class SP_PUBLIC ProgressBar : public Panel {
public:
	virtual ~ProgressBar();

	virtual bool init() override;
	virtual bool init(float progress);

	virtual void handleContentSizeDirty() override;

	// Clamped into [0, 1]. Pass nan() when the total is unknown - `cloneEngine` reports bytes
	// received and no total, and a bar that invented one would be lying. See isIndeterminate().
	virtual void setProgress(float);
	float getProgress() const { return _progress; }

	// True while the progress is nan(): the fill is hidden and the track carries `indeterminate`.
	bool isIndeterminate() const;

	Panel *getFill() const { return _fill; }

protected:
	using Panel::init;

	Panel *_fill = nullptr;
	float _progress = 0.0f;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUIPROGRESSBAR_H_
