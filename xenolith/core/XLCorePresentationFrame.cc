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

#include "XLCorePresentationFrame.h"
#include "XLCorePresentationEngine.h"
#include "XLCoreFrameRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool PresentationFrame::init(PresentationEngine *e, FrameConstraints c, uint64_t frameOrder,
		uint64_t serial, Flags flags,
		Function<void(PresentationFrame *, bool)> &&completeCallback) {
	_info.constraints = c;
	_info.serial = serial;
	_info.order = frameOrder;
	_flags = (flags & InitFlags);

	_engine = e;
	_swapchain = _engine->getSwapchain();

	if (!sp::hasFlag(_flags, OffscreenTarget)) {
		if (!_swapchain) {
			return false;
		}

		_target = Rc<SwapchainImage>::create(_swapchain, frameOrder);
		if (!_target) {
			return false;
		}

		_target->setReady(false);

		auto extent = _target->getInfo().extent;

		c.extent = Extent3(extent.width, extent.height, 1);
	}

	_frameRequest = Rc<FrameRequest>::create(this, c);

	_active = e->handleFrameStarted(this);
	_completeCallback = sp::move(completeCallback);

	return true;
}

bool PresentationFrame::isValid() const {
	return !hasFlag(Invalidated) && _engine->isFrameValid(this);
}

SwapchainImage *PresentationFrame::getSwapchainImage() const {
	if (!sp::hasFlag(_flags, OffscreenTarget) && _target) {
		return static_cast<SwapchainImage *>(_target.get());
	}
	return nullptr;
}

core::AttachmentData *PresentationFrame::setupOutputAttachment() {
	auto &queue = _frameRequest->getQueue();
	if (!queue.get()) {
		return nullptr;
	}

	auto a = queue->getPresentImageOutput();
	if (!a) {
		a = queue->getTransferImageOutput();
	}
	if (a && _target) {
		_frameRequest->setRenderTarget(a, Rc<core::ImageStorage>(_target));
	}
	_flags |= InputAcquired;
	return a;
}

core::FrameHandle *PresentationFrame::submitFrame() {
	_frameHandle = _engine->submitNextFrame(Rc<core::FrameRequest>(_frameRequest));

	if (!_frameHandle) {
		invalidate();
		return nullptr;
	}

	if (_target) {
		_target->setFrameIndex(_frameHandle->getOrder());
	}

	_info.order = _frameHandle->getOrder();

	_flags |= FrameSubmitted;
	return _frameHandle;
}

bool PresentationFrame::assignSwapchainImage(Swapchain::SwapchainAcquiredImage *acquiredImage) {
	auto sw = getSwapchainImage();
	if (!sw) {
		log::source().error("core::PresentationFrame",
				"assignSwapchainImage called on a frame without a swapchain image target");
		return false;
	}

	if (acquiredImage->swapchain != _swapchain) {
		log::source().error("core::PresentationFrame",
				"Image swapchain and ViewFrame swapchain are different");
		return false;
	}

	sw->setAcquisitionTime(sp::platform::clock(ClockType::Monotonic));
	// Keep the SwapchainAcquiredImage intact (copy the swapchain handle instead of moving it) and retain
	// it: if this frame is discarded before rendering starts, invalidate() hands this untouched image
	// straight back to the engine's reuse pool instead of dropping the acquired swapchain slot.
	sw->setImage(Rc<Swapchain>(acquiredImage->swapchain), *acquiredImage->data, acquiredImage->sem);
	sw->setReady(true);
	_acquiredImage = acquiredImage;
	_flags |= ImageAcquired;
	return true;
}

bool PresentationFrame::assignResult(core::ImageStorage *target) {
	if (_target && _target != target) {
		log::source().error("vk::ViewFrame", "Target already assigned");
		return false;
	}
	_target = target;
	_flags |= ImageRendered;
	_engine->handleFrameReady(this);
	return true;
}

void PresentationFrame::invalidate() {
	if (sp::hasFlag(_flags, Invalidated)) {
		return;
	}

	_flags |= Invalidated;

	auto refId = sprt::retain(this);

	if (auto sw = getSwapchainImage()) {
		if (_engine && _acquiredImage && !sp::hasFlag(_flags, InputAcquired) && _swapchain
				&& _swapchain->isValid()) {
			// Discarded before rendering started (image acquired, but no frame data / queue yet): the
			// image is untouched, so detach it from this frame (without releasing it) and hand it back to
			// the engine's reuse pool for the next frame.
			sw->detachImage();
			if (_swapchain) {
				if (!_swapchain->isDeprecated()) {
					_engine->reclaimAcquiredImage(sp::move(_acquiredImage));
				} else {
					_swapchain->invalidateImage(_acquiredImage->imageIndex, true);
				}
			} else {
				sw->invalidateImage();
			}
		} else {
			// Rendering had started (or the swapchain is gone): return the image directly to the swapchain.
			sw->invalidateImage();
		}
	}

	if (_target) {
		_target->invalidate();
	}

	if (_active && _engine) {
		_active = false;
		if (_frameHandle) {
			_frameHandle->invalidate();
		}

		if (_engine) {
			_engine->handleFrameInvalidated(this);
		}

		if (_completeCallback) {
			_completeCallback(this, false);
		}
	}

	_swapchain = nullptr;
	_target = nullptr;
	_acquiredImage =
			nullptr; // dropped here for the direct-return path; already moved out for pool reuse
	_frameRequest = nullptr;

	sprt::release(this, refId);
}

void PresentationFrame::cancelFrameHandle() {
	_engine->handleFrameComplete(this);
	_frameHandle = nullptr;
}

void PresentationFrame::setSubmitted() { _flags |= QueueSubmitted; }

void PresentationFrame::markRemote() { _flags |= Remote; }

void PresentationFrame::setPresented(Status st) {
	_flags |= ImagePresented;
	_presentationStatus = st;
	if (_active && _engine) {
		_engine->handleFramePresented(this);
		if (_completeCallback) {
			_completeCallback(this, true);
		}
		_active = false;
		_target = nullptr;
	}
}

} // namespace stappler::xenolith::core
