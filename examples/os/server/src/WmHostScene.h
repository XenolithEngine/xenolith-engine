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

#ifndef EXAMPLES_OS_SERVER_SRC_WMHOSTSCENE_H_
#define EXAMPLES_OS_SERVER_SRC_WMHOSTSCENE_H_

#include "XL2dScene.h"
#include "WmServer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

// The host window's own scene. It draws nothing worth seeing: once presented, it starts the window
// manager, whose compositor takes the window's frames over. It stays to carry the inspector
// commands, and to get the window back if the compositor cannot have it.
class WmHostScene : public basic2d::Scene2d {
public:
	virtual ~WmHostScene() = default;

	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &) override;

	virtual void handlePresented(Director *) override;
	virtual void handleExit() override;

protected:
	using basic2d::Scene2d::init;

	virtual void describeQueue(QueueInfo &) override;

	Rc<WmServer> _server;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SERVER_SRC_WMHOSTSCENE_H_ */
