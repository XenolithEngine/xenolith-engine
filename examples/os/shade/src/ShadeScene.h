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


#ifndef EXAMPLES_OS_SHADE_SRC_SHADESCENE_H_
#define EXAMPLES_OS_SHADE_SRC_SHADESCENE_H_

#include "WmClient.h"
#include "XL2dScene.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class Panel;
class Button;

} // namespace stappler::xenolith::ui

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class Label;
class Layer;

} // namespace stappler::xenolith::basic2d

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

/* The shade: a transparent plane over the whole screen. It draws the status bar at the top - the
time, the server's numbers, Home and Apps - and, pulled down, a panel with the running applications
over a dimmed screen. It tells the server where it takes input (the bar, or everything while the
panel is out) and when it wants the keyboard. */
class ShadeScene : public basic2d::Scene2d {
public:
	virtual ~ShadeScene() = default;

	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &) override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	void open();
	void close();

protected:
	using Scene2d::init;

	struct Row {
		protocol::RunningApp app;
		ui::Panel *row = nullptr;
		ui::Button *title = nullptr;
		ui::Button *close = nullptr;
	};

	virtual void describeQueue(QueueInfo &) override;

	void handleMessage(Value &&);
	void setApps(Vector<protocol::RunningApp> &&);
	void setStatus(const protocol::Stats &);
	void updateClock();
	void scheduleClock();
	void requestApps();

	// The bar, or the whole window: in the window's pixels, y down.
	void sendInputRegion(bool whole);

	Vec2 getPanelPosition(bool opened) const;
	void registerCommands();

	Rc<WmClient> _client;
	ui::Panel *_bar = nullptr;
	basic2d::Label *_clock = nullptr;
	basic2d::Label *_indicators = nullptr;
	ui::Button *_home = nullptr;
	ui::Button *_apps = nullptr;
	basic2d::Layer *_backdrop = nullptr;
	ui::Panel *_panel = nullptr;
	Vector<Row> _rows;
	protocol::Stats _stats;
	bool _opened = false;
	Rc<sprt::dispatch::Handle> _clockTimer;
	Rc<sprt::dispatch::Handle> _retry;
	uint32_t _attempts = 0;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SHADE_SRC_SHADESCENE_H_ */
