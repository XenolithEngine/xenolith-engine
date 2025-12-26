/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLCoreImageStorage.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

ImageStorage::~ImageStorage() {
	for (auto &it : _views) {
		it.second->runReleaseCallback();
	}
	_views.clear();
	_image = nullptr;
}

bool ImageStorage::init(Rc<ImageObject> &&image) {
	_image = move(image);
	return true;
}

bool ImageStorage::isStatic() const {
	return _image && (_image->getInfo().hints & core::ImageHints::Static) == core::ImageHints::Static;
}

bool ImageStorage::isCacheable() const {
	return !_isSwapchainImage && (!_image || (_image->getInfo().hints & core::ImageHints::DoNotCache) == core::ImageHints::None);
}

bool ImageStorage::isSwapchainImage() const {
	return _isSwapchainImage;
}

void ImageStorage::cleanup() { }

const Rc<Semaphore> &ImageStorage::getWaitSem() const {
	return _waitSem;
}

const Rc<Semaphore> &ImageStorage::getSignalSem() const {
	return _signalSem;
}

uint64_t ImageStorage::getImageIndex() const {
	return _image->getIndex();
}

void ImageStorage::rearmSemaphores(Loop &loop) {
	if (isStatic()) {
		return; // no need for sync
	}

	if (_waitSem && _waitSem->isWaited()) {
		// general case
		// successfully waited on this sem
		// swap wait/signal sems
		auto tmp = move(_waitSem);
		_waitSem = nullptr;

		if (_signalSem && _signalSem->isSignaled()) {
			// successfully signaled
			if (!_signalSem->isWaited()) {
				_waitSem = move(_signalSem);
			}
		}

		_signalSem = move(tmp);
		if (!_signalSem->reset()) {
			_signalSem = nullptr;
		}
	} else if (!_waitSem) {
		// initial case, no wait sem was defined
		// try to swap signal sem to wait sem
		if (_signalSem && _signalSem->isSignaled()) {
			if (!_signalSem->isWaited()) {
				// successfully signaled
				_waitSem = move(_signalSem);
			}
		}
		_signalSem = nullptr;
	} else {
		// next frame should wait on this waitSem
		// signalSem should be unsignaled due frame processing logic
		_signalSem = nullptr;
	}

	if (!_signalSem) {
		_signalSem = loop.makeSemaphore();
	}
}

void ImageStorage::releaseSemaphore(Semaphore *) {

}

void ImageStorage::setReady(bool value) {
	if (_ready != value) {
		_ready = value;
		if (value) {
			notifyReady();
		}
	}
}

void ImageStorage::invalidate() {
	_invalid = true;
	notifyReady();
}

void ImageStorage::waitReady(Function<void(bool)> &&cb) {
	if (_invalid || isStatic()) {
		cb(false);
	}
	if (!_ready) {
		_waitReady.emplace_back(sp::move(cb));
	} else {
		cb(true);
	}
}

ImageInfoData ImageStorage::getInfo() const {
	return _image->getInfo();
}
Rc<ImageObject> ImageStorage::getImage() const {
	return _image;
}

void ImageStorage::addView(const ImageViewInfo &info, Rc<ImageView> &&view) {
	_views.emplace(info, move(view));
}

Rc<ImageView> ImageStorage::getView(const ImageViewInfo &info) const {
	auto it = _views.find(info);
	if (it != _views.end()) {
		return it->second;
	}
	return nullptr;
}

Rc<ImageView> ImageStorage::makeView(const ImageViewInfo &) {
	return nullptr;
}

void ImageStorage::setLayout(AttachmentLayout l) {
	_layout = l;
}
AttachmentLayout ImageStorage::getLayout() const {
	return _layout;
}

void ImageStorage::notifyReady() {
	for (auto &it : _waitReady) {
		it(!_invalid);
	}
	_waitReady.clear();
}

}
