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

#ifndef TESTS_WINDOW_SRC_WINDOW_GEOMETRYLAYOUT_H_
#define TESTS_WINDOW_SRC_WINDOW_GEOMETRYLAYOUT_H_

#include "app/TestLayout.h"
#include "XLWindowSceneInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The window geometry protocol, both halves of it.
//
// READING: what the scene sees through Director::getRenderServer()->getWindowGeometry(), and
// whether Scene::handleWindowGeometryChanged fires when the window changes. Both are reported by
// `geometry.state`, which is the whole assertion surface.
//
// RESTORING: the layout opens a second Root window at a REQUESTED position and the check reads that
// window's own geometry back. That round trip - a rect put into WindowInfo::rect and read out again
// through the protocol - is what "the geometry can be restored" means, and it is exactly what a
// window that saves its position across runs does.
//
// Headless honours a requested position outright, so the round trip is exact there. Under a real
// window manager it is a hint, and the check treats it as one.
class GeometryLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleExit() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;

	// The geometry of the secondary window, read from ITS window rather than from ours.
	Value encodeSecondary() const;

	AppWindow *getAppWindow() const;

	basic2d::Label *_report = nullptr;
	Rc<WindowSceneInfo> _secondWindow;

	// What the second window was asked for. The check compares the readback against this.
	IVec2 _requestedOrigin = IVec2{137, 89};
	Extent2 _requestedSize = Extent2(480, 320);
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WINDOW_GEOMETRYLAYOUT_H_
