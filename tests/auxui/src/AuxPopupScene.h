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

#ifndef TESTS_AUXUI_SRC_AUXPOPUPSCENE_H_
#define TESTS_AUXUI_SRC_AUXPOPUPSCENE_H_

#include "AuxBaseScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class AuxPopupScene : public AuxBaseScene {
public:
	// Level 1 is the menu opened from Root; every "More" opens the next one. The deepest level
	// has no "More", so the chain terminates at kMaxLevel nested native Popup windows.
	static constexpr uint32_t kMaxLevel = 4;

	static Rc<AuxPopupScene> create(NotNull<AppThread> app,
			NotNull<core::RenderServerChannel> window, const core::FrameConstraints &constraints,
			NotNull<ui::SubWindow> subWindow, uint32_t level) {
		auto ret = Rc<AuxPopupScene>::create();
		if (ret && ret->init(app, window, constraints, subWindow, level)) {
			return ret;
		}
		return nullptr;
	}

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints, NotNull<ui::SubWindow>, uint32_t level);

	Rc<basic2d::SceneLayout2d> buildMenuPanel(uint32_t level);

	// Window size a menu at `level` needs — the parent has to pass it to createWindow before the
	// child scene exists, so the geometry has to be computable from the level alone.
	static Size2 getMenuSize(uint32_t level);

protected:
	using AuxBaseScene::init;

	virtual Rc<basic2d::SceneLayout2d> buildContent() override;

	virtual void handlePresented(Director *) override;

	// Open the next level as a separate native Popup parented to this window.
	void openSubmenu();

	// AUXUI_HOVER_STRESS: recreate (and optional refresh) flaps of the tip on this menu.
	// Driven by Scene Sequences (not a bare looper timer) so the step state stays alive.
	void runHoverStress(bool alsoRefresh);

	basic2d::SceneLayout2d *_menuLayout = nullptr;

	// The submenu this menu opened, if any. Held so the chain has an owner other than the window
	// system - dismissing this level takes its children with it.
	Rc<ui::SubWindow> _childMenu;

	uint32_t _level = 1;
};

} // namespace stappler::xenolith::app

#endif // TESTS_AUXUI_SRC_AUXPOPUPSCENE_H_
