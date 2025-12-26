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

#ifndef XENOLITH_BACKEND_VK_XLVKPRESENTATIONENGINE_H_
#define XENOLITH_BACKEND_VK_XLVKPRESENTATIONENGINE_H_

#include "XLCorePresentationEngine.h"
#include "XLVkSwapchain.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SP_PUBLIC PresentationEngine final : public core::PresentationEngine {
public:
	virtual ~PresentationEngine() = default;

	virtual bool run() override;

	virtual Rc<core::ScreenInfo> getScreenInfo() const override;
	virtual Status setFullscreenSurface(const core::MonitorId &, const core::ModeInfo &) override;

	virtual bool recreateSwapchain() override;
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg,
			core::PresentMode presentMode, bool oldSwapchainValid) override;

protected:
	bool isImagePresentable(const core::ImageObject &image, VkFilter &filter) const;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKPRESENTATIONENGINE_H_ */
