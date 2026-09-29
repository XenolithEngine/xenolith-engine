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

#include "XLVkHeadlessPresentation.h"
#include "XLVkDevice.h"
#include "XLVkAllocator.h"
#include "XLVkLoop.h"
#include "XLCoreFrameCache.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

/* The one barrier that moves a presented image between its writer and its readers (see
HeadlessSwapchain). A task of its own, submitted after the frame's fence: the frame is finished by
then, and nothing else touches the image while the slot is pinned. */
class PlaneLayoutTask : public core::DeviceQueueTask {
public:
	virtual ~PlaneLayoutTask() = default;

	// `toReaders`: PresentSrc -> ShaderReadOnly as the frame is published; otherwise back, as the
	// last reader lets go.
	bool init(Rc<Image> &&image, bool toReaders, Function<void(bool)> &&cb) {
		if (!DeviceQueueTask::init(vk::getQueueFlags(image->getInfo().type))) {
			return false;
		}
		_image = sp::move(image);
		_toReaders = toReaders;
		_callback = sp::move(cb);
		return true;
	}

	virtual bool handleQueueAcquired(core::Device &, core::DeviceQueue &) override { return true; }

	virtual void fillCommandBuffer(core::Device &, core::CommandBuffer &cbuf) override {
		auto &buf = static_cast<CommandBuffer &>(cbuf);
		if (_toReaders) {
			// What the render pass wrote becomes visible to whatever samples the image.
			auto barrier = ImageMemoryBarrier(_image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
					makeSpanView(&barrier, 1));
		} else {
			// Every read is done before the image is drawn into again - a partial redraw LOADs it.
			auto barrier = ImageMemoryBarrier(_image, VK_ACCESS_SHADER_READ_BIT,
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
							| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, makeSpanView(&barrier, 1));
		}
	}

	virtual void handleComplete(bool success) override {
		if (_callback) {
			_callback(success);
		}
	}

protected:
	Rc<Image> _image;
	bool _toReaders = true;
	Function<void(bool)> _callback;
};

/* Copies what a frame changed in a presented image into a host-visible buffer laid out like the
whole image (rows of the image's width), for HeadlessSwapchain's output. Submitted after the frame
on the same queue - lavapipe, the case this is for, has exactly one. The image leaves and comes
back in PresentSrc: the next partial redraw into the slot LOADs it. */
class OutputCopyTask : public core::DeviceQueueTask {
public:
	virtual ~OutputCopyTask() = default;

	bool init(Rc<Image> &&image, Rc<Buffer> &&buffer, SpanView<URect> damage,
			Function<void(bool)> &&cb) {
		if (!DeviceQueueTask::init(vk::getQueueFlags(image->getInfo().type))) {
			return false;
		}
		_image = sp::move(image);
		_buffer = sp::move(buffer);
		_damage.assign(damage.begin(), damage.end());
		_callback = sp::move(cb);
		return true;
	}

	virtual bool handleQueueAcquired(core::Device &, core::DeviceQueue &) override { return true; }

	virtual void fillCommandBuffer(core::Device &, core::CommandBuffer &cbuf) override {
		auto &buf = static_cast<CommandBuffer &>(cbuf);

		auto toTransfer = ImageMemoryBarrier(_image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, 0, makeSpanView(&toTransfer, 1));

		auto &info = _image->getInfo();
		if (_damage.empty()) {
			buf.cmdCopyImageToBuffer(_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _buffer, 0);
		} else {
			const auto pixel = core::getFormatImageSize(info.format, Extent3(1, 1, 1));
			Vector<VkBufferImageCopy> regions;
			regions.reserve(_damage.size());
			for (auto &r : _damage) {
				// bufferOffset puts the rect where it lies in the whole image, so the buffer keeps
				// the image's layout whatever part of it was copied.
				regions.emplace_back(VkBufferImageCopy{
					.bufferOffset = (VkDeviceSize(r.y) * info.extent.width + r.x) * pixel,
					.bufferRowLength = info.extent.width,
					.bufferImageHeight = info.extent.height,
					.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
					.imageOffset = {int32_t(r.x), int32_t(r.y), 0},
					.imageExtent = {r.width, r.height, 1},
				});
			}
			buf.cmdCopyImageToBuffer(_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _buffer,
					regions);
		}

		BufferMemoryBarrier toHost(_buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
				makeSpanView(&toHost, 1));

		auto back = ImageMemoryBarrier(_image, VK_ACCESS_TRANSFER_READ_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, makeSpanView(&back, 1));
	}

	virtual void handleComplete(bool success) override {
		if (_callback) {
			_callback(success);
		}
	}

protected:
	Rc<Image> _image;
	Rc<Buffer> _buffer;
	Vector<URect> _damage;
	Function<void(bool)> _callback;
};

// The rows a frame changed, from the copy of the whole image into the output's buffer, which keeps
// whatever the previous frames left outside them.
static void writeOutput(sprt::window::SoftwareSwapchain *output, const uint8_t *src,
		Extent2 extent, size_t pixel, SpanView<URect> damage) {
	Status status = Status::Ok;
	auto index = output->acquire(status);
	if (status != Status::Ok) {
		log::source().warn("vk::HeadlessSwapchain", "Output has no free buffer, frame dropped");
		return;
	}

	auto &dst = output->getBuffers()[index];
	auto copyRect = [&](const URect &r) {
		for (uint32_t y = r.y; y < r.y + r.height; ++y) {
			auto offset = size_t(y) * dst.stride + size_t(r.x) * pixel;
			if (offset + size_t(r.width) * pixel > dst.size) {
				break;
			}
			::memcpy(dst.data + offset, src + (size_t(y) * extent.width + r.x) * pixel,
					size_t(r.width) * pixel);
		}
	};

	if (damage.empty()) {
		copyRect(URect(0, 0, extent.width, extent.height));
	} else {
		for (auto &r : damage) { copyRect(r); }
	}

	output->present(index, damage);
}

HeadlessSurface::~HeadlessSurface() { }

bool HeadlessSurface::init(Instance *instance, Extent2 extent, Ref *win,
		Rc<sprt::window::SoftwareSurface> &&output) {
	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_extent = extent;
	_output = sp::move(output);
	return true;
}

void HeadlessSurface::invalidate() {
	if (_output) {
		_output->invalidate();
		_output = nullptr;
	}
	_window = nullptr;
}

core::SurfaceInfo HeadlessSurface::getSurfaceOptions(const core::Device &, //
		core::FullScreenExclusiveMode, void *) const {
	core::SurfaceInfo info;

	// A pseudo-swapchain is not bounded by a compositor: one image is enough to render, and the
	// engine's default of three is well within what we can allocate.
	info.minImageCount = 1;
	info.maxImageCount = 8;

	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	info.maxImageArrayLayers = 1;

	/* Premultiplied as well as Opaque, though there is no compositor: captureImage reads alpha too,
	so a shaped window (e.g. a rounded menu) must render the same pixels as when windowed. */
	info.supportedCompositeAlpha =
			core::CompositeAlphaFlags::Opaque | core::CompositeAlphaFlags::Premultiplied;
	info.supportedTransforms = core::SurfaceTransformFlags::Identity;
	info.currentTransform = core::SurfaceTransformFlags::Identity;

	// TransferSrc is the whole point: whatever is rendered must stay readable through
	// Loop::captureImage without an extra offscreen pass.
	info.supportedUsageFlags = core::ImageUsage::ColorAttachment | core::ImageUsage::TransferSrc
			| core::ImageUsage::TransferDst | core::ImageUsage::Sampled;

	// B8G8R8A8 first, so a capture matches what a real swapchain would have produced on the same
	// machine (core::getBitmap already un-swizzles it).
	info.formats.emplace_back(core::ImageFormat::B8G8R8A8_UNORM,
			core::ColorSpace::SRGB_NONLINEAR_KHR);
	info.formats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM,
			core::ColorSpace::SRGB_NONLINEAR_KHR);

	// Nothing paces us, so every mode is "present immediately".
	info.presentModes.emplace_back(core::PresentMode::Immediate);
	info.presentModes.emplace_back(core::PresentMode::Fifo);
	info.presentModes.emplace_back(core::PresentMode::Mailbox);

	if (_output) {
		// The output's buffers are the screen: they decide the extent, the formats and the pacing.
		// The images are still ours, so the engine picks how many.
		info.formats.clear();
		info.presentModes.clear();
		info = _output->getSurfaceOptions(sp::move(info));
		info.minImageCount = 1;
		info.maxImageCount = 8;
	}

	return info;
}

HeadlessSwapchain::~HeadlessSwapchain() {
	// A frame still held by a reader outlives this generation; releasing it must not reach into a
	// ring that no longer exists.
	if (_slots) {
		_slots->retire();
	}
	invalidateViews();
}

void HeadlessSwapchain::invalidateViews() {
	// Mirrors core::Swapchain::SwapchainData::invalidate: the frame cache is told the view is gone
	// while everything is still alive, and the framebuffers that referenced it are retired in an
	// orderly way. A Framebuffer holding an Rc to the view keeps the object itself alive, which is
	// what makes this safe to do before the images are dropped.
	for (auto &it : _images) {
		for (auto &v : it.views) {
			if (v.second) {
				v.second->runReleaseCallback();
				v.second->invalidate();
				v.second = nullptr;
			}
		}
		it.views.clear();
	}
}

bool HeadlessSwapchain::init(Device &dev, NotNull<core::Loop> loop, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, HeadlessSurface *surface) {
	// AppWindow asks for ColorAttachment [| TransferDst]; TransferSrc is what turns these into
	// always-capturable buffers.
	swapchainImageInfo.usage |= core::ImageUsage::TransferSrc;

	auto imageCount = sprt::max(cfg.imageCount, uint32_t(1));
	auto viewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

	_images.reserve(imageCount);
	for (uint32_t i = 0; i < imageCount; ++i) {
		auto image = dev.getAllocator()->spawnPersistent(AllocationUsage::DeviceLocal,
				toString("HeadlessSwapchainImage[", i, "]"), swapchainImageInfo, false);
		if (!image) {
			log::source().error("vk::HeadlessSwapchain", "Fail to allocate image ", i);
			return false;
		}

		auto view = Rc<ImageView>::create(dev, image.get(), viewInfo);
		if (!view) {
			log::source().error("vk::HeadlessSwapchain", "Fail to create view for image ", i);
			return false;
		}

		Map<ImageViewInfo, Rc<core::ImageView>> views;
		views.emplace(viewInfo, sp::move(view));

		_images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
	}

	if (auto output = surface ? surface->getOutput() : nullptr) {
		const auto &extent = swapchainImageInfo.extent;
		_output = output->makeSwapchain(sprt::window::SoftwareSwapchainInfo{
			Extent2(extent.width, extent.height), swapchainImageInfo.format, 1});
		if (!_output) {
			log::source().error("vk::HeadlessSwapchain", "Output refused the swapchain");
			return false;
		}

		const auto size = core::getFormatImageSize(swapchainImageInfo.format, extent);
		_outputBuffers.reserve(imageCount);
		for (uint32_t i = 0; i < imageCount; ++i) {
			auto buffer = dev.getAllocator()->spawnPersistent(
					AllocationUsage::HostTransitionDestination,
					BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferDst), size_t(size),
							swapchainImageInfo.type));
			if (!buffer) {
				log::source().error("vk::HeadlessSwapchain", "Fail to allocate output buffer ", i);
				return false;
			}
			_outputBuffers.emplace_back(sp::move(buffer));
		}
	}

	_acquired.resize(imageCount, false);
	_slots = Rc<core::PlaneSlotTable>::create(imageCount);
	_loop = loop;

	// The images are freshly allocated with undefined content, so the first frame into each slot
	// must report full damage. The tracker is keyed by slot (SwapchainImage::getSwapchainSlot), not
	// by these images' object ids.
	_damage.resize(imageCount);

	_presentMode = presentMode;
	_imageInfo = move(swapchainImageInfo);
	_config = cfg;
	_config.imageCount = imageCount;
	_surface = surface;
	_surfaceInfo = info;

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Swapchain, core::ObjectHandle::zero());
}

auto HeadlessSwapchain::acquire(bool lockfree, const Rc<core::Fence> &fence, Status &status)
		-> Rc<SwapchainAcquiredImage> {
	if (_deprecated || _invalid) {
		status = Status::ErrorCancelled;
		return nullptr;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	auto count = uint32_t(_images.size());
	for (uint32_t i = 0; i < count; ++i) {
		auto index = (_nextIndex + i) % count;
		// A pinned slot holds a frame somebody is still reading.
		if (_acquired[index] || _slots->isPinned(index)) {
			continue;
		}

		_acquired[index] = true;
		_nextIndex = (index + 1) % count;
		++_acquiredImages;

		status = Status::Ok;

		// No wait semaphore: acquisition is instantaneous and nothing on the GPU produced this
		// image. Handing out a freshly created (unsignalled) binary semaphore here would put it in
		// pWaitSemaphores and hang the submit forever.
		return Rc<SwapchainAcquiredImage>::alloc(index, &_images[index], nullptr, this);
	}

	// Every image is in flight - the engine retries through its acquisition timer.
	status = Status::Timeout;
	return nullptr;
}

Status HeadlessSwapchain::present(core::DeviceQueue *, core::ImageStorage *image,
		const core::PresentInfo &info) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	Rc<Image> published;
	uint32_t publishedSlot = maxOf<uint32_t>();
	Rc<Image> output;
	uint32_t outputSlot = maxOf<uint32_t>();

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (image) {
		// ImageStorage::getImageIndex is the underlying ImageObject's global id, not a slot
		// number, so the slot has to be looked up rather than indexed
		auto id = image->getImageIndex();
		for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
			if (_images[i].image && _images[i].image->getIndex() == id) {
				_acquired[i] = false;
				// This is now "what is on screen": the screenshot command reads it back directly
				// instead of rendering another frame.
				_lastPresentedIndex = i;

				if (_planeSource && _slots->pin(i)) {
					published = static_cast<Image *>(_images[i].image.get());
					publishedSlot = i;
				} else if (_output && _slots->pin(i)) {
					output = static_cast<Image *>(_images[i].image.get());
					outputSlot = i;
				}
				break;
			}
		}

		// The per-frame storage lets go of the slot here: without it, its destructor would
		// invalidate the slot again later - by then possibly a different frame's.
		if (image->isSwapchainImage()) {
			static_cast<core::SwapchainImage *>(image)->setPresented();
		}
	}

	if (_acquiredImages > 0) {
		--_acquiredImages;
	}
	++_presentedFrames;
	_presentTime = sp::platform::clock(ClockType::Monotonic);

	lock.unlock();

	if (published) {
		publishPlaneFrame(publishedSlot, sp::move(published));
	}

	if (output) {
		// Clipped to the image here, so the copy and the rows agree. Empty is the whole image.
		auto &extent = output->getInfo().extent;
		Vector<URect> damage;
		for (auto &r : info.damage) {
			if (r.x >= extent.width || r.y >= extent.height) {
				continue;
			}
			damage.emplace_back(URect(r.x, r.y, sprt::min(r.width, extent.width - r.x),
					sprt::min(r.height, extent.height - r.y)));
		}
		if (!info.damage.empty() && damage.empty()) {
			_slots->unpin(outputSlot); // nothing on the screen changed
		} else {
			presentToOutput(outputSlot, sp::move(output), sp::move(damage));
		}
	}

	return Status::Ok;
}

void HeadlessSwapchain::presentToOutput(uint32_t slot, Rc<Image> &&image, Vector<URect> &&damage) {
	auto loop = Rc<core::Loop>(_loop);
	auto device = Rc<Device>(static_cast<Device *>(_object.device));
	auto slots = _slots;
	auto output = _output;
	auto buffer = _outputBuffers[slot];

	if (device->isDeviceLost() || !loop->isRunning()) {
		slots->unpin(slot);
		return;
	}

	auto &info = image->getInfo();
	auto extent = Extent2(info.extent.width, info.extent.height);
	auto pixel = size_t(core::getFormatImageSize(info.format, Extent3(1, 1, 1)));

	device->runTask(*loop,
			Rc<OutputCopyTask>::create(sp::move(image), Rc<Buffer>(buffer), damage,
					[slots, slot, output, buffer, extent, pixel, damage](bool success) {
		// A retired generation (a resize) has nothing on the screen to update any more.
		if (success && !slots->isRetired()) {
			buffer->map([&](uint8_t *data, VkDeviceSize) {
				writeOutput(output, data, extent, pixel, damage);
			}, DeviceMemoryAccess::Invalidate);
		}
		slots->unpin(slot);
	}));
}

void HeadlessSwapchain::attachPlaneSource(NotNull<core::PlaneSource> source) {
	_planeSource = source.get();
	_planeSource->setSlotTable(Rc<core::PlaneSlotTable>(_slots));
}

void HeadlessSwapchain::publishPlaneFrame(uint32_t slot, Rc<Image> &&image) {
	auto loop = Rc<core::Loop>(_loop);
	auto device = Rc<Device>(static_cast<Device *>(_object.device));
	auto source = _planeSource;
	auto slots = _slots;
	auto serial = source->acquireSerial();

	/* The way back, run by whoever drops the last reference. Everything it needs is captured: the
	frame can outlive this swapchain (a resize) and be let go from any thread. A retired generation
	has no ring to return the image to, and a lost device runs no tasks. */
	auto release = [loop, device, slots, image, slot] {
		loop->performOnThread([loop, device, slots, image, slot]() mutable {
			if (slots->isRetired() || device->isDeviceLost() || !loop->isRunning()) {
				slots->unpin(slot);
				return;
			}
			device->runTask(*loop,
					Rc<PlaneLayoutTask>::create(sp::move(image), false,
							[slots, slot](bool) { slots->unpin(slot); }));
		});
	};

	// Published only once readable: the frame is announced after its image reached ShaderReadOnly.
	device->runTask(*loop,
			Rc<PlaneLayoutTask>::create(Rc<Image>(image), true,
					[source, serial, slot, image, slots, release = sp::move(release)](
							bool success) mutable {
		auto frame = Rc<core::PlaneFrame>::create(serial, slot, Rc<core::ImageObject>(image.get()),
				sp::move(slots), sp::move(release));
		if (success) {
			source->publish(sp::move(frame));
		}
		// On failure the frame goes away here, and its release returns the slot.
	}));
}

void HeadlessSwapchain::invalidateImage(const core::ImageStorage *image, bool) {
	if (!image) {
		return;
	}

	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	auto id = image->getImageIndex();
	for (uint32_t i = 0; i < uint32_t(_images.size()); ++i) {
		if (_images[i].image && _images[i].image->getIndex() == id) {
			if (_acquired[i]) {
				_acquired[i] = false;
				if (_acquiredImages > 0) {
					--_acquiredImages;
				}
			}
			break;
		}
	}
}

void HeadlessSwapchain::invalidateImage(uint32_t index, bool) {
	sprt::unique_lock<sprt::mutex> lock(_resourceMutex);

	if (index < _acquired.size() && _acquired[index]) {
		_acquired[index] = false;
		if (_acquiredImages > 0) {
			--_acquiredImages;
		}
	}
}

Rc<core::ImageView> HeadlessSwapchain::makeView(const Rc<core::ImageObject> &image,
		const ImageViewInfo &info) {
	auto dev = static_cast<Device *>(_object.device);
	auto view = Rc<ImageView>::create(*dev, static_cast<Image *>(image.get()), info);
	if (!view) {
		return nullptr;
	}

	for (auto &it : _images) {
		if (it.image == image) {
			it.views.emplace(info, view);
			break;
		}
	}

	return view;
}

Rc<core::Semaphore> HeadlessSwapchain::acquireSemaphore() {
	// Nothing waits on the render's completion here - present is a bookkeeping no-op - so the
	// frame is submitted without a signal semaphore at all.
	return nullptr;
}

bool HeadlessSwapchain::releaseSemaphore(Rc<core::Semaphore> &&) { return true; }

core::ImageObject *HeadlessSwapchain::getLastPresentedImage() const {
	if (_lastPresentedIndex >= _images.size()) {
		return nullptr;
	}
	return _images[_lastPresentedIndex].image.get();
}

Status HeadlessPresentationEngine::setFullscreenSurface(const core::MonitorId &,
		const core::ModeInfo &) {
	return Status::ErrorNotSupported;
}

bool HeadlessPresentationEngine::createSwapchain(const core::SurfaceInfo &info,
		core::SwapchainConfig &&cfg, core::PresentMode presentMode, bool) {
	auto dev = static_cast<Device *>(_device);

	auto surface = _surface.get_cast<HeadlessSurface>();
	if (!surface) {
		log::source().error("vk::HeadlessPresentationEngine", "No headless surface bound");
		return false;
	}

	// The swapchain owns its images outright, so there is no old-swapchain handoff to do. Retire it
	// explicitly (views first, see invalidateViews) so the frame cache is unwound while everything
	// is still alive, then let it go.
	auto oldSwapchain = move(_swapchain);
	if (oldSwapchain) {
		if (oldSwapchain->getAcquiredImagesCount() != 0) {
			log::source().warn("vk::HeadlessPresentationEngine",
					"Some swapchain images still active");
		}
		oldSwapchain.get_cast<HeadlessSwapchain>()->invalidateViews();
		oldSwapchain = nullptr;
	}

	surface->setExtent(cfg.extent);

	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);

	_swapchain = Rc<HeadlessSwapchain>::create(*dev, _loop, info, cfg, move(swapchainImageInfo),
			presentMode, surface);
	if (!_swapchain) {
		log::source().error("vk::HeadlessPresentationEngine", "Fail to create swapchain");
		return false;
	}

	if (auto source = _window->getPlaneSource()) {
		_swapchain.get_cast<HeadlessSwapchain>()->attachPlaneSource(source);
	}

	auto newConstraints = _window->exportConstraints(_serial);
	newConstraints.extent = Extent3(cfg.extent, 1);
	newConstraints.transform = cfg.transform;

	_constraints = sp::move(newConstraints);

	Vector<uint64_t> ids;
	auto cache = _loop->getFrameCache();
	for (auto &it : _swapchain.get_cast<HeadlessSwapchain>()->getImages()) {
		for (auto &iit : it.views) {
			auto id = iit.second->getIndex();
			ids.emplace_back(id);
			// the loop keeps the frame cache alive for as long as a view can outlive us
			iit.second->setReleaseCallback(
					[loop = _loop, cache, id] { cache->removeImageView(id); });
		}
	}

	for (auto &id : ids) { cache->addImageView(id); }

	handleSwapchainUpdated(_constraints);
	_waitForDisplayLink = false;
	_readyForNextFrame = true;
	return true;
}

void HeadlessPresentationEngine::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	/* A window read as a plane is captured as the compositor sees it: the latest published frame,
	in ShaderReadOnly. The copy moves the layout and back, so it takes the frame alone: compositor
	frames sampling it finish first, and the next ones wait for the copy. */
	if (auto source = _window->getPlaneSource()) {
		if (auto frame = source->getLatest()) {
			frame->lock(core::PlaneFrameAccess::Exclusive, _loop,
					[loop = _loop, cb = sp::move(cb)](Rc<core::PlaneFrameLock> &&lock) mutable {
				auto image = Rc<core::ImageObject>(lock->getFrame()->getImage());
				loop->captureImage(
						[cb = sp::move(cb), lock = sp::move(lock)](const core::ImageInfoData &info,
								BytesView view) mutable {
					cb(info, view);
					lock = nullptr;
				},
						image, core::AttachmentLayout::ShaderReadOnlyOptimal);
			});
			return;
		}
	}

	auto swapchain = _swapchain.get_cast<HeadlessSwapchain>();
	auto image = swapchain ? swapchain->getLastPresentedImage() : nullptr;

	if (!image) {
		// Nothing has been presented yet - fall back to rendering a dedicated offscreen frame.
		core::PresentationEngine::captureScreenshot(sp::move(cb));
		return;
	}

	/* The pseudo-swapchain image still holds the last presented frame and carries TransferSrc, so
	it can be read back as-is. A swapchain image's render pass leaves it in PresentSrc (the
	PresentSrc -> TransferSrcOptimal remap in FrameQueue only applies to non-swapchain images).

	The slot is pinned for the copy: the read moves the layout, and the next frame into the slot
	would start from PRESENT_SRC and LOAD it. A slot already pinned (another read in flight) is read
	as it is. */
	Rc<core::PlaneSlotTable> slots;
	auto slot = swapchain->getLastPresentedSlot();
	if (swapchain->getSlotTable()->pin(slot)) {
		slots = swapchain->getSlotTable();
	}
	_loop->captureImage(
			[cb = sp::move(cb), slots = sp::move(slots), slot](const core::ImageInfoData &info,
					BytesView view) mutable {
		cb(info, view);
		if (slots) {
			slots->unpin(slot);
		}
	},
			Rc<core::ImageObject>(image), core::AttachmentLayout::PresentSrc);
}

} // namespace stappler::xenolith::vk
