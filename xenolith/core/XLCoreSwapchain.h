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
#include "XLCoreFrameDamage.h"

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

// What is handed to the platform present call. `damage` being empty means "the whole surface
// changed"; a non-empty list is a hint that only those regions differ from the previously
// presented image.
struct SP_PUBLIC PresentInfo {
	uint64_t presentWindow = 0;
	SpanView<URect> damage;
};

// Turns "current frame vs a stored snapshot of what was drawn" into a list of damaged rectangles.
//
// Two different snapshots are kept, because the two consumers ask different questions:
//
//   * per swapchain image index - "what does this particular image buffer already hold?", which is
//     what bounds a LOAD_OP_LOAD partial redraw. Diffing against a per-index snapshot rather than
//     against "the previous frame" is what makes this correct: image indexes are pooled and reused
//     out of order, and frames can be dropped after their data was built. A dropped frame simply
//     never updates a snapshot.
//
//   * the presented snapshot - "what is on screen right now?". VK_KHR_incremental_present
//     rectangles are relative to the previously presented image, not to the previous contents of
//     the image being presented, so the per-index snapshot is the wrong baseline for them: with
//     content going A -> B -> A the per-index diff can come out empty while the screen still shows
//     B, and the compositor would then never repaint.
class SP_PUBLIC SwapchainDamage {
public:
	// beyond this, the compositor gains nothing over a plain full present
	static constexpr size_t MaxRects = 8;

	void resize(uint32_t imageCount);

	// What has to be re-rendered into image `imageIndex` for it to hold this frame. Commits the
	// per-index snapshot. Returns false when the whole image must be re-rendered; an empty `out`
	// with a true return means the image already holds exactly this frame.
	bool computeRedrawArea(uint32_t imageIndex, const FrameDamageState *, Extent2 imageExtent,
			Vector<URect> &out);

	// What changed on screen. Commits the presented snapshot. Returns false when the whole surface
	// must be considered damaged; an empty `out` with a true return means the screen already shows
	// this frame.
	bool computePresentDamage(const FrameDamageState *, Extent2 imageExtent, Vector<URect> &out);

	void invalidateImage(uint32_t imageIndex);
	void invalidateAll();

protected:
	struct ImageState {
		Vector<DamageEntry> snapshot;
		bool valid = false;
	};

	// Shared body of both diffs. Callers hold the lock and have already established that `state` is
	// usable and `prev` is valid; they also commit the snapshot afterwards.
	bool diff(const ImageState &prev, const FrameDamageState *, Extent2 imageExtent,
			Vector<URect> &out);

	// Store what this frame drew as the new baseline. A frame that could not be described at all
	// (`full`) leaves the baseline unusable, so the next frame starts over from a full redraw.
	static void commit(ImageState &, const FrameDamageState *);

	// A resize invalidates every stored rectangle
	void checkExtent(Extent2 imageExtent);

	Vector<ImageState> _images;
	ImageState _presented;
	Extent2 _extent;
	mutable sprt::mutex _mutex;
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

	virtual Status present(DeviceQueue &queue, ImageStorage *, const PresentInfo &) = 0;

	SwapchainDamage &getDamage() { return _damage; }
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

	sprt::mutex _resourceMutex;
	Rc<Surface> _surface;

	// owned by the swapchain, so recreation resets the history for free
	SwapchainDamage _damage;

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

	// Relinquish the acquired image WITHOUT returning it to the swapchain. Used when the acquired image
	// is handed back to the engine's reuse pool (a frame discarded before rendering): clearing _image
	// makes the destructor's invalidateImage a no-op, so the pooled image is accounted for exactly once.
	void detachImage();

protected:
	using core::ImageStorage::init;

	uint64_t _order = 0;
	State _state = State::Initial;
	Rc<Swapchain> _swapchain;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCORESWAPCHAIN_H_ */
