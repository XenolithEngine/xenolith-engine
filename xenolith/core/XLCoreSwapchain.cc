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

#include "XLCoreSwapchain.h"
#include "XLCoreDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool Surface::init(Instance *instance, Ref *win) {
	_instance = instance;
	_window = win;
	return true;
}

void Swapchain::SwapchainData::invalidate(Device &dev) {
	for (auto &it : images) {
		for (auto &v : it.views) {
			if (v.second) {
				v.second->runReleaseCallback();
				v.second->invalidate();
				v.second = nullptr;
			}
		}
	}

	semaphores.clear();

	for (auto &it : presentSemaphores) {
		if (it) {
			dev.invalidateSemaphore(move(it));
			it = nullptr;
		}
	}

	presentSemaphores.clear();
}

Swapchain::~Swapchain() {
	invalidate();
	_surface = nullptr;
}

bool Swapchain::isDeprecated() { return _deprecated; }

bool Swapchain::isOptimal() const { return _presentMode == _config.presentMode; }

bool Swapchain::isValid() const { return !_invalid; }

bool Swapchain::deprecate() {
	auto tmp = _deprecated;
	_deprecated = true;
	return !tmp;
}

ImageViewInfo Swapchain::getSwapchainImageViewInfo(const ImageInfo &image) const {
	ImageViewInfo info;
	switch (image.imageType) {
	case core::ImageType::Image1D: info.type = core::ImageViewType::ImageView1D; break;
	case core::ImageType::Image2D: info.type = core::ImageViewType::ImageView2D; break;
	case core::ImageType::Image3D: info.type = core::ImageViewType::ImageView3D; break;
	}

	return image.getViewInfo(info);
}

SwapchainImage::~SwapchainImage() {
	if (_image && _swapchain) {
		_swapchain->invalidateImage(this, false);
		_image = nullptr;
	}
	if (_state == State::Presented) {
		if (_swapchain && _waitSem) {
			_swapchain->releaseSemaphore((Semaphore *)_waitSem.get());
		}
	}

	_swapchain = nullptr;
	// prevent views from released
	_views.clear();

	_waitSem = nullptr;
	_signalSem = nullptr;
}

bool SwapchainImage::init(Swapchain *swapchain, uint64_t order) {
	_swapchain = swapchain;
	_order = order;
	_state = State::Submitted;
	_isSwapchainImage = true;
	return true;
}

bool SwapchainImage::init(Swapchain *swapchain, const Swapchain::SwapchainImageData &image,
		Rc<Semaphore> &&sem) {
	_swapchain = swapchain;
	_image = image.image.get();
	for (auto &it : image.views) { _views.emplace(it.first, it.second); }
	if (sem) {
		_waitSem = sem.get();
	}
	_signalSem = _swapchain->acquireSemaphore().get();
	_state = State::Submitted;
	_isSwapchainImage = true;
	return true;
}

void SwapchainImage::cleanup() {
	//stappler::log::source().info("SwapchainImage", "cleanup");
}

void SwapchainImage::rearmSemaphores(core::Loop &loop) { ImageStorage::rearmSemaphores(loop); }

void SwapchainImage::releaseSemaphore(core::Semaphore *sem) {
	if (_state == State::Presented && sem == _waitSem && _swapchain) {
		// work on last submit is over, wait sem no longer in use
		if (_swapchain->releaseSemaphore((Semaphore *)sem)) {
			_waitSem = nullptr;
		}
	}
}

ImageInfoData SwapchainImage::getInfo() const {
	if (_image) {
		return _image->getInfo();
	} else if (_swapchain) {
		return _swapchain->getImageInfo();
	}
	return ImageInfoData();
}

Rc<core::ImageView> SwapchainImage::makeView(const ImageViewInfo &info) {
	auto it = _views.find(info);
	if (it != _views.end()) {
		return it->second;
	}

	it = _views.emplace(info, _swapchain->makeView(_image, info)).first;
	return it->second;
}

void SwapchainImage::setImage(Rc<Swapchain> &&handle, const Swapchain::SwapchainImageData &image,
		const Rc<Semaphore> &sem) {
	_image = image.image.get();
	for (auto &it : image.views) { _views.emplace(it.first, it.second); }
	if (sem) {
		_waitSem = sem.get();
	}
	_signalSem = _swapchain->acquireSemaphore().get();
}

void SwapchainImage::setPresented() {
	_state = State::Presented;
	_image = nullptr;
}

void SwapchainImage::invalidateImage() {
	if (_image && _swapchain) {
		_swapchain->invalidateImage(this, false);
	}
	_swapchain = nullptr;
	_image = nullptr;
	_state = State::Presented;
}

} // namespace stappler::xenolith::core
