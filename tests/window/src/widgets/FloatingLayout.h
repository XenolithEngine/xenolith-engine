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

#ifndef TESTS_WINDOW_SRC_WIDGETS_FLOATINGLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_FLOATINGLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiFloatingSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* ui::FloatingSystem: a window inside the scene over a busy background.

  * `#below` fills the work area and counts taps, wheel turns and hover, and carries a hint - every
    kind of pointer answer the floating window must hide where it is drawn;
  * `#plate` is an opaque ui::Panel under part of the window: a Panel is a Surface, drawn by
    material, so only the overlay pass puts the window over it;
  * `#window` floats: `#header` moves it and holds `#header-button`, `#inside` counts the taps and
    wheel turns that reach the content. */
class FloatingLayout : public TestLayout {
public:
	static constexpr Rect InitialFrame = Rect(200.0f, 200.0f, 360.0f, 240.0f);
	static constexpr float HeaderHeight = 28.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;
	void placeWindowContent();

	Node *_below = nullptr;
	Node *_plate = nullptr;
	Node *_window = nullptr;
	Node *_header = nullptr;
	Node *_headerButton = nullptr;
	Node *_inside = nullptr;
	ui::FloatingSystem *_floating = nullptr;

	uint32_t _belowTaps = 0;
	uint32_t _belowScrolls = 0;
	bool _belowHovered = false;
	uint32_t _insideTaps = 0;
	uint32_t _insideScrolls = 0;
	uint32_t _headerTaps = 0;
	uint32_t _frameChanges = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_FLOATINGLAYOUT_H_
