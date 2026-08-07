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

#ifndef TESTS_AUXUI_SRC_AUXTOOLTIPSCENE_H_
#define TESTS_AUXUI_SRC_AUXTOOLTIPSCENE_H_

#include "AuxBaseScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Scene presented inside a Tooltip window, on the rare path where one is materialized natively.
// The engine's default is an in-scene tip (SubWindow::showTooltip sets preferNative=false).
class AuxTooltipScene : public AuxBaseScene {
public:
	static Rc<AuxTooltipScene> create(NotNull<AppThread> app,
			NotNull<core::RenderServerChannel> window, const core::FrameConstraints &constraints,
			NotNull<ui::SubWindow> subWindow) {
		auto ret = Rc<AuxTooltipScene>::create();
		if (ret && ret->init(app, window, constraints, subWindow)) {
			return ret;
		}
		return nullptr;
	}

protected:
	virtual Rc<basic2d::SceneLayout2d> buildContent() override;

	virtual void handlePresented(Director *) override;
};

} // namespace stappler::xenolith::app

#endif // TESTS_AUXUI_SRC_AUXTOOLTIPSCENE_H_
