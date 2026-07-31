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

// Past this share of the surface, telling the compositor "these regions changed" costs more than
// simply presenting everything.
static constexpr float DamageFullSurfaceRatio = 0.6f;

void SwapchainDamage::resize(uint32_t imageCount) {
	sprt::unique_lock lock(_mutex);
	_images.clear();
	_images.resize(imageCount);
	_presented.valid = false;
	_presented.snapshot.clear();
	_extent = Extent2(0, 0);
}

void SwapchainDamage::invalidateImage(uint32_t imageIndex) {
	sprt::unique_lock lock(_mutex);
	if (imageIndex < _images.size()) {
		_images[imageIndex].valid = false;
		_images[imageIndex].snapshot.clear();
	}
}

void SwapchainDamage::invalidateAll() {
	sprt::unique_lock lock(_mutex);
	for (auto &it : _images) {
		it.valid = false;
		it.snapshot.clear();
	}
	_presented.valid = false;
	_presented.snapshot.clear();
}

void SwapchainDamage::checkExtent(Extent2 imageExtent) {
	if (_extent == imageExtent) {
		return;
	}
	for (auto &it : _images) {
		it.valid = false;
		it.snapshot.clear();
	}
	_presented.valid = false;
	_presented.snapshot.clear();
	_extent = imageExtent;
}

void SwapchainDamage::commit(ImageState &target, const FrameDamageState *state) {
	if (state) {
		target.snapshot = state->entries;
		target.valid = !state->full;
	} else {
		target.snapshot.clear();
		target.valid = false;
	}
}

bool SwapchainDamage::computeRedrawArea(uint32_t imageIndex, const FrameDamageState *state,
		Extent2 imageExtent, Vector<URect> &out) {
	sprt::unique_lock lock(_mutex);

	if (imageIndex >= _images.size()) {
		return false;
	}

	checkExtent(imageExtent);

	auto &image = _images[imageIndex];

	if (!state || state->full || !image.valid) {
		commit(image, state);
		return false;
	}

	auto ret = diff(image, state, imageExtent, out);

	commit(image, state);
	return ret;
}

bool SwapchainDamage::computePresentDamage(const FrameDamageState *state, Extent2 imageExtent,
		Vector<URect> &out) {
	sprt::unique_lock lock(_mutex);

	checkExtent(imageExtent);

	if (!state || state->full || !_presented.valid) {
		commit(_presented, state);
		return false;
	}

	auto ret = diff(_presented, state, imageExtent, out);

	commit(_presented, state);
	return ret;
}

bool SwapchainDamage::diff(const ImageState &prev, const FrameDamageState *state,
		Extent2 imageExtent, Vector<URect> &out) {
	Vector<Rect> damage;

	auto push = [&](const Rect &r) {
		if (r.size.width > 0.0f && r.size.height > 0.0f) {
			damage.emplace_back(r);
		}
	};

	// Linear merge of the snapshot against this frame. Both sides are sorted by id, and repeated
	// ids keep their emission order, so equal runs line up pairwise.
	size_t i = 0, j = 0;
	const auto &oldEntries = prev.snapshot;
	const auto &newEntries = state->entries;
	while (i < oldEntries.size() && j < newEntries.size()) {
		const auto &o = oldEntries[i];
		const auto &n = newEntries[j];
		if (o.id < n.id) {
			push(o.bounds); // disappeared
			++i;
		} else if (n.id < o.id) {
			push(n.bounds); // appeared
			++j;
		} else {
			// Same element. A moved node keeps its generation, so the bounds have to be compared
			// too - skipping that comparison is what leaves trails on screen.
			if (o.generation != n.generation || o.signature != n.signature
					|| o.bounds != n.bounds) {
				push(o.bounds);
				push(n.bounds);
			}
			++i;
			++j;
		}
	}
	for (; i < oldEntries.size(); ++i) { push(oldEntries[i].bounds); }
	for (; j < newEntries.size(); ++j) { push(newEntries[j].bounds); }

	for (auto &it : state->alwaysDirty) { push(it); }

	if (damage.empty()) {
		out.clear();
		return true;
	}

	// Merge the pair whose union wastes the least area until the list fits.
	while (damage.size() > MaxRects) {
		size_t bestA = 0, bestB = 1;
		float bestCost = maxOf<float>();
		for (size_t a = 0; a < damage.size(); ++a) {
			for (size_t b = a + 1; b < damage.size(); ++b) {
				auto u = damage[a].unionWithRect(damage[b]);
				const float cost = u.size.width * u.size.height
						- damage[a].size.width * damage[a].size.height
						- damage[b].size.width * damage[b].size.height;
				if (cost < bestCost) {
					bestCost = cost;
					bestA = a;
					bestB = b;
				}
			}
		}
		damage[bestA] = damage[bestA].unionWithRect(damage[bestB]);
		damage.erase(damage.begin() + bestB);
	}

	const float surface = float(imageExtent.width) * float(imageExtent.height);
	float covered = 0.0f;
	for (auto &it : damage) { covered += it.size.width * it.size.height; }
	if (surface <= 0.0f || covered > surface * DamageFullSurfaceRatio) {
		return false;
	}

	// Round outward and pad by a pixel: the boxes are derived from pre-rasterization geometry, so
	// antialiasing and rounding can touch just outside them.
	out.clear();
	out.reserve(damage.size());
	for (auto &it : damage) {
		const float x0 = sprt::max(0.0f, sprt::floor(it.getMinX()) - 1.0f);
		const float y0 = sprt::max(0.0f, sprt::floor(it.getMinY()) - 1.0f);
		const float x1 = sprt::min(float(imageExtent.width), sprt::ceil(it.getMaxX()) + 1.0f);
		const float y1 = sprt::min(float(imageExtent.height), sprt::ceil(it.getMaxY()) + 1.0f);
		if (x1 > x0 && y1 > y0) {
			out.emplace_back(URect{uint32_t(x0), uint32_t(y0), uint32_t(x1 - x0), uint32_t(y1 - y0)});
		}
	}

	return true;
}

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
	if (_swapchain) {
		if (_image) {
			_swapchain->invalidateImage(this, false);
		}
	}
	_swapchain = nullptr;
	_image = nullptr;
	_state = State::Presented;
}

void SwapchainImage::detachImage() {
	// Hand the acquired image off to the engine's reuse pool: relinquish our references WITHOUT
	// releasing the image to the swapchain (otherwise it would be returned twice -- once here and once
	// when the pooled image is finally presented). The acquire (wait) semaphore travels with the pooled
	// SwapchainAcquiredImage; only return the unused signal semaphore reserved in setImage.
	if (_signalSem && _swapchain) {
		_swapchain->releaseSemaphore(sp::move(_signalSem));
	}
	_swapchain = nullptr;
	_image = nullptr;
	_waitSem = nullptr;
	_signalSem = nullptr;
	_state = State::Presented;
}

} // namespace stappler::xenolith::core
