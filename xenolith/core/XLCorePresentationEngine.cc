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
#include "XLCoreRenderSession.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

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
	// Both sides null must not count as equal: after end() that let callers touch a dead
	// swapchain.
	if (_swapchain && frame->getSwapchain() == _swapchain && !_swapchain->isDeprecated()) {
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

#if XL_FRAME_ACCOUNT
	// After the display-link barrier above: a call that returns without scheduling anything is not
	// the start of a frame, and marking it would charge the wait to the wrong bucket.
	markFrame(FrameMark::Scheduled);
#endif

	if (scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder,
				_serial, frameFlags, sp::move(cb)))) {
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
		if (isRunning() && _swapchain && frame->getSwapchain() == _swapchain) {
			XL_COREPRESENT_LOG("scheduleSwapchainImage: setup frame request");
			auto a = frame->setupOutputAttachment();
			if (!a) {
				if (frame->getRequest()->getQueue()) {
					log::source().error("core::PresentationEngine", "Fail to run view with queue '",
							frame->getRequest()->getQueue()->getName(),
							"': no usable output attachments found");
				}
				// Full frame invalidation (not just engine bookkeeping): this returns the swapchain image
				// the frame already acquired -- to the reuse pool when no queue was ever assigned (the
				// usual case when a remote client never answered AcquireFrame), or to the swapchain when
				// rendering had started. Calling handleFrameInvalidated() directly here would skip that and
				// strand the acquired image.
				frame->invalidate();
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

				// arm the deadline now that the frame is processing (and its queue is armed)
				scheduleFrameDeadline(frame);
			}
		} else {
			log::source().error("core::PresentationEngine",
					"acquireFrameData - Swapchain was invalidated");
			frame->invalidate();
		}
	});

	// Track a remote-served frame from the moment it is scheduled. The window marks it Remote (above, in
	// acquireFrameData) when it is handed to a remote client; tracking it here -- before it enters
	// _activeFrames via submitFrame -- means a connection reset can still force-invalidate a frame that
	// is merely awaiting the client's AcquireFrame reply (the exact state a non-responding client wedges).
	if (frame->hasFlag(PresentationFrame::Remote)) {
		_remoteFrames.emplace(frame.get(), frame);
	}

	if (frame->getSwapchainImage()) {
		scheduleImage(frame);
	}

	return true;
}

void PresentationEngine::updateConstraints(UpdateConstraintsFlags flags,
		Function<void(bool)> &&cb) {
	XL_COREPRESENT_LOG("deprecateSwapchain");
	if (!_running || !_swapchain) {
		// The callback is a handshake, not a notification: close() waits on it to release the
		// window. Report the failure rather than going silent and stranding the AppWindow.
		if (cb) {
			cb(false);
		}
		return;
	}

	if (flags == UpdateConstraintsFlags::None) {
		// update should not deprecate swapchain, just update secondary fields
		auto newConstraints = _window->exportConstraints(_serial);
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

	// EndOfLife cannot wait for acquiredImages==0 the way a resize deprecate does: the display
	// link may already be paused, so an in-flight frame never finishes, recreation never
	// schedules and the close callback never fires. Abort the frames and recreate unconditionally.
	if (hasFlag(flags, UpdateConstraintsFlags::EndOfLife)) {
		resetFrames();
		scheduleSwapchainRecreation();
	} else {
		auto acquiredImages = _swapchain->getAcquiredImagesCount();
		if (acquiredImages == 0) {
			scheduleSwapchainRecreation();
		}
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
	_constraints = _window->exportConstraints(_serial);

	if (auto value = ::getenv("XL_DAMAGE_DEBUG")) {
		_damageDebug = StringView(value) != "0";
	}

	// Bound the frame rate. On platforms with a display-link (vsync) callback presentation is
	// driven by that; without one — e.g. the WebGPU/wasm backend — nothing limits the rate and the
	// engine renders on every scheduler tick/event (hundreds of fps, wasted work + churn). Fall
	// back to pacing at the surface's WM frame interval unless a rate was set explicitly.
	if (!_options.followDisplayLink && _targetFrameInterval == 0 && _constraints.frameInterval) {
		_targetFrameInterval = _constraints.frameInterval;
	}
	return true;
}

bool PresentationEngine::run() {
	_running = true;
	return isRunning();
}

void PresentationEngine::end() {
	_running = false;

	// The swapchain and its images go away below; with several windows on one device a sibling
	// may still be rendering, so drain the GPU first — the same barrier recreateSwapchain takes.
	if (_device && _swapchain) {
		_device->waitIdle();
	}

	if (_acquisitionTimer) {
		_acquisitionTimer->cancel();
		_acquisitionTimer = nullptr;
	}

	Vector<Rc<Ref>> releaseList;

	auto activeFrames = sprt::move(_activeFrames);
	_activeFrames.clear();

	for (auto &it : activeFrames) {
		releaseList.emplace_back(it);
		it->invalidate();
	}

	auto totalFrames = sprt::move(_totalFrames);
	_totalFrames.clear();

	for (auto &it : totalFrames) {
		releaseList.emplace_back(it);
		it->invalidate();
	}

	// Remote frames still awaiting a client reply are not in _activeFrames/_totalFrames yet; invalidate
	// them too (idempotent for any already handled above).
	auto remoteFrames = sprt::move(_remoteFrames);
	_remoteFrames.clear();
	for (auto &it : remoteFrames) { it.second->invalidate(); }

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
							presentWindow](sprt::dispatch::Handle *h, bool success) {
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

void PresentationEngine::presentWithQueue(DeviceQueue *queue, NotNull<PresentationFrame> frame,
		ImageStorage *image, uint64_t presentWindow) {
	XL_COREPRESENT_LOG("presentWithQueue: ", _activeFrames.size());

	_window->handleFrameReady(frame);

	// Diff this frame against what the compositor currently shows. Incremental-present rectangles
	// are relative to the previously presented image, so the baseline is the presented snapshot,
	// not the one describing this particular image buffer (which is what bounds partial redraw).
	auto request = frame->getRequest();

	Vector<URect> damage;
	bool partial = _swapchain->getDamage().computePresentDamage(
			request ? request->getDamageState().get() : nullptr,
			Extent2(_constraints.extent.width, _constraints.extent.height), damage);

	if (partial && damage.empty()) {
		// Screen and frame already agree. There is no way to say "nothing changed" - a rectangle
		// count of zero means the whole image changed - so hand over the smallest legal region.
		damage.emplace_back(URect{0, 0, 1, 1});
	}

	if (_damageDebug) {
		float covered = 0.0f;
		for (auto &it : damage) { covered += float(it.width) * float(it.height); }
		const float surface = float(_constraints.extent.width) * float(_constraints.extent.height);
		log::source().info("DamageDebug", "image=", image->getImageIndex(),
				partial ? " partial rects=" : " FULL rects=", damage.size(),
				" area=", surface > 0.0f ? covered / surface : 1.0f,
				request && request->isRedrawSkipped() ? " [redraw skipped]" : "");
		for (auto &it : damage) {
			log::source().info("DamageDebug", "  rect ", it.x, ",", it.y, " ", it.width, "x",
					it.height);
		}
	}

	core::PresentInfo presentInfo{presentWindow,
		partial ? makeSpanView(damage) : SpanView<URect>()};

	auto clock = sp::platform::clock(ClockType::Monotonic);
	auto res = _swapchain->present(queue, image, presentInfo);
#if XL_FRAME_ACCOUNT
	// Closes the timeline. Here and not in the backend's swapchain: every backend presents through
	// this call, and the mark has to be the same point on every one of them for the buckets to
	// mean the same thing.
	markFrame(FrameMark::Presented);
#endif
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

	cancelFrameDeadline(frame);
	_remoteFrames.erase(frame.get());

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

	// A scheduled frame raises the display-link barrier (_waitForDisplayLink, see scheduleNextImage),
	// which is normally lowered when a frame presents. A frame that is invalidated never presents, so if
	// it was the last in-flight frame nothing remains to lower the barrier -- and in barrier mode the
	// display-link signal is itself driven by presentation, so it would stay raised forever, wedging all
	// further scheduling. Release it only in that no-frame-left case (so continuous, normally-presenting
	// frames keep their display-link pacing untouched).
	if (_activeFrames.empty()) {
		_waitForDisplayLink = false;
	}

	if (_swapchain->isDeprecated()) {
		auto acquiredImageCount = _swapchain->getAcquiredImagesCount();
		if (acquiredImageCount == 0) {
			// perform on next stack frame
			scheduleSwapchainRecreation();
		}
	} else {
		acquireScheduledImage();
	}
}

void PresentationEngine::handleFrameReady(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameReady");
	cancelFrameDeadline(frame);
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

	cancelFrameDeadline(frame);
	_remoteFrames.erase(frame.get());

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
	cancelFrameDeadline(frame);
	_remoteFrames.erase(frame.get());
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_detachedFrames.erase(frame);
		return;
	}
	if (auto h = frame->getHandle()) {
		_lastFrameTime = h->getTimeEnd() - h->getTimeStart();
#if XL_FRAME_ACCOUNT
		// Written INSIDE the existing block, after the DoNotPresent return above. Moving anything
		// ahead of that return crashed the app once; a capture frame simply has no timing and is
		// correctly absent from this account.
		_lastFrameOrder = frame->getFrameOrder();
#endif
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

void PresentationEngine::scheduleFrameDeadline(NotNull<PresentationFrame> frame) {
	auto req = frame->getRequest();
	if (!req) {
		return;
	}

	auto deadline = req->getDeadline();
	if (deadline == 0) {
		return; // no deadline by default
	}
	auto now = sp::platform::clock(ClockType::Monotonic);
	auto timeout = (deadline > now) ? (deadline - now) : 0;

	auto handle = _loop->getLooper()->schedule(TimeInterval::microseconds(timeout),
			[this, frame = Rc<PresentationFrame>(frame)](sprt::dispatch::Handle *, bool success) {
		// erase before invalidate so the resulting handleFrameInvalidated finds nothing to cancel
		_frameDeadlines.erase(frame.get());
		if (success) {
			log::source().warn("core::PresentationEngine", "Frame ", frame->getFrameOrder(),
					" deadline reached; cancelling (stuck waiting for input/dependencies)");
			frame->invalidate();
		}
	}, this);

	_frameDeadlines.emplace(frame.get(), sp::move(handle));
}

void PresentationEngine::cancelFrameDeadline(NotNull<PresentationFrame> frame) {
	auto it = _frameDeadlines.find(frame.get());
	if (it != _frameDeadlines.end()) {
		// erase first, then cancel: cancel() may re-enter the schedule callback synchronously
		auto handle = sp::move(it->second);
		_frameDeadlines.erase(it);
		handle->cancel();
	}
}

void PresentationEngine::handleSwapchainUpdated(const FrameConstraints &c) {
	_window->handleSwapchainUpdated(c);
}

void PresentationEngine::invalidateRemoteFrames() {
	if (_remoteFrames.empty()) {
		return;
	}
	// Snapshot + clear first: frame->invalidate() re-enters handleFrameInvalidated, which mutates
	// _remoteFrames. The held Rc keeps each frame alive across its own invalidation even if the dropped
	// connection was its only other owner.
	auto frames = sprt::move(_remoteFrames);
	_remoteFrames.clear();
	log::source().warn("core::PresentationEngine", "Killing ", frames.size(),
			" remote frame(s) after client reset");
	for (auto &it : frames) { it.second->invalidate(); }
}

void PresentationEngine::resetForRenderClientChange() {
	// The window's render client just changed (a remote client took over, or the window reverted to its
	// local Director after a reset). A frame that was dropped rather than presented may have left the
	// display-link barrier raised; in barrier mode the display-link signal is driven by presentation, so
	// once frames stop it can never fire again to clear it -- wedging all further scheduling. Clear it
	// and pump one fresh frame to restart the present -> display-link cycle for the new client. This runs
	// only on a client change, so it does not affect normal frame pacing.
	_waitForDisplayLink = false;
	_readyForNextFrame = true;
	if (canScheduleNextFrame()) {
		scheduleNextImage();
	}
}

void PresentationEngine::captureScreenshot(
		Function<void(const ImageInfoData &info, BytesView view)> &&cb) {

	scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder, _serial,
			PresentationFrame::OffscreenTarget | PresentationFrame::DoNotPresent,
			[this, cb = sp::move(cb)](PresentationFrame *frame, bool success) mutable {
		auto target = frame->getTarget();
		_loop->captureImage(sp::move(cb), target->getImage(), target->getLayout());
	}));
}

void PresentationEngine::scheduleOffscreenFrame(Function<void(bool)> &&cb) {
	scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder, _serial,
			PresentationFrame::OffscreenTarget | PresentationFrame::DoNotPresent,
			[cb = sp::move(cb)](PresentationFrame *, bool success) mutable {
		if (cb) {
			cb(success);
		}
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

	// Move out before cancelling: a cancel runs its completion, which erases the handle from this
	// very set and would invalidate the iterator underneath us.
	auto scheduledPresentHandles = sp::move(_scheduledPresentHandles);
	_scheduledPresentHandles.clear();
	for (auto &it : scheduledPresentHandles) { it->cancel(); }

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
		XL_COREPRESENT_LOG("acquireScheduledImage - dropped: ", !_requestedSwapchainImage.empty(),
				" ", _framesAwaitingImages.empty(), " ", _totalFrames.size(), " ",
				_activeFrames.size());
		return Status::Declined;
	}


	XL_COREPRESENT_LOG("acquireScheduledImage");
	auto loop = (Loop *)_loop.get();

	Status status = Status::Ok;
	Rc<Swapchain::SwapchainAcquiredImage> acquiredImage;
	Rc<Fence> fence;

	if (!_options.acquireImageWithoutFence) {
		fence = loop->acquireFence(FenceType::Swapchain);
		acquiredImage = _swapchain->acquire(true, fence, status);
		if (acquiredImage) {
			_requestedSwapchainImage.emplace(acquiredImage);
			XL_COREPRESENT_LOG("acquireScheduledImage - spawn request: ",
					_requestedSwapchainImage.size(), " - ", acquiredImage->imageIndex);
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
		}
	} else {
		// Without Fence, we have no ability to wait before image ACTUALLY ready, so, lock immediately (lockfree = false)
		acquiredImage = _swapchain->acquire(false, fence, status);
		if (acquiredImage) {
			_requestedSwapchainImage.emplace(acquiredImage);
			XL_COREPRESENT_LOG("acquireScheduledImage - acquired: ",
					_requestedSwapchainImage.size(), " - ", acquiredImage->imageIndex);
			handleSwapchainImageReady(move(acquiredImage));
			return Status::Ok;
		}
	}

	if (!acquiredImage) {
		XL_COREPRESENT_LOG("acquireScheduledImage - failed");
		if (fence) {
			fence->schedule(*loop);
		}
		// Timeout and Declined both mean "no image available yet" for a lockfree acquire
		// (VK_TIMEOUT and VK_NOT_READY); either way this timer is the only thing that retries.
		if (status == Status::Timeout || status == Status::Declined) {
			// schedule timed waiter
			scheduleImageAcquisition();
		}
	}
	return status;
}

void PresentationEngine::scheduleImageAcquisition() {
	// One retry timer per engine, and it repeats on its own (count = Infinite), so a second one is
	// never useful: each firing retries and schedules again on failure, so arming a new timer per
	// failed acquire would double the timer population every interval.
	//
	// Status::Ok is "armed and running"; anything else means the handle is spent and a new one is
	// needed (see Handle::getStatus).
	if (_acquisitionTimer && _acquisitionTimer->getStatus() == Status::Ok) {
		return;
	}

	_acquisitionTimer = _loop->getLooper()->scheduleTimer(sprt::dispatch::TimerInfo{
		.completion = sprt::dispatch::CompletionHandle<sprt::dispatch::TimerHandle>::create<
				PresentationEngine>(this,
				[](PresentationEngine *e, sprt::dispatch::TimerHandle *h, uint32_t, Status st) {
		auto acquireStatus = st == Status::Ok ? e->acquireScheduledImage() : st;

		// Timeout/Declined both mean "still no image": keep the timer, it will retry. Anything else
		// ends the retry - either an image arrived, or the timer itself is finishing (cancelled,
		// failed). Forget the handle in both cases, so the next failed acquire can arm a fresh one
		// instead of finding a dead handle parked in _acquisitionTimer and never retrying again.
		if (acquireStatus == Status::Timeout || acquireStatus == Status::Declined) {
			return;
		}
		if (st == Status::Ok) {
			h->cancel();
		}
		if (e->_acquisitionTimer == h) {
			e->_acquisitionTimer = nullptr;
		}
	}),
		.interval = config::PresentationSchedulerInterval,
		.count = sprt::dispatch::TimerInfo::Infinite,
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

void PresentationEngine::reclaimAcquiredImage(Rc<Swapchain::SwapchainAcquiredImage> &&image) {
	// Only re-pool an image that still belongs to the current swapchain; one from a superseded swapchain
	// is dropped (that swapchain's teardown reclaims it).
	if (image && _swapchain && image->swapchain == _swapchain) {
		_acquiredSwapchainImages.emplace_back(sp::move(image));
	}
}

void PresentationEngine::runScheduledPresent(NotNull<PresentationFrame> frame, ImageStorage *image,
		uint64_t presentWindow) {
	XL_COREPRESENT_LOG("runScheduledPresent");

	// EndOfLife/resetFrames can null _swapchain while a Present queue acquire is in flight — the
	// release callback must not present into a dead engine.
	if (!_running || !_swapchain || !_loop->isRunning()
			|| frame->hasFlag(PresentationFrame::Invalidated)) {
		return;
	}

	// A pseudo-swapchain (headless) presents without touching the GPU, and its device has no
	// Present family to acquire from in the first place.
	if (!_swapchain->isPresentQueueRequired()) {
		presentSwapchainImage(nullptr, frame, image, presentWindow);
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
	// After end()/EndOfLife both _swapchain and the frame's swapchain can be null, and `null ==
	// null` passes the equality check below - hence the explicit null tests.
	auto *swImage = frame->getSwapchainImage();
	if (_running && _swapchain && swImage && frame->getSwapchain() == _swapchain
			&& !frame->hasFlag(PresentationFrame::Invalidated) && swImage->isSubmitted()) {
		presentWithQueue(queue.get(), frame, image, presentWindow);
	}
	if (queue) {
		_device->releaseQueue(move(queue));
	}
}

bool PresentationEngine::canScheduleNextFrame() const {
	return (!_options.renderOnDemand || _readyForNextFrame) && _swapchain && _activeFrames.empty();
}

} // namespace stappler::xenolith::core
