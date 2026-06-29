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

#ifndef EXAMPLES_VK_GUI_SRC_SHAPINGLAYOUT_H_
#define EXAMPLES_VK_GUI_SRC_SHAPINGLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Visual test bed for HarfBuzz shaping (stages 3-4): rows of text rendered with shaping/bidi off vs
// on, so the shaped glyphs (kerning, ligatures, Arabic joining, RTL order) can be eyeballed from a
// screenshot. Reachable from the GeneralLayout menu, or directly via the XL_SHAPING_TEST env var.
class ShapingLayout : public basic2d::SceneLayout2d {
public:
	virtual ~ShapingLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	basic2d::Layer *_background = nullptr;
	Vector<basic2d::Label *> _rows;
};

} // namespace stappler::xenolith::app

#endif // EXAMPLES_VK_GUI_SRC_SHAPINGLAYOUT_H_
