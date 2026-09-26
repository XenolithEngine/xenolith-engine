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


#ifndef EXAMPLES_OS_SERVER_SRC_WMSURFACESCENE_H_
#define EXAMPLES_OS_SERVER_SRC_WMSURFACESCENE_H_

#include "XL2dScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

/* The server's scene for a client's window. It only lends the window a Flat queue: once presented,
the queue is shared with the session and the client draws through it. A transparent surface (the
shade) gets a premultiplied queue cleared to nothing, so the planes below show through.

Every frame the scene itself presents is reported: the window manager tells the client's first
frame from the frames before it by that count. */
class WmSurfaceScene : public basic2d::Scene2d {
public:
	virtual ~WmSurfaceScene() = default;

	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &, StringView label, bool transparent,
			Function<void()> &&onPresented);

	virtual void handlePresented(Director *) override;

protected:
	using basic2d::Scene2d::init;

	virtual void describeQueue(QueueInfo &) override;

	bool share();

	String _label;
	bool _transparent = false;
	bool _shared = false;
	Function<void()> _onPresented;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SERVER_SRC_WMSURFACESCENE_H_ */
