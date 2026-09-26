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


#ifndef EXAMPLES_OS_SHELL_SRC_SHELLSCENE_H_
#define EXAMPLES_OS_SHELL_SRC_SHELLSCENE_H_

#include "WmClient.h"
#include "XL2dScene.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class Panel;
class Button;

} // namespace stappler::xenolith::ui

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

/* The launcher: a tile per application of the server's catalog. A tap launches it; an application
that is not built is shown, but cannot be tapped. */
class ShellScene : public basic2d::Scene2d {
public:
	virtual ~ShellScene() = default;

	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &) override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

protected:
	using Scene2d::init;

	struct Tile {
		protocol::CatalogEntry entry;
		ui::Button *button = nullptr;
	};

	virtual void describeQueue(QueueInfo &) override;

	void requestCatalog();
	void setCatalog(Vector<protocol::CatalogEntry> &&);
	void registerCommands();

	Rc<WmClient> _client;
	ui::Panel *_grid = nullptr;
	Vector<Tile> _tiles;
	Rc<sprt::dispatch::Handle> _retry;
	uint32_t _attempts = 0;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SHELL_SRC_SHELLSCENE_H_ */
