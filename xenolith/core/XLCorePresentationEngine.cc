/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLCorePresentationEngine.h"
#include "XLCoreImageStorage.h"
#include "XLCorePresentationFrame.h"
#include "XLCoreSwapchain.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreDevice.h"
#include "SPEventLooper.h"
#include "SPEventTimerHandle.h"

#define XL_COREPRESENT_DEBUG 0

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

#if XL_COREPRESENT_DEBUG
#define XL_COREPRESENT_LOG(...) log::source().debug("core::PresentationEngine", __VA_ARGS__)
#else
#define XL_COREPRESENT_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool PresentationEngine::isFrameValid(const PresentationFrame *frame) const {
	if (frame->getSwapchain() == _swapchain && !_swapchain->isDeprecated()) {
		return true;
	}
	return false;
}

Rc<FrameHandle> PresentationEngine::submitNextFrame(Rc<FrameRequest> &&req) {
	auto frame = _loop->makeFrame(move(req), 0);
	if (frame && frame->isValidFlag()) {
		frame->update(true);
		return frame;
	}
	return nullptr;
}

bool PresentationEngine::waitUntilFramePresentation() {
	if (!_loop->isOnThisThread()) {
		return false;
	}

	if (_waitUntilFrame) {
		return false;
	}

	if (_swapchain) {
		_waitUntilFrame = true;
		_nextPresentWindow = 0;
		setReadyForNextFrame();
		auto ret = _loop->getLooper()->run();
		_waitUntilFrame = false;
		return ret == Status::Suspended;
	}
	return false;
}

void PresentationEngine::scheduleNextImage(Function<void(PresentationFrame *, bool)> &&cb,
		PresentationFrame::Flags frameFlags) {
	if (!_activeFrames.empty() || !_swapchain || _swapchain->isDeprecated()) {
		return;
	}

	if (_options.followDisplayLinkBarrier && _waitForDisplayLink) {
		return;
	}

	XL_COREPRESENT_LOG("scheduleNextImage");

	if (_options.renderImageOffscreen) {
		frameFlags = PresentationFrame::OffscreenTarget;
	} else {
		frameFlags = PresentationFrame::None;
	}

	if (scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder,
				frameFlags, sp::move(cb)))) {
		_readyForNextFrame = false;
		_waitForDisplayLink = true;
	}
}

bool PresentationEngine::scheduleSwapchainImage(Rc<PresentationFrame> &&frame) {
	if (!frame) {
		return false;
	}

	XL_COREPRESENT_LOG("scheduleSwapchainImage");

	acquireFrameData(frame, [this](core::PresentationFrame *frame) mutable {
		if (isRunning() && frame->getSwapchain() == _swapchain) {
			XL_COREPRESENT_LOG("scheduleSwapchainImage: setup frame request");
			auto a = frame->setupOutputAttachment();
			if (!a) {
				if (frame->getRequest()->getQueue()) {
					log::source().error("core::PresentationEngine", "Fail to run view with queue '",
							frame->getRequest()->getQueue()->getName(),
							"': no usable output attachments found");
				}
				return;
			}

			frame->getRequest()->setOutput(a,
					[frame = Rc<PresentationFrame>(frame)](core::FrameAttachmentData &data,
							bool success, Ref *) mutable -> bool {
				if (!frame) {
					return true;
				}
				// Called in GL Thread
				XL_COREPRESENT_LOG("scheduleSwapchainImage: output on frame");
				if (data.image && success) {
					frame->assignResult(data.image);
					frame = nullptr;
					return false;
				} else {
					frame->invalidate();
				}
				frame = nullptr;
				return true;
			},
					this);

			XL_COREPRESENT_LOG("scheduleSwapchainImage: submit frame");

			auto nextFrame = frame->submitFrame();
			if (nextFrame) {
				// set to next suggested number
				_frameOrder = nextFrame->getOrder() + 1;

				_window->setFrameOrder(nextFrame->getOrder());
			}
		} else {
			log::source().error("core::PresentationEngine",
					"acquireFrameData - Swapchain was invalidated");
			frame->invalidate();
		}
	});

	if (frame->getSwapchainImage()) {
		scheduleImage(frame);
	}

	return true;
}

void PresentationEngine::updateConstraints(UpdateConstraintsFlags flags,
		Function<void(bool)> &&cb) {
	XL_COREPRESENT_LOG("deprecateSwapchain");
	if (!_running || !_swapchain) {
		return;
	}

	if (flags == UpdateConstraintsFlags::None) {
		// update should not deprecate swapchain, just update secondary fields
		auto newConstraints = _window->exportConstraints();
		newConstraints.extent = _constraints.extent;
		newConstraints.transform = _constraints.transform;

		_constraints = sp::move(newConstraints);
		_waitForDisplayLink = false;
		return;
	}

	_waitForDisplayLink = false;
	_swapchain->deprecate();

	_deprecationFlags |= flags;
	if (cb) {
		_deprecationCallbacks.emplace_back(sp::move(cb));
	}

	auto it = _scheduledForPresent.begin();
	while (it != _scheduledForPresent.end()) {
		runScheduledPresent(move(it->first), move(it->second), 0);
		it = _scheduledForPresent.erase(it);
	}

	if (_acquisitionTimer) {
		_acquisitionTimer->cancel();
		_acquisitionTimer = nullptr;
	}

	for (auto &it : _acquiredSwapchainImages) {
		if (it->swapchain == _swapchain) {
			_swapchain->invalidateImage(it->imageIndex, true);
		}
	}

	_acquiredSwapchainImages.clear();

	auto acquiredImages = _swapchain->getAcquiredImagesCount();
	if (acquiredImages == 0) {
		scheduleSwapchainRecreation();
	}

	if (_options.syncConstraintsUpdate && hasFlag(flags, UpdateConstraintsFlags::SyncUpdate)
			&& !_waitUntilSwapchainRecreation) {
		_waitUntilSwapchainRecreation = true;
		_loop->getLooper()->run();
		_waitUntilSwapchainRecreation = false;
	}
}

PresentationEngine::~PresentationEngine() {
	log::source().debug("PresentationEngine", "~PresentationEngine");
}

bool PresentationEngine::init(NotNull<Loop> loop, NotNull<Device> device,
		NotNull<PresentationWindow> window, PresentationOptions opts) {
	_options = opts;
	_loop = loop;
	_device = device;
	_window = window;
	_originalSurface = _surface = _window->makeSurface(loop->getInstance());
	_constraints = _window->exportConstraints();
	return true;
}

bool PresentationEngine::run() {
	_running = true;
	return isRunning();
}

void PresentationEngine::end() {
	_running = false;

	if (_acquisitionTimer) {
		_acquisitionTimer->cancel();
		_acquisitionTimer = nullptr;
	}

	Vector<Rc<Ref>> releaseList;

	auto activeFrames = std::move(_activeFrames);
	_activeFrames.clear();

	for (auto &it : activeFrames) {
		releaseList.emplace_back(it);
		it->invalidate();
	}

	auto totalFrames = std::move(_totalFrames);
	_totalFrames.clear();

	for (auto &it : totalFrames) {
		releaseList.emplace_back(it);
		it->invalidate();
	}

	releaseList.clear();

	auto framesAwaitingImages = sp::move(_framesAwaitingImages);
	auto scheduledForPresent = sp::move(_scheduledForPresent);
	auto scheduledPresentHandles = sp::move(_scheduledPresentHandles);

	for (auto &it : framesAwaitingImages) { it->invalidate(); }
	for (auto &it : scheduledForPresent) {
		it.first->invalidate();
		it.second = nullptr;
	}
	for (auto &it : scheduledPresentHandles) { it->cancel(); }

	_framesAwaitingImages.clear();
	_scheduledForPresent.clear();
	_scheduledPresentHandles.clear();

	_swapchain = nullptr;
}

bool PresentationEngine::present(PresentationFrame *frame, ImageStorage *image) {
	XL_COREPRESENT_LOG("present");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		frame->setPresented(Status::Done);
		return true;
	}

	if (image) {
		if (_options.followDisplayLink) {
			// schedule image for next DispayLink signal
			XL_COREPRESENT_LOG("schedulePresent: ", 0);
			_scheduledForPresent.emplace_back(frame, image);
			return true;
		}
		auto clock = sp::platform::clock(ClockType::Monotonic);
		if (_presentWithWindowTiming || _waitUntilFrame || _options.followDisplayLinkBarrier
				|| !_options.usePresentWindow || !_nextPresentWindow
				|| _nextPresentWindow < clock + _engineUpdateInterval) {
			runScheduledPresent(frame, image, _nextPresentWindow);
		} else {
			auto presentWindow = _nextPresentWindow;
			auto frameTimeout = presentWindow - clock;
			XL_COREPRESENT_LOG("schedulePresent: ", frameTimeout);

			// schedule image until next present window
			auto handle = _loop->getLooper()->schedule(TimeInterval::microseconds(frameTimeout),
					[this, frame = Rc<PresentationFrame>(frame), image = Rc<ImageStorage>(image),
							presentWindow](event::Handle *h, bool success) {
				if (success) {
					runScheduledPresent(frame, image, presentWindow);
				} else {
					frame->invalidate();
				}
				_scheduledPresentHandles.erase(h);
			},
					this);

			_scheduledPresentHandles.emplace(move(handle));
		}
	} else {
		if (!_options.renderImageOffscreen) {
			return true;
		}
		if (presentImmediate(frame)) {
			frame->setPresented(Status::ErrorCancelled);
		} else {
			frame->invalidate();
		}
		if (_swapchain->isDeprecated()) {
			scheduleSwapchainRecreation();
		}
	}
	return true;
}

void PresentationEngine::update(PresentationUpdateFlags flags) {
	if ((hasFlag(flags, PresentationUpdateFlags::DisplayLink) && _options.followDisplayLink)
			|| hasFlag(flags, PresentationUpdateFlags::FlushPending)) {
		// ignore present windows
		for (auto &it : _scheduledForPresent) {
			runScheduledPresent(move(it.first), move(it.second), 0);
		}
		_scheduledForPresent.clear();
	}
	if (hasFlag(flags, PresentationUpdateFlags::DisplayLink) && _options.followDisplayLinkBarrier) {
		_waitForDisplayLink = false;
		if (canScheduleNextFrame()) {
			scheduleNextImage();
		}
	}
}

void PresentationEngine::setTargetFrameInterval(uint64_t value) { _targetFrameInterval = value; }

void PresentationEngine::presentWithQueue(DeviceQueue &queue, NotNull<PresentationFrame> frame,
		ImageStorage *image, uint64_t presentWindow) {
	XL_COREPRESENT_LOG("presentWithQueue: ", _activeFrames.size());
	auto clock = sp::platform::clock(ClockType::Monotonic);
	auto res = _swapchain->present(queue, image, presentWindow);
	auto dt = updatePresentationInterval();

	if (res == Status::ErrorFullscreenLost) {
		_swapchain->deprecate();
	} else if (res == Status::Suboptimal || res == Status::ErrorCancelled) {
		XL_COREPRESENT_LOG("presentWithQueue - deprecate swapchain");
		_swapchain->deprecate();
	} else if (res != Status::Ok) {
		log::source().error("vk::View", "presentWithQueue: error:", res);
	}
	XL_COREPRESENT_LOG("presentWithQueue - presented");

	// read before frame marked as presented
	bool isCorrectable = frame->hasFlag(PresentationFrame::CorrectableFrame);

	frame->setPresented(res);

	if (_waitUntilFrame) {
		_loop->getLooper()->wakeup();
		return;
	}

	if (!_options.followDisplayLink && _targetFrameInterval) {
		// use clock before `present` call
		_nextPresentWindow = clock + _targetFrameInterval
				- _engineUpdateInterval; // allow one tick of `update`, that may be required for scheduling
	}

	if (!_running || (_swapchain->getAcquiredImagesCount() != 0 && !_activeFrames.empty())) {
		return;
	}

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		// perform on next stack frame
		scheduleSwapchainRecreation();
	} else if (canScheduleNextFrame()) {
		if (_options.followDisplayLinkBarrier) {
			if (!_waitForDisplayLink) {
				XL_COREPRESENT_LOG(
						"presentWithQueue - scheduleNextImage - followDisplayLinkBarrier");
				scheduleNextImage();
			}
		} else if (_options.followDisplayLink) {
			// no need for a present window if we in DisplayLink mode
			XL_COREPRESENT_LOG("presentWithQueue - scheduleNextImage - followDisplayLink");
			scheduleNextImage();
		} else {
			if (_targetFrameInterval) {
				// adjust present window
				// if current or average framerate below target - reduce present window to release new frame early
				if (isCorrectable && dt.dt > _targetFrameInterval + _engineUpdateInterval) {
					_nextPresentWindow -= (dt.dt - _targetFrameInterval);
				}
			} else {
				_nextPresentWindow = 0;
			}

			XL_COREPRESENT_LOG("presentWithQueue - scheduleNextImage");
			scheduleNextImage(nullptr, PresentationFrame::CorrectableFrame);
		}
	}
}

PresentationEngine::FrameTimeInfo PresentationEngine::updatePresentationInterval() {
	FrameTimeInfo ret;
	ret.clock = sp::platform::clock(ClockType::Monotonic);
	ret.dt = ret.clock - _lastPresentationTime;
	_lastPresentationInterval = ret.dt;
	_avgPresentationInterval.addValue(ret.dt);
	_avgPresentationIntervalValue = _avgPresentationInterval.getAverage();
	_lastPresentationTime = ret.clock;
	ret.avg = _avgPresentationIntervalValue.load();
	return ret;
}

uint64_t PresentationEngine::getFrameOrder() const { return _frameOrder; }

uint64_t PresentationEngine::getLastFrameInterval() const { return _lastPresentationInterval; }

uint64_t PresentationEngine::getAvgFrameInterval() const { return _avgPresentationIntervalValue; }

uint64_t PresentationEngine::getLastFrameTime() const { return _lastFrameTime; }

uint64_t PresentationEngine::getLastFenceFrameTime() const { return _lastFenceFrameTime; }

uint64_t PresentationEngine::getLastTimestampFrameTime() const { return _lastTimestampFrameTime; }

void PresentationEngine::setReadyForNextFrame() {
	// if we not in on-demand mode - ignore
	if (!_options.renderOnDemand) {
		_readyForNextFrame = false;
		return;
	}

	if (!_readyForNextFrame) {
		// spawn frame if there is none
		_readyForNextFrame = true;
		if (canScheduleNextFrame()) {
			XL_COREPRESENT_LOG("setReadyForNextFrame - scheduleNextImage");
			scheduleNextImage();
		}
	}
}

void PresentationEngine::setRenderOnDemand(bool value) { _options.renderOnDemand = value; }

bool PresentationEngine::isRenderOnDemand() const { return _options.renderOnDemand; }

bool PresentationEngine::isRunning() const {
	return _running && _swapchain && !_swapchain->isDeprecated();
}

void PresentationEngine::enableExclusiveFullscreen() {
	_exclusiveFullscreenAvailable = true;
	if (_swapchain
			&& _swapchain->getConfig().fullscreenMode != core::FullScreenExclusiveMode::Default
			&& !_swapchain->isExclusiveFullscreen()) {
		updateConstraints(UpdateConstraintsFlags::DeprecateSwapchain);
	}
}

bool PresentationEngine::handleFrameStarted(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameStarted");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		return _detachedFrames.emplace(frame).second;
	} else {
		_totalFrames.emplace(frame);
		return _activeFrames.emplace(frame).second;
	}
}

void PresentationEngine::handleFrameInvalidated(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameInvalidated");

	auto it = _framesAwaitingImages.begin();
	while (it != _framesAwaitingImages.end()) {
		if (*it == frame) {
			it = _framesAwaitingImages.erase(it);
		} else {
			++it;
		}
	}

	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_detachedFrames.erase(frame);
		return;
	}

	_activeFrames.erase(frame);
	_totalFrames.erase(frame);

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		// perform on next stack frame
		scheduleSwapchainRecreation();
	} else {
		acquireScheduledImage();
	}
}

void PresentationEngine::handleFrameReady(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameReady");
	if (_options.earlyPresent) {
		present(frame, frame->getSwapchainImage());
	} else if (_options.preStartFrame) {
		if (!frame->hasFlag(PresentationFrame::DoNotPresent)) {
			_activeFrames.erase(frame);
			if (canScheduleNextFrame()) {
				XL_COREPRESENT_LOG("handleFrameReady - scheduleNextImage");
				scheduleNextImage();
			}
		}
	}
}

void PresentationEngine::handleFramePresented(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFramePresented");

	if (!frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_window->handleFramePresented(frame);
		_activeFrames.erase(frame);
	}

	if (!_options.earlyPresent) {
		if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
			_detachedFrames.erase(frame);
		} else {
			_totalFrames.erase(frame);
			if (!_framesAwaitingImages.empty()) {
				acquireScheduledImage();
			}
		}
	}
}

void PresentationEngine::handleFrameComplete(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameCancel");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_detachedFrames.erase(frame);
		return;
	}
	if (auto h = frame->getHandle()) {
		_lastFrameTime = h->getTimeEnd() - h->getTimeStart();
		_avgFrameTime.addValue(_lastFrameTime);
		_avgFrameTimeValue = _avgFrameTime.getAverage();

		if (auto t = h->getSubmissionTime()) {
			_lastFenceFrameTime = t;
			_avgFenceInterval.addValue(t);
			_avgFenceIntervalValue = _avgFenceInterval.getAverage();
		}
		if (auto t = h->getDeviceTime()) {
			_lastTimestampFrameTime = t;
			_avgTimestampInterval.addValue(t);
			_avgTimestampIntervalValue = _avgTimestampInterval.getAverage();
		}
	}
	if (!_options.earlyPresent && frame->hasFlag(PresentationFrame::ImageRendered)) {
		_loop->performOnThread(
				[this, image = Rc<SwapchainImage>(frame->getSwapchainImage()),
						frame = Rc<PresentationFrame>(frame)] { present(frame, image); },
				this, false);
	} else {
		_totalFrames.erase(frame);
		if (_swapchain) {
			if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
				// perform on next stack frame
				scheduleSwapchainRecreation();
			} else if (canScheduleNextFrame()) {
				XL_COREPRESENT_LOG("handleFrameComplete - scheduleNextImage");
				scheduleNextImage();
			} else if (!_framesAwaitingImages.empty()) {
				acquireScheduledImage();
			}
		}
	}
}

void PresentationEngine::captureScreenshot(
		Function<void(const ImageInfoData &info, BytesView view)> &&cb) {

	scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder,
			PresentationFrame::OffscreenTarget | PresentationFrame::DoNotPresent,
			[this, cb = sp::move(cb)](PresentationFrame *frame, bool success) mutable {
		auto target = frame->getTarget();
		_loop->captureImage(sp::move(cb), target->getImage(), target->getLayout());
	}));
}

void PresentationEngine::synchronizeClose() { _surface->invalidate(); }

void PresentationEngine::acquireFrameData(NotNull<PresentationFrame> frame,
		Function<void(NotNull<PresentationFrame>)> &&cb) {
	_window->acquireFrameData(frame, sp::move(cb));
}

void PresentationEngine::scheduleSwapchainRecreation() {
	if (_swapchain && _swapchain->getPresentedFramesCount() == 0) {
		log::source().warn("core::PresentationEngine",
				"Scheduling swapchain recreation without frame presentation");
	}

	// prevent to schedule more then one callback
	if (!_swapchainRecreationScheduled) {
		_swapchainRecreationScheduled = true;
		_loop->performOnThread([this] {
			log::source().debug("PresentationEngine", "scheduleSwapchainRecreation");
			_swapchainRecreationScheduled = false;
			recreateSwapchain();
			if (_waitUntilSwapchainRecreation) {
				_loop->getLooper()->wakeup();
			}
		}, this, false);
	}
}

void PresentationEngine::resetFrames() {
	auto frames = _activeFrames;
	for (auto &it : frames) { it->invalidate(); }

	frames = _totalFrames;
	for (auto &it : frames) { it->invalidate(); }

	frames = _detachedFrames;
	for (auto &it : frames) { it->invalidate(); }

	for (auto &it : _scheduledPresentHandles) { it->cancel(); }
	_scheduledPresentHandles.clear();

	_framesAwaitingImages.clear();
	_scheduledForPresent.clear();
	_requestedSwapchainImage.clear();
	_acquiredSwapchainImages.clear();
}

void PresentationEngine::scheduleImage(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG("scheduleImage");
	if (!_acquiredSwapchainImages.empty()) {
		// pop one of the previously acquired images
		auto acquiredImage = _acquiredSwapchainImages.front();
		_acquiredSwapchainImages.pop_front();
		frame->assignSwapchainImage(acquiredImage);
	} else {
		_framesAwaitingImages.emplace_back(frame);
		acquireScheduledImage();
	}
}

Status PresentationEngine::acquireScheduledImage() {
	if (!_requestedSwapchainImage.empty() || _framesAwaitingImages.empty()
			|| _totalFrames.size() != _activeFrames.size()) {
		XL_COREPRESENT_LOG("acquireScheduledImage - dropped");
		return Status::Declined;
	}

	Status status;

	XL_COREPRESENT_LOG("acquireScheduledImage");
	auto loop = (Loop *)_loop.get();
	auto fence = loop->acquireFence(FenceType::Swapchain);
	if (auto acquiredImage = _swapchain->acquire(true, fence, status)) {
		_requestedSwapchainImage.emplace(acquiredImage);
		XL_COREPRESENT_LOG("acquireScheduledImage - spawn request: ",
				_requestedSwapchainImage.size());
		fence->addRelease([this, f = fence.get(), acquiredImage](bool success) mutable {
			if (success) {
				handleSwapchainImageReady(move(acquiredImage));
			} else {
				_requestedSwapchainImage.erase(acquiredImage);
			}
			XL_COREPRESENT_LOG("[", f->getFrame(), "] acquireScheduledImage [complete]", " [",
					sp::platform::clock(ClockType::Monotonic) - f->getArmedTime(), "]");
		}, this, "PresentationEngine::acquireScheduledImage");
		fence->schedule(*loop);
		return Status::Ok;
	} else {
		XL_COREPRESENT_LOG("acquireScheduledImage - failed");
		fence->schedule(*loop);
		if (status == Status::Timeout) {
			// schedule timed waiter
			scheduleImageAcquisition();
		}
		return status;
	}
}

void PresentationEngine::scheduleImageAcquisition() {
	_acquisitionTimer = _loop->getLooper()->scheduleTimer(event::TimerInfo{
		.completion = event::CompletionHandle<event::TimerHandle>::create<PresentationEngine>(this,
				[](PresentationEngine *e, event::TimerHandle *h, uint32_t, Status st) {
		if (st == Status::Ok && e->acquireScheduledImage() != Status::Timeout) {
			h->cancel();
			if (e->_acquisitionTimer == h) {
				e->_acquisitionTimer = nullptr;
			}
		}
	}),
		.interval = config::PresentationSchedulerInterval,
		.count = event::TimerInfo::Infinite,
	});
}

void PresentationEngine::handleSwapchainImageReady(Rc<Swapchain::SwapchainAcquiredImage> &&image) {
	XL_COREPRESENT_LOG("onSwapchainImageReady: ", _framesAwaitingImages.size());
	auto ptr = image.get();

	if (!_framesAwaitingImages.empty()) {
		// send new swapchain image to framebuffer
		auto target = _framesAwaitingImages.front();

		if (target->assignSwapchainImage(image)) {
			_framesAwaitingImages.pop_front();
		} else {
			target->invalidate();
		}
	} else {
		// hold image until next framebuffer request, if not active queries
		_acquiredSwapchainImages.emplace_back(move(image));
	}

	_requestedSwapchainImage.erase(ptr);

	if (!_framesAwaitingImages.empty()) {
		// run next image query if someone waits for it
		acquireScheduledImage();
	}
}

void PresentationEngine::runScheduledPresent(NotNull<PresentationFrame> frame, ImageStorage *image,
		uint64_t presentWindow) {
	XL_COREPRESENT_LOG("runScheduledPresent");

	if (!_loop->isRunning() || frame->hasFlag(PresentationFrame::Invalidated)) {
		return;
	}
	auto queue = _device->tryAcquireQueue(QueueFlags::Present);
	if (queue) {
		presentSwapchainImage(move(queue), frame, image, presentWindow);
	} else {
		_device->acquireQueue(QueueFlags::Present, *_loop,
				[this, frame = Rc<PresentationFrame>(frame), image = Rc<ImageStorage>(image),
						presentWindow](Loop &, const Rc<DeviceQueue> &queue) mutable {
			presentSwapchainImage(Rc<DeviceQueue>(queue), frame, image, presentWindow);
		},
				[frame = Rc<PresentationFrame>(frame)](Loop &) { frame->invalidate(); }, this);
	}
}

void PresentationEngine::presentSwapchainImage(Rc<DeviceQueue> &&queue,
		NotNull<PresentationFrame> frame, ImageStorage *image, uint64_t presentWindow) {
	XL_COREPRESENT_LOG("presentSwapchainImage");
	if (frame->getSwapchain() == _swapchain && frame->getSwapchainImage()->isSubmitted()) {
		presentWithQueue(*queue, frame, image, presentWindow);
	}
	_device->releaseQueue(move(queue));
}

bool PresentationEngine::canScheduleNextFrame() const {
	return (!_options.renderOnDemand || _readyForNextFrame) && _swapchain && _activeFrames.empty();
}

} // namespace stappler::xenolith::core
