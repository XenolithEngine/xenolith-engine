/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_WIDGETS_SLIDERLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SLIDERLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiSlider.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::Slider: a value picked by sliding.
//
// Five sliders, and each one exists for a single claim the others cannot make:
//
//   * `steps`      0..100 by 5, integer - the reference. Twenty-one notches, and every value is
//                  exactly 5*i, so a drag and an arrow press can be compared for EQUALITY;
//   * `real`       0..1 by 0.25 - the fraction that is exactly representable. A step of 0.1 would
//                  make 0.1*6 come out as 0.6000000000000001 and force the check to compare with a
//                  tolerance, which would no longer be checking "the widget carries an index";
//   * `unreachable` 0..10 by 3 - four notches ending at 9. The maximum the author declared is NOT
//                  a whole number of steps away, and the widget must say so rather than trim it;
//   * `vertical`   the other axis, growing upward, answering Up/Down and not Left/Right;
//   * `form`       inside a ui::FormSystem, which is what collects a VALUE rather than an index.
//
// Nothing here is visible in a screenshot: an index of 10 and an index of 11 differ by five points
// of handle position, and a value that was clamped looks exactly like one that was chosen.
class SliderLayout : public TestLayout {
public:
	// Track geometry, duplicated by slider-check.py. A check that reads its expectations out of the
	// thing it is checking cannot fail, so these numbers are written twice on purpose.
	static constexpr float TrackWidth = 220.0f;
	static constexpr float TrackHeight = 20.0f;
	static constexpr float ThumbSize = 16.0f;
	static constexpr float RowStride = 56.0f;
	static constexpr float RowLeft = 48.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeSlider(ui::Slider *) const;
	Value encodeState() const;

	ui::Slider *getTarget(const Value &args) const;

	ui::Slider *_steps = nullptr;
	ui::Slider *_real = nullptr;
	ui::Slider *_unreachable = nullptr;
	ui::Slider *_vertical = nullptr;
	ui::Slider *_form = nullptr;
	ui::FormSystem *_formSystem = nullptr;

	// Per slider, so "the callback did not fire" is a statement about the slider it is made about
	// rather than about the stand as a whole.
	Map<String, uint32_t> _callbacks;
	Map<String, int64_t> _lastIndex;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SLIDERLAYOUT_H_
