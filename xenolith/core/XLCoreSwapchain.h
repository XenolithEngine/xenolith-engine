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

#ifndef XENOLITH_CORE_XLCORESWAPCHAIN_H_
#define XENOLITH_CORE_XLCORESWAPCHAIN_H_

#include "XLCoreInstance.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class SP_PUBLIC Surface : public Ref {
public:
	virtual ~Surface() = default;

	virtual bool init(Instance *, Ref *);

	virtual void invalidate() = 0;

	Instance *getInstance() const { return _instance; }

	virtual SurfaceInfo getSurfaceOptions(const Device &, FullScreenExclusiveMode,
			void *) const = 0;

protected:
	Rc<Ref> _window;
	Rc<Instance> _instance;
};

class SP_PUBLIC Swapchain : public Object {
public:
	struct SwapchainImageData {
		Rc<ImageObject> image;
		Map<ImageViewInfo, Rc<ImageView>> views;
	};

	struct SwapchainData {
		Vector<SwapchainImageData> images;
		Vector<Rc<Semaphore>> semaphores;
		Vector<Rc<Semaphore>> presentSemaphores;

		void invalidate(Device &);
	};

	struct SwapchainAcquiredImage : public Ref {
		uint32_t imageIndex;
		const SwapchainImageData *data;
		Rc<Semaphore> sem;
		Rc<Swapchain> swapchain;

		SwapchainAcquiredImage(uint32_t idx, const SwapchainImageData *data, Rc<Semaphore> &&sem,
				Rc<Swapchain> &&s)
		: imageIndex(idx), data(data), sem(move(sem)), swapchain(move(s)) { }
	};

	virtual ~Swapchain();

	PresentMode getPresentMode() const { return _presentMode; }
	const ImageInfo &getImageInfo() const { return _imageInfo; }
	const SwapchainConfig &getConfig() const { return _config; }
	const SurfaceInfo &getSurfaceInfo() const { return _surfaceInfo; }

	uint32_t getAcquiredImagesCount() const { return _acquiredImages; }
	uint64_t getPresentedFramesCount() const { return _presentedFrames; }

	bool isDeprecated();
	bool isOptimal() const;
	bool isValid() const;
	bool isExclusiveFullscreen() const { return _fullscreenExclusive; }

	// returns true if it was first deprecation
	bool deprecate();

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<Fence> &fence, Status &) = 0;

	virtual Status present(DeviceQueue &queue, ImageStorage *, uint64_t presentWindow) = 0;
	virtual void invalidateImage(const ImageStorage *, bool release) = 0;
	virtual void invalidateImage(uint32_t, bool release) = 0;

	virtual Rc<core::ImageView> makeView(const Rc<core::ImageObject> &, const ImageViewInfo &) = 0;

	virtual Rc<Semaphore> acquireSemaphore() = 0;
	virtual bool releaseSemaphore(Rc<Semaphore> &&) = 0;

protected:
	using core::Object::init;

	ImageViewInfo getSwapchainImageViewInfo(const ImageInfo &image) const;

	bool _deprecated = false; // should we recreate swapchain
	bool _invalid = false; // can we present images with this swapchain
	bool _fullscreenExclusive = false;
	core::PresentMode _presentMode = core::PresentMode::Unsupported;
	ImageInfo _imageInfo;
	core::SurfaceInfo _surfaceInfo;
	core::SwapchainConfig _config;
	uint32_t _acquiredImages = 0;
	uint64_t _presentedFrames = 0;
	uint64_t _presentTime = 0;

	Mutex _resourceMutex;
	Rc<Surface> _surface;

	Vector<Rc<Semaphore>> _invalidatedSemaphores;
};

class SP_PUBLIC SwapchainImage : public ImageStorage {
public:
	enum class State {
		Initial,
		Submitted,
		Presented,
	};

	virtual ~SwapchainImage();

	virtual bool init(Swapchain *, uint64_t frameOrder);
	virtual bool init(Swapchain *, const Swapchain::SwapchainImageData &, Rc<Semaphore> &&);

	virtual void cleanup() override;
	virtual void rearmSemaphores(core::Loop &) override;
	virtual void releaseSemaphore(core::Semaphore *) override;

	virtual bool isSemaphorePersistent() const override { return false; }

	virtual ImageInfoData getInfo() const override;

	virtual Rc<core::ImageView> makeView(const ImageViewInfo &) override;

	void setImage(Rc<Swapchain> &&, const Swapchain::SwapchainImageData &, const Rc<Semaphore> &);

	uint64_t getOrder() const { return _order; }

	void setPresented();
	bool isPresented() const { return _state == State::Presented; }
	bool isSubmitted() const { return _state == State::Submitted || _state == State::Presented; }

	const Rc<Swapchain> &getSwapchain() const { return _swapchain; }

	void invalidateImage();

protected:
	using core::ImageStorage::init;

	uint64_t _order = 0;
	State _state = State::Initial;
	Rc<Swapchain> _swapchain;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCORESWAPCHAIN_H_ */
