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

#include "XLFrameCapture.h"
#include "XLAppThread.h"
#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "XLCoreRenderSession.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool FrameCaptureTarget::init(StringView key, const URect &region, core::ImageFormat format) {
	if (region.width == 0 || region.height == 0 || format == core::ImageFormat::Undefined) {
		return false;
	}

	_region = region;

	// TransferDst for the filling copy, Sampled for the drawing material; never rendered into or
	// read back.
	core::ImageInfo info(Extent2(region.width, region.height),
			core::ForceImageUsage(core::ImageUsage::TransferDst | core::ImageUsage::Sampled),
			format);

	const auto bytes =
			size_t(region.width) * size_t(region.height) * core::getFormatBlockSize(format);

	_dynamic = Rc<core::DynamicImage>::create([&](core::DynamicImage::Builder &builder) {
		// The payload is generated on demand for the upload only. Using the ordinary compile path
		// leaves the image in ShaderReadOnlyOptimal with defined contents, safe to sample before a
		// capture lands.
		return builder.setImage(key, sp::move(info),
					   [bytes](uint8_t *, uint64_t, const core::ImageData::DataCallback &dcb) {
			Bytes transparent;
			transparent.resize(bytes, uint8_t(0));
			dcb(transparent);
		}) != nullptr;
	});

	return _dynamic != nullptr;
}

core::ImageObject *FrameCaptureTarget::getImage() const {
	if (!_dynamic) {
		return nullptr;
	}
	auto instance = _dynamic->getInstance();
	return instance ? instance->data.image.get() : nullptr;
}

void FrameCaptureTarget::handleCompiled(bool success) {
	if (_state != State::Allocating) {
		return;
	}

	if (success && getImage()) {
		_texture = Rc<Texture>::create(_dynamic);
	}

	if (!_texture) {
		// Report at once, so the caller does not wait for a capture that can never arrive.
		_state = State::Failed;
		if (auto cb = sp::move(_callback)) {
			_callback = nullptr;
			cb(this);
		}
		return;
	}

	_state = State::Armed;
}

void FrameCaptureTarget::handleCaptured(bool success) {
	if (_state != State::Armed) {
		return;
	}

	_state = success ? State::Ready : State::Failed;
	if (auto cb = sp::move(_callback)) {
		_callback = nullptr;
		cb(this);
	}
}

bool FrameCapture::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> channel) {
	_application = app;
	_channel = channel;

	// Only the Vulkan backend has a copy path. Whether this surface's presented image is readable
	// arrives later, via setSurfaceSupported(), once a swapchain is configured.
	if (auto loop = _application->getGlLoop()) {
		if (auto instance = loop->getInstance()) {
			_backendSupported = instance->getApi() == core::InstanceApi::Vulkan;
		}
	}

	// XL_NO_FRAME_CAPTURE=1 disables the facility, to exercise consumers' fallback drawing.
	if (auto value = ::getenv("XL_NO_FRAME_CAPTURE")) {
		if (StringView(value) != "0") {
			_backendSupported = false;
			log::source().info("FrameCapture", "Disabled by XL_NO_FRAME_CAPTURE");
		}
	}

	return true;
}

URect FrameCapture::makeRegion(const Rect &world, const Mat4 &viewProjection, Extent2 extent) {
	const auto clip = TransformRect(world, viewProjection);

	const float w = float(extent.width);
	const float h = float(extent.height);

	// clip y = -1 is the top row, so the result is y-down without a separate flip
	const float x0 = (clip.getMinX() * 0.5f + 0.5f) * w;
	const float x1 = (clip.getMaxX() * 0.5f + 0.5f) * w;
	const float y0 = (clip.getMinY() * 0.5f + 0.5f) * h;
	const float y1 = (clip.getMaxY() * 0.5f + 0.5f) * h;

	// Round to nearest, not outward: a pixel-aligned rect must come back at exactly its size;
	// rounding out would add a row of neighbouring content on non-integral edges.
	auto x0i = int64_t(sprt::round(sprt::min(x0, x1)));
	auto y0i = int64_t(sprt::round(sprt::min(y0, y1)));
	auto x1i = int64_t(sprt::round(sprt::max(x0, x1)));
	auto y1i = int64_t(sprt::round(sprt::max(y0, y1)));

	x0i = sprt::max(int64_t(0), x0i);
	y0i = sprt::max(int64_t(0), y0i);
	x1i = sprt::min(int64_t(w), x1i);
	y1i = sprt::min(int64_t(h), y1i);

	if (x1i <= x0i || y1i <= y0i) {
		return URect();
	}

	return URect{uint32_t(x0i), uint32_t(y0i), uint32_t(x1i - x0i), uint32_t(y1i - y0i)};
}

bool FrameCapture::hasPending() const { return !_pending.empty(); }

Rc<FrameCaptureTarget> FrameCapture::request(const URect &region,
		Function<void(FrameCaptureTarget *)> &&cb) {
	if (!isAvailable()) {
		return nullptr;
	}

	auto loop = _application ? _application->getGlLoop() : nullptr;
	if (!loop) {
		return nullptr;
	}

	auto target = Rc<FrameCaptureTarget>::create(toString("FrameCapture:", _nextId++), region,
			loop->getCommonFormat());
	if (!target) {
		return nullptr;
	}

	target->_callback = sp::move(cb);

	// compileImage answers on the loop thread, but this object's state is app-thread only, hence
	// the hop. The Rc on `this` keeps the window's capture alive across it.
	_channel->compileImage(target->_dynamic,
			[self = Rc<FrameCapture>(this), target](bool success) mutable {
		self->_application->performOnAppThread([self, target, success] {
			target->handleCompiled(success);
			if (target->getState() != FrameCaptureTarget::State::Armed) {
				return;
			}

			self->_pending.emplace_back(target);

			// Scheduled here, not in request(): a frame before the image exists would copy nothing,
			// and the target would wait for another offscreen frame nobody requests.
			if (!self->_surfaceSupported) {
				if (!self->_channel->scheduleOffscreenFrame()) {
					self->_pending.pop_back();
					target->handleCaptured(false);
				}
			}
		}, self);
	});

	return target;
}

Vector<Rc<FrameCaptureTarget>> FrameCapture::takePending() {
	auto ret = sp::move(_pending);
	_pending.clear();
	return ret;
}

void FrameCapture::handleCaptured(SpanView<Rc<FrameCaptureTarget>> targets, bool success) {
	for (auto &it : targets) { it->handleCaptured(success); }
}

} // namespace stappler::xenolith
