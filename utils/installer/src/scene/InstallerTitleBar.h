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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERTITLEBAR_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERTITLEBAR_H_

#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Client-side window decorations: the OS buttons, the app icon, and the draggable title strip.
// Order and per-platform arrangement (the macOS traffic-light cluster) live in
// resources/style.css — this class only creates the parts and names them.
// Derives from Layer so `#title-bar { background-color }` actually paints (a plain Node
// ignores fill — any inset around the title strip would show the window backdrop).
class TitleBar : public basic2d::Layer {
public:
	virtual ~TitleBar();

	virtual bool init() override;

protected:
	Node *_osMaximize = nullptr;
	Node *_osMinimize = nullptr;
	Node *_osClose = nullptr;
	Node *_osMenu = nullptr;
	Node *_osIcon = nullptr;
	Node *_profile = nullptr;
	Node *_titleLine = nullptr;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERTITLEBAR_H_
