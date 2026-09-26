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

#include "XLCompositorSwapchainPipe.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLServerAppThread.h"
#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameRequestProxy.h"

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_SOFT
#include "XLCompositorSoftPass.h"
#endif

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_VK
#include "XLCompositorVkPass.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

bool PipeRenderClient::init(SwapchainDisplayPipe *pipe, Rc<core::Queue> &&queue) {
	_pipe = pipe;
	_queue = sp::move(queue);
	_planeSet = _queue->getAttachment(PlaneSetAttachmentName);
	if (!_planeSet) {
		log::source().error("compositor::PipeRenderClient", "queue '", _queue->getName(),
				"' has no '", PlaneSetAttachmentName, "' attachment");
		return false;
	}
	return true;
}

void PipeRenderClient::invalidate() { _pipe = nullptr; }

void PipeRenderClient::acquireFrame(uint64_t, NotNull<core::FrameRequestProxy> proxy,
		Function<void(bool)> &&cb) {
	auto pipe = _pipe;
	if (!pipe) {
		// The queue is never selected, so the frame drops itself.
		cb(false);
		return;
	}

	auto &snapshot = pipe->getSnapshot();

	auto input = Rc<PlaneSetInput>::alloc();
	input->background = snapshot->background;
	input->snapshot = snapshot->serial;
	for (auto &it : snapshot->planes) {
		// Before its first frame a plane shows nothing, and the background shows through.
		if (auto frame = it.plane->getSource()->getLatest()) {
			input->planes.emplace_back(PlaneInput{sp::move(frame), it.state.src, it.state.dst,
				it.state.alpha, it.state.blend, it.plane->getId()});
		}
	}

	proxy->selectQueue(_queue);

	// The window always wraps its request in a LocalFrameRequestProxy. Inputs are added on the gapi
	// loop thread, as on every other path; every frame gets one, an empty plane set included.
	auto request = Rc<core::FrameRequest>(
			static_cast<core::LocalFrameRequestProxy *>(proxy.get())->getRequest());
	if (auto loop = pipe->getApplication()->getGlLoop()) {
		loop->performOnThread([request, attachment = _planeSet, input]() mutable {
			request->addInput(attachment, sp::move(input));
		}, this);
	}

	cb(true);
}

void PipeRenderClient::handleFramePresented(uint64_t frameOrder) {
	if (_pipe) {
		_pipe->handleHostPresented(frameOrder);
	}
}

SwapchainDisplayPipe::~SwapchainDisplayPipe() {
	if (_client) {
		_client->invalidate();
	}
}

bool SwapchainDisplayPipe::init(NotNull<ServerAppThread> app, NotNull<AppWindow> host) {
	if (!DisplayPipe::init(app)) {
		return false;
	}
	_host = host.get();
	return true;
}

Status SwapchainDisplayPipe::attach(Function<void(Status)> &&cb) {
	if (_attached || _attaching) {
		return Status::ErrorAlreadyPerformed;
	}

	auto loop = _app->getGlLoop();
	if (!loop || !loop->getInstance()) {
		return Status::ErrorInvalidArguemnt;
	}

	core::Queue::Builder builder("Compositor");
	bool built = false;

	[[maybe_unused]]
	CompositorQueueInfo info{loop, getExtent(), getBackground()};

	PlaneCaps caps;

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_SOFT
	if (!built && loop->getInstance()->getApi() == core::InstanceApi::Software) {
		built = soft::CompositorPass::makeRenderQueue(builder, info);
	}
#endif

#if MODULE_XENOLITH_RENDERER_COMPOSITOR_VK
	if (!built && loop->getInstance()->getApi() == core::InstanceApi::Vulkan) {
		built = vk::CompositorPass::makeRenderQueue(builder, info);
		caps = vk::CompositorPass::getCaps();
	}
#endif

	if (!built) {
		return Status::ErrorNotSupported;
	}

	setCaps(caps);

	auto queue = Rc<core::Queue>::create(sp::move(builder));
	if (!queue) {
		return Status::ErrorInvalidArguemnt;
	}

	_client = Rc<PipeRenderClient>::create(this, Rc<core::Queue>(queue));
	if (!_client) {
		return Status::ErrorInvalidArguemnt;
	}

	_attaching = true;
	_host->compileRenderQueue(queue,
			[this, app = Rc<ServerAppThread>(_app), cb = sp::move(cb)](bool success) mutable {
		app->performOnAppThread([this, success, cb = sp::move(cb)]() mutable {
			_attaching = false;
			if (!success || !_client) {
				log::source().error("compositor::SwapchainDisplayPipe",
						"fail to compile the compositor queue");
				if (cb) {
					cb(Status::ErrorInvalidArguemnt);
				}
				return;
			}

			// As a remote client takes a shared window over: the window's frames are ours now.
			_host->setRenderClient(_client);
			_host->resetForRenderClientChange();
			_attached = true;

			scheduleHostFrame();
			if (cb) {
				cb(Status::Ok);
			}
		}, this);
	});

	return Status::Ok;
}

void SwapchainDisplayPipe::detach() {
	if (!_attached) {
		return;
	}

	_attached = false;
	_client->invalidate();
	if (_host->getRenderClient() == _client.get()) {
		_host->setRenderClient(_host->getDirector());
		_host->resetForRenderClientChange();
	}
}

Extent2 SwapchainDisplayPipe::getExtent() const {
	auto &extent = _host->getConstraints().extent;
	return Extent2(extent.width, extent.height);
}

void SwapchainDisplayPipe::requestHostFrame() { _host->setReadyForNextFrame(); }

} // namespace stappler::xenolith::compositor
