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
		Flags flags, Function<void(PresentationFrame *, bool)> &&completeCallback) {
	_constraints = c;
	_frameOrder = frameOrder;
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

		c.extent = Extent2(extent.width, extent.height);
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

	if (_target) {
		_target->setFrameIndex(_frameHandle->getOrder());
	}

	_frameOrder = _frameHandle->getOrder();

	_flags |= FrameSubmitted;
	return _frameHandle;
}

bool PresentationFrame::assignSwapchainImage(Swapchain::SwapchainAcquiredImage *acquiredImage) {
	auto sw = getSwapchainImage();

	if (acquiredImage->swapchain != _swapchain) {
		log::source().error("core::PresentationFrame",
				"Image swapchain and ViewFrame swapchain are different");
		return false;
	}

	sw->setAcquisitionTime(sp::platform::clock(ClockType::Monotonic));
	sw->setImage(move(acquiredImage->swapchain), *acquiredImage->data, move(acquiredImage->sem));
	sw->setReady(true);
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

	auto refId = retain();

	if (auto sw = getSwapchainImage()) {
		sw->invalidateImage();
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
	_frameRequest = nullptr;

	release(refId);
}

void PresentationFrame::cancelFrameHandle() {
	_engine->handleFrameComplete(this);
	_frameHandle = nullptr;
}

void PresentationFrame::setSubmitted() { _flags |= QueueSubmitted; }

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
