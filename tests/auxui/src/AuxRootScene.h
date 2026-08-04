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

#ifndef TESTS_AUXUI_SRC_AUXROOTSCENE_H_
#define TESTS_AUXUI_SRC_AUXROOTSCENE_H_

#include "XL2dScene.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XLSimpleButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

namespace app {

// Root panel of the scaffold: opens Popup/Tooltip via ui::AuxSession
// (native when Subwindows is advertised, in-scene overlay otherwise).
class AuxRootScene : public basic2d::Scene2d {
public:
	static Rc<AuxRootScene> create(NotNull<AppThread> app,
			NotNull<core::RenderServerChannel> window, const core::FrameConstraints &constraints,
			StringView id) {
		auto ret = Rc<AuxRootScene>::create();
		if (ret && ret->init(app, window, constraints, id)) {
			return ret;
		}
		return nullptr;
	}

	virtual ~AuxRootScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints, StringView id);

protected:
	virtual void handlePresented(Director *) override;
	virtual void buildQueueResources(QueueInfo &, core::Queue::Builder &) override;

	void layoutRootPanel();
	void openMenuAt(Vec2 anchorWorld);
	void openTooltipAt(Vec2 anchorWorld, StringView text);
	void dismissTooltip();

	basic2d::Label *_heading = nullptr;
	basic2d::Layer *_bg = nullptr;
	simpleui::ButtonWithLabel *_btnPopup = nullptr;
	simpleui::ButtonWithLabel *_btnTooltip = nullptr;

	AppWindow *_appWindow = nullptr;
	String _myWindowId;
	bool _hoverArmed = false;
	static constexpr uint32_t kHeadingHoverTipTag = 0x41555831; // 'AUX1'
};

} // namespace app
} // namespace stappler::xenolith

#endif // TESTS_AUXUI_SRC_AUXROOTSCENE_H_
