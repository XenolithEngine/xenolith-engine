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

#ifndef XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORSWAPCHAINPIPE_H_
#define XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORSWAPCHAINPIPE_H_

#include "XLCompositorPipe.h"
#include "XLCompositorQueue.h"
#include "XLCoreRenderSession.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

class SwapchainDisplayPipe;

// Draws the host window: every frame is the pipe's current snapshot, composed by the queue.
class SP_PUBLIC PipeRenderClient : public core::RenderClientChannel {
public:
	virtual ~PipeRenderClient() = default;

	bool init(SwapchainDisplayPipe *, Rc<core::Queue> &&);

	// The pipe let go of the window; frames still asked for are declined.
	void invalidate();

	const Rc<core::Queue> &getQueue() const { return _queue; }

	virtual void acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy>,
			Function<void(bool)> &&) override;

	virtual void handleRenderQueueAttached(const Rc<core::Queue> &) override { }
	virtual void handleConstraintsChanged(const core::FrameConstraints &) override { }
	virtual void handleInputEvents(uint64_t, Vector<core::InputEventData> &&) override;
	virtual void handleTextInput(uint64_t, const core::TextInputState &) override { }
	virtual void handleFramePresented(uint64_t frameOrder) override;
	virtual bool wantsFramePresented() const override { return true; }
	virtual void pushDrawStat(uint64_t, const core::DrawStat &) override { }

protected:
	SwapchainDisplayPipe *_pipe = nullptr;
	Rc<core::Queue> _queue;
	const core::AttachmentData *_planeSet = nullptr;
};

// A pipe whose output is an application window: the compositor becomes that window's render
// client, in place of its Director, the way a remote client takes a shared window over.
class SP_PUBLIC SwapchainDisplayPipe : public DisplayPipe {
public:
	virtual ~SwapchainDisplayPipe();

	bool init(NotNull<ServerAppThread>, NotNull<AppWindow> host);

	// Build and compile the compositor queue for the host's graphics API, then take the window
	// over. `cb` runs on the app thread once it is done or failed. ErrorNotSupported: no compositor
	// pass for this API.
	Status attach(Function<void(Status)> && = nullptr);

	// Give the window back to its Director.
	void detach();

	AppWindow *getHost() const { return _host; }
	bool isAttached() const { return _attached; }

	virtual Extent2 getExtent() const override;

protected:
	virtual void requestHostFrame() override;
	virtual bool hasOutput() const override { return _attached; }

	Rc<AppWindow> _host;
	Rc<PipeRenderClient> _client;
	bool _attaching = false;
	bool _attached = false;
};

} // namespace stappler::xenolith::compositor

#endif /* XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORSWAPCHAINPIPE_H_ */
