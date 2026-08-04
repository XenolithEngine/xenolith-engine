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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTHEADLESSPRESENTATION_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTHEADLESSPRESENTATION_H_

#include "XLSoftPresentation.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// A surface with no window system behind it. Capabilities are synthesized from the window
// extent, exactly as vk::HeadlessSurface does - there is nothing to query.
class SP_PUBLIC HeadlessSurface final : public core::Surface {
public:
	virtual ~HeadlessSurface() = default;

	bool init(Instance *, Extent2 extent, Ref *window = nullptr);

	virtual void invalidate() override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			core::FullScreenExclusiveMode, void *) const override;

	void setExtent(Extent2 extent) { _extent = extent; }

protected:
	Extent2 _extent;
};

// Pseudo-swapchain: a ring of ordinary bitmaps that stand in for swapchain images.
//
// Acquisition is synchronous and hands out no semaphore - nothing produced the image
// asynchronously, so there is nothing to wait on - and present is bookkeeping only. The image
// that was presented last is kept addressable, which is what lets a screenshot read "the current
// screen" without rendering another frame.
class SP_PUBLIC HeadlessSwapchain final : public SwapchainBase {
public:
	virtual ~HeadlessSwapchain();

	bool init(Device &, NotNull<core::Loop>, const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode, HeadlessSurface *);

	virtual Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<core::Fence> &fence,
			Status &) override;

	virtual Status present(core::DeviceQueue *, core::ImageStorage *,
			const core::PresentInfo &) override;

protected:
	using SwapchainBase::init;

	uint32_t _nextIndex = 0;
};

// Presentation engine for a headless window. Inherits run()/recreateSwapchain() unchanged - they
// only talk to PresentationWindow and to makeSwapchain - and replaces the swapchain construction
// with a ring of ordinary bitmaps.
class SP_PUBLIC HeadlessPresentationEngine final : public PresentationEngine {
public:
	virtual ~HeadlessPresentationEngine() = default;

	virtual bool init(NotNull<core::Loop>, NotNull<core::Device>, NotNull<core::PresentationWindow>,
			core::PresentationOptions) override;

protected:
	virtual Rc<SwapchainBase> makeSwapchain(const core::SurfaceInfo &,
			const core::SwapchainConfig &, core::ImageInfo &&, core::PresentMode) override;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTHEADLESSPRESENTATION_H_ */
