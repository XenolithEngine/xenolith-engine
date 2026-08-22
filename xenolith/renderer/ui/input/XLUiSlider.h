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

THE WIDGET CARRIES AN INDEX, NOT A FRACTION, and that is the whole design.

A slider that held 0..1 and multiplied would answer 0.5499999999999999 to a drag that landed exactly
on the middle notch, and two runs on two ABIs would disagree in the last place. Here the position
under the pointer is turned into a STEP INDEX and nothing else is stored; the fraction is computed
once, when the fill is drawn, and never read back. So `getValue()` is `min + step * index` - an
arithmetic expression over three declared numbers, identical everywhere - and a drag and an arrow
press that land on the same notch produce the same number rather than two close ones.

WHAT IT IS MADE OF, and none of it is new: three ui::Panels, the same shape as ui::ProgressBar -
the widget is the track, and it owns the fill and the handle. Everything visible comes from CSS; C++
writes geometry only. Give the track its size the way you would any other atom.

IT IS NOT A ui::ProgressBar AND DOES NOT DERIVE FROM ONE. The picture is nearly the same and the
widget is not: an indicator has no step, takes no focus, cannot be locked, and answers `nan` for
"unknown", which is not a thing a slider can mean. A shared silhouette is not a base class.

IT DOES NOT CARRY SystemManagedLayout, and that is deliberate rather than an omission. The handle
takes its size from CSS - a rule saying `slider-thumb { width:16px }` has to reach it - and under a
system-managed parent a child's declared size is routed into a MeasureComponent for the owner to
read instead of being committed. So this widget follows ui::Select: it POSITIONS its children in
handleContentSizeDirty, stands aside the moment a real LayoutSystem is present, and reads the
handle's size the way Select reads its icon's. The one geometry the widget writes outright is the
fill's, which no sheet has any reason to declare.

THE HANDLE HAS WIDTH, and the arithmetic respects it. The travel is `track - handle`, and the
handle's centre is `handle/2 + travel * fraction`, so at index 0 the handle sits inside the track
rather than half outside it, and the map from coordinate to index is exactly reversible.

AN UNREACHABLE MAXIMUM IS REPORTED, NOT TRIMMED. `setRange(0, 10, 3)` has four notches - 0, 3, 6, 9 -
and 10 is not one of them. The widget keeps the max it was given and lets `getValueAt(getMaxIndex())`
say that the end is not reachable; an owner that cares (a screen editor validating an author's
binding) can then say so in its own words. Silently moving the max to 9, or adding a notch at 10 that
is not a whole step from its neighbour, would take that away and put a number nobody wrote on screen.

KEYS ANSWER ALONG THE WIDGET'S OWN AXIS ONLY. A horizontal slider ignores Up/Down and a vertical one
ignores Left/Right: on a horizontal track "up" names no direction, and a control that guesses one
takes the key away from whatever beside it meant something by it. Home/End and PageUp/PageDown work
either way, because "the end" and "a big step" are unambiguous. Keys are answered only while the
widget is FOCUSED - inside a ui::FormSystem that works because the group passes events to listeners
at or below the focused field's node, and standalone because a tap takes focus and a tap outside
gives it up.

THE CALLBACK FIRES THROUGHOUT A DRAG, not at its end: a value that arrives only on release gives no
live feedback. As with ui::NumberField, grouping one drag into one history entry is the owner's job.

CSS: type `slider`, class `xl-ui-slider`; children `slider > slider-fill` and
`slider > slider-thumb`, each with its own type so a rule can tell them apart. Classes `vertical`
while the axis is vertical, `dragging` between press and release, plus `disabled` / `locked` from
ui::applyControlEnabled and ui::setEditLock. `:hover`, `:focus`, `:active` and `:disabled` come from
InteractiveComponent, as they do for ui::Select. All three parts are Panels, and a Panel with no
fill declared is an opaque WHITE surface - so all three need a colour.

    slider              { width:220px; height:20px; }
    slider-fill         { background-color:#FCB400; border-radius:2px; }
    slider-thumb        { width:16px; height:16px; border-radius:8px; background-color:#E8E8E8; }
    slider:focus > slider-thumb { background-color:#FCB400; } */
class SP_PUBLIC Slider : public Panel, public EditLockTarget {
public:
	// The index now chosen. The value is `getValue()`; the index is what MOVED.
	using Callback = Function<void(int64_t index)>;

	// What PageUp / PageDown are worth, in steps.
	static constexpr uint32_t DefaultPageSteps = 10;

	virtual ~Slider();

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	/* Declare the scale. `step` must be > 0 and `max` >= `min`, or nothing moves and this answers
	false - a range nobody can express is a programming mistake, not a value.

	The current index is kept where it still exists and clamped to the new end otherwise: a widget
	whose scale changed under it must not report a notch that is no longer there. */
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

	/* The NEAREST notch to `value`. A value exactly between two of them goes to the HIGHER one,
	always - not "away from zero", which is what a plain round() would do and which would tie
	upward on a positive range and downward on a range that straddles or sits below zero. Where the
	scale happens to sit must not change what the widget does with the same input. */
	virtual void setValue(double, bool silent = false);

	/* Whether the value is a whole number - DECLARED, exactly as ui::NumberField declares it, and
	never inferred from min and step happening to be integral. A float-typed field with a step of 1
	is still a float field, and a form that collected it as an integer would have changed the value
	on its way out. */
	virtual void setInteger(bool);
	bool isInteger() const { return _integer; }

	// The axis. Vertical grows UPWARD: this is a level, not a scrollbar.
	virtual void setVertical(bool);
	bool isVertical() const { return _vertical; }

	virtual void setPageSteps(uint32_t);
	uint32_t getPageSteps() const { return _pageSteps; }

	virtual void setEnabled(bool);
	bool isEnabled() const override { return isControlEnabled(this); }

	virtual void setCallback(Callback &&cb) { _callback = sp::move(cb); }

	// Focus in the widget's own terms. A ui::FormSystem drives these through FormFieldSlots;
	// standalone, this widget's own listeners do.
	virtual void focus();
	virtual void blur();
	bool isFocused() const { return _focused; }

	/* Told when the widget takes focus BY ITSELF - a tap on the track. A form needs this or it goes
	on filtering keys to the field it focused last, and the arrows die in the widget the user just
	clicked. The widget cannot ask for it: forms/ knows about input/ and never the other way round.
	The same seam ui::ColorField and ui::ChipRow need, for the same reason. */
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
