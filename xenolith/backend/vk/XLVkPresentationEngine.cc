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

#include "XLVkPresentationEngine.h"
#include "SPStatus.h"
#include "XLCorePresentationEngine.h"
#include "XLCorePresentationFrame.h"
#include "XLVkInfo.h"
#include "XLVkSwapchain.h"
#include "XlCoreMonitorInfo.h"
#include <vulkan/vulkan_core.h>

#define XL_VKPRESENT_DEBUG 0

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

#if XL_VKPRESENT_DEBUG
#define XL_VKPRESENT_LOG(...) log::source().debug("vk::PresentationEngine", __VA_ARGS__)
#else
#define XL_VKPRESENT_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool PresentationEngine::run() {
	auto dev = static_cast<Device *>(_device);
	if (dev->hasExtension(OptionalDeviceExtension::DisplayTiming)) {
		_presentWithWindowTiming = true;
	}

	auto info = _window->getSurfaceOptions(*static_cast<Device *>(_device), _surface);
	auto cfg = _window->selectConfig(info, false);

	createSwapchain(info, move(cfg), cfg.presentMode, true);

	return core::PresentationEngine::run();
}

Rc<core::ScreenInfo> PresentationEngine::getScreenInfo() const {
	Rc<core::ScreenInfo> ret = Rc<core::ScreenInfo>::create();

	ret->primaryMonitor = maxOf<uint32_t>();

	auto &info = static_cast<Device *>(_device)->getInfo();

	for (auto &it : info.displays) { ret->monitors.emplace_back(it); }

	return ret;
}

Status PresentationEngine::setFullscreenSurface(const core::MonitorId &monId,
		const core::ModeInfo &mode) {
	if (monId == core::MonitorId::None) {
		if (_surface != _originalSurface) {
			_nextSurface = _originalSurface;
			updateConstraints(core::UpdateConstraintsFlags::SwitchToNext);
			return Status::Ok;
		}
		return Status::ErrorInvalidArguemnt;
	}

	if (monId == core::MonitorId::Primary) {
		return Status::ErrorInvalidArguemnt;
	}

	auto &devInfo = static_cast<Device *>(_device)->getInfo();

	const DisplayInfo *display = nullptr;
	for (auto &it : devInfo.displays) {
		if (it == monId) {
			display = &it;
		}
	}

	if (!display) {
		return Status::ErrorInvalidArguemnt;
	}

	const ModeInfo *targetMode = nullptr;
	for (auto &it : display->modes) {
		if (it.info == mode) {
			targetMode = &it;
		}
	}

	if (!targetMode) {
		return Status::ErrorInvalidArguemnt;
	}

	VkDisplaySurfaceCreateInfoKHR info;
	info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
	info.pNext = nullptr;
	info.flags = 0;
	info.displayMode = targetMode->mode;
	info.planeIndex = targetMode->planes.front().index;
	info.planeStackIndex = targetMode->planes.front().index;
	info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	info.globalAlpha = 0.0f;
	info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
	info.imageExtent = VkExtent2D{targetMode->info.width, targetMode->info.height};

	VkSurfaceKHR surface;

	auto instance = static_cast<Instance *>(_loop->getInstance());
	auto result = instance->vkCreateDisplayPlaneSurfaceKHR(instance->getInstance(), &info, nullptr,
			&surface);

	if (result == VK_SUCCESS) {
		_nextSurface = Rc<Surface>::create(instance, surface);

		updateConstraints(core::UpdateConstraintsFlags::SwitchToNext);
	}
	return Status::Ok;
}

bool PresentationEngine::recreateSwapchain() {
	XL_VKPRESENT_LOG("recreateSwapchain");
	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::Finalized)) {
		return false;
	}

	_device->waitIdle();
	_waitForDisplayLink = false;

	bool oldSwapchainValid = true;
	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::SwitchToNext)) {
		if (_nextSurface) {
			_surface = move(_nextSurface);
			oldSwapchainValid = false;
		}
	}

	resetFrames();

	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::EndOfLife)) {
		_deprecationFlags |= core::UpdateConstraintsFlags::Finalized;

		auto callbacks = sp::move(_deprecationCallbacks);
		_deprecationCallbacks.clear();

		for (auto &it : callbacks) { it(false); }

		end();

		return false;
	}

	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::EnableLiveResize)) {
		_liveResizeEnabled = true;
	}

	if (hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::DisableLiveResize)) {
		_liveResizeEnabled = false;
	}

	auto fastModeSelected = _liveResizeEnabled
			|| hasFlag(_deprecationFlags, core::UpdateConstraintsFlags::SwitchToFastMode);
	auto info = _window->getSurfaceOptions(*static_cast<Device *>(_device), _surface);
	auto cfg = _window->selectConfig(info, fastModeSelected);

	if (!info.isSupported(cfg)) {
		log::source().error("Vk-Error", "Presentation with config ", cfg.description(),
				" is not supported for ", info.description());
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		return false;
	}

	bool ret = false;

	auto mode = cfg.presentMode;
	if (fastModeSelected) {
		mode = cfg.presentModeFast;
	}

	if (_liveResizeEnabled) {
		cfg.liveResize = true;
	}

	ret = createSwapchain(info, move(cfg), mode, oldSwapchainValid);

	_deprecationFlags = core::UpdateConstraintsFlags::None;

	auto callbacks = sp::move(_deprecationCallbacks);
	_deprecationCallbacks.clear();

	for (auto &it : callbacks) { it(true); }

	if (ret) {
		_nextPresentWindow = 0;
		_readyForNextFrame = true;
		XL_VKPRESENT_LOG("recreateSwapchain - scheduleNextImage");
		// run frame, no present window, no wait on fences
		scheduleNextImage();
	}
	return ret;
}

bool PresentationEngine::createSwapchain(const core::SurfaceInfo &info, core::SwapchainConfig &&cfg,
		core::PresentMode presentMode, bool oldSwapchainValid) {
	auto dev = static_cast<Device *>(_device);
	auto &devInfo = dev->getInfo();

	auto swapchainImageInfo = _window->getSwapchainImageInfo(cfg);
	uint32_t queueFamilyIndices[] = {devInfo.graphicsFamily.index, devInfo.presentFamily.index};

	if (!_exclusiveFullscreenAvailable
			&& cfg.fullscreenMode == core::FullScreenExclusiveMode::ApplicationControlled) {
		cfg.fullscreenMode = core::FullScreenExclusiveMode::Allowed;
	}

	do {
		auto oldSwapchain = move(_swapchain);

		if (oldSwapchain && oldSwapchain->getPresentedFramesCount() == 0) {
			log::source().warn("vk::View", "Swapchain replaced without frame presentation");
		}

		// log::source().verbose("vk::PresentationEngine", "Surface: ", info.description());
		_swapchain = Rc<SwapchainHandle>::create(*dev, info, cfg, move(swapchainImageInfo),
				presentMode, _surface.get_cast<Surface>(), queueFamilyIndices,
				(oldSwapchain && oldSwapchainValid) ? oldSwapchain.get_cast<SwapchainHandle>()
													: nullptr);

		if (_swapchain) {
			if (cfg.fullscreenMode == core::FullScreenExclusiveMode::ApplicationControlled) {
				if (!_swapchain.get_cast<SwapchainHandle>()->enableExclusiveFullscreen(*dev)) {
					cfg.fullscreenMode = core::FullScreenExclusiveMode::Allowed;
					_swapchain = nullptr;
					continue;
				}
			}

			auto newConstraints = _window->exportConstraints();
			newConstraints.extent = cfg.extent;
			newConstraints.transform = cfg.transform;

			_constraints = sp::move(newConstraints);

			Vector<uint64_t> ids;
			auto cache = _loop->getFrameCache();
			for (auto &it : static_cast<SwapchainHandle *>(_swapchain.get())->getImages()) {
				for (auto &iit : it.views) {
					auto id = iit.second->getIndex();
					ids.emplace_back(iit.second->getIndex());
					iit.second->setReleaseCallback(
							[loop = _loop, cache, id] { cache->removeImageView(id); });
				}
			}

			for (auto &id : ids) { cache->addImageView(id); }

			// log::source().verbose("vk::PresentationEngine", "Swapchain: ", cfg.description());
		} else {
			log::source().error("vk::PresentationEngine", "Fail to create swapchain");
			break;
		}
	} while (!_swapchain);

	if (_swapchain) {
		_waitForDisplayLink = false;
		_readyForNextFrame = true;
		return true;
	}
	return false;
}

bool PresentationEngine::isImagePresentable(const core::ImageObject &image,
		VkFilter &filter) const {
	auto dev = static_cast<Device *>(_device);

	auto &config = _swapchain->getConfig();

	auto &sourceImageInfo = image.getInfo();
	if (sourceImageInfo.extent.depth != 1 || sourceImageInfo.format != config.imageFormat
			|| (sourceImageInfo.usage & core::ImageUsage::TransferSrc) == core::ImageUsage::None) {
		log::source().error("Swapchain", "Image can not be presented on swapchain");
		return false;
	}

	VkFormatProperties sourceProps;
	VkFormatProperties targetProps;

	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(sourceImageInfo.format), &sourceProps);
	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(config.imageFormat), &targetProps);

	if (config.extent.width == sourceImageInfo.extent.width
			&& config.extent.height == sourceImageInfo.extent.height) {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == core::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		}
	} else {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == core::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.optimalTilingFeatures
						& VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
					!= 0) {
				filter = VK_FILTER_LINEAR;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.linearTilingFeatures
						& VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
					!= 0) {
				filter = VK_FILTER_LINEAR;
			}
		}
	}

	return true;
}

} // namespace stappler::xenolith::vk
