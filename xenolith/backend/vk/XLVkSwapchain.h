/**
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

#ifndef XENOLITH_BACKEND_VK_XLVKSWAPCHAIN_H_
#define XENOLITH_BACKEND_VK_XLVKSWAPCHAIN_H_

#include "XLVkDevice.h"
#include "XLVkObject.h"
#include "XLCoreSwapchain.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SP_PUBLIC Surface : public core::Surface {
public:
	virtual ~Surface();

	bool init(Instance *instance, VkSurfaceKHR surface, Ref * = nullptr);

	virtual void invalidate() override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &, core::FullScreenExclusiveMode,
			void *) const override;

	VkSurfaceKHR getSurface() const { return _surface; }

protected:
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
};

class SP_PUBLIC SwapchainHandle : public core::Swapchain {
public:
	struct SwapchainHandleData : SwapchainData {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	};

	virtual ~SwapchainHandle() = default;

	bool init(Device &dev, const core::SurfaceInfo &, const core::SwapchainConfig &, ImageInfo &&,
			core::PresentMode, Surface *, uint32_t families[2], SwapchainHandle *);

	bool enableExclusiveFullscreen(Device &dev);

	VkSwapchainKHR getSwapchain() const { return _data->swapchain; }

	SpanView<SwapchainImageData> getImages() const;

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<core::Fence> &fence,
			Status &) override;

	virtual Status present(core::DeviceQueue &queue, core::ImageStorage *,
			uint64_t presentWindow) override;
	virtual void invalidateImage(const core::ImageStorage *image, bool release) override;
	virtual void invalidateImage(uint32_t, bool release) override;

	virtual Rc<core::ImageView> makeView(const Rc<core::ImageObject> &,
			const ImageViewInfo &) override;

	virtual Rc<core::Semaphore> acquireSemaphore() override;
	virtual bool releaseSemaphore(Rc<core::Semaphore> &&) override;

protected:
	using core::Object::init;

	SwapchainHandleData *_data = nullptr;
	Set<uint32_t> _acquiredIndexes;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKSWAPCHAIN_H_ */
