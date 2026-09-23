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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUISLIDER_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUISLIDER_H_

#include "XLUiPanel.h"
#include "XLUiControlLock.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A value picked by sliding: a track, the part of it behind the handle, and the handle.

The widget stores a step index, not a fraction: `getValue()` is `min + step * index`, so a drag and
a key press landing on the same notch give the same exact number. The fraction is only used to draw.

The widget is the track and owns two ui::Panels, the fill and the handle; everything visible comes
from CSS. It does not use SystemManagedLayout, so the handle's CSS size is committed: children are
positioned in handleContentSizeDirty unless a LayoutSystem is present, and only the fill's geometry
is written outright.

The travel is `track - handle` and the handle's centre is `handle/2 + travel * fraction`, so the
handle stays inside the track and the coordinate-to-index map is exactly reversible.

A max that is not a whole number of steps from min is kept, not trimmed: `setRange(0, 10, 3)` has
notches 0, 3, 6, 9, and `getValueAt(getMaxIndex())` reports that 10 is unreachable.

Arrow keys work only along the widget's axis; Home/End and PageUp/PageDown work on both. Keys are
answered only while focused (via ui::FormSystem, or tap to focus and tap outside to blur).

The callback fires on every step during a drag; grouping a drag into one history entry is the
owner's job.

CSS: type `slider`, class `xl-ui-slider`; children `slider > slider-fill` and
`slider > slider-thumb`, each with its own type so a rule can tell them apart. Classes `vertical`
while the axis is vertical, `dragging` between press and release, plus `disabled` / `locked` from
ui::applyControlEnabled and ui::setEditLock. `:hover`, `:focus`, `:active` and `:disabled` come from
InteractiveComponent, as they do for ui::Select. All three parts are Panels, and a Panel with no
fill declared is opaque white, so all three need a colour.

The fill and the handle carry the widget's own interactive state, so a sheet paints them through
their own pseudo-classes (`slider-thumb:hover`); a state on the widget does not reach them.

    slider              { width:220px; height:20px; }
    slider-fill         { background-color:#FCB400; border-radius:2px; }
    slider-thumb        { width:16px; height:16px; border-radius:8px; background-color:#E8E8E8; }
    slider-thumb:hover  { background-color:#FFFFFF; }
    slider-thumb:focus  { background-color:#FCB400; } */
class SP_PUBLIC Slider : public Panel, public EditLockTarget {
public:
	// The index now chosen; the value is `getValue()`.
	using Callback = Function<void(int64_t index)>;

	// PageUp / PageDown size, in steps.
	static constexpr uint32_t DefaultPageSteps = 10;

	virtual ~Slider();

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	/* Declare the scale. Returns false and changes nothing unless `step` > 0 and `max` >= `min`.
	The current index is kept if still in range, clamped to the new end otherwise. */
	virtual bool setRange(double min, double max, double step);

	double getMin() const { return _min; }
	double getMax() const { return _max; }
	double getStep() const { return _step; }

	// The last notch: floor((max - min) / step). Indices run [0, getMaxIndex()].
	int64_t getMaxIndex() const { return _maxIndex; }

	virtual void setIndex(int64_t, bool silent = false);
	int64_t getIndex() const { return _index; }

	// min + step * index, for the current index and for any index in range.
	double getValue() const { return getValueAt(_index); }
	double getValueAt(int64_t index) const;

	// The nearest notch to `value`. Ties always go to the higher notch, regardless of sign.
	virtual void setValue(double, bool silent = false);

	// Whether the value is a whole number. Declared, as in ui::NumberField, never inferred from an
	// integral min and step.
	virtual void setInteger(bool);
	bool isInteger() const { return _integer; }

	// The axis. Vertical grows upward (the minimum is at the bottom).
	virtual void setVertical(bool);
	bool isVertical() const { return _vertical; }

	virtual void setPageSteps(uint32_t);
	uint32_t getPageSteps() const { return _pageSteps; }

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	virtual void setCallback(Callback &&cb) { _callback = sp::move(cb); }

	// Focus in the widget's own terms. A ui::FormSystem drives these through FormFieldSlots;
	// standalone, this widget's own listeners do.
	virtual void focus();
	virtual void blur();
	bool isFocused() const { return _focused; }

	/* Told when the widget takes or loses focus by itself (a tap), so a form can move its key
	routing here. input/ cannot depend on forms/, hence the callback. */
	virtual void setFocusCallback(Function<void(bool)> &&cb) { _focusCallback = sp::move(cb); }

	// True between the press and the release of a drag.
	bool isDragging() const { return _dragging; }

	// Points of travel available to the handle: the track's length along the axis, less the
	// handle's own. Zero until both have been measured.
	float getTravel() const;

	Panel *getFill() const { return _fill; }
	Panel *getThumb() const { return _thumb; }

protected:
	using Panel::init;

	// The notch nearest a point in this node's own space, clamped into range.
	int64_t indexForLocation(const Vec2 &) const;

	// index + delta, clamped. Returns whether the index moved.
	bool step(int64_t delta);

	bool handleKey(const GestureData &);
	bool handleDragBegin(const Vec2 &location);
	void handleDragMove(const Vec2 &location);
	void handleDragEnd();

	virtual void updateGeometry();
	virtual void updateInteractiveState();

	// Give one part (the fill, the handle) the widget's interactive state as its own.
	void updatePartState(Panel *, InteractiveState);

	Panel *_fill = nullptr;
	Panel *_thumb = nullptr;

	InputListener *_listener = nullptr;
	InputListener *_focusListener = nullptr;

	Callback _callback;
	Function<void(bool)> _focusCallback;

	double _min = 0.0;
	double _max = 1.0;
	double _step = 1.0;
	int64_t _maxIndex = 1;
	int64_t _index = 0;

	uint32_t _pageSteps = DefaultPageSteps;

	bool _integer = false;
	bool _vertical = false;
	bool _focused = false;
	bool _dragging = false;

	/* Guards updateGeometry() against the write it makes to the handle's own ContentSize being
	read back as a reason to run again. */
	bool _inGeometry = false;

	// Edge trackers for InteractiveComponent's cumulative counters.
	bool _hoverApplied = false;
	bool _focusApplied = false;
	bool _activeApplied = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUISLIDER_H_
