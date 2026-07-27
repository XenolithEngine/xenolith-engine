/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLVkInstance.h"
#include "SPMemory.h"
#include "XLVk.h"
#include "XLVkInfo.h"
#include "XLVkLoop.h"
#include "XLVkDevice.h"
#include <vulkan/vulkan_core.h>

// Direct-to-display: enumerate KMS connectors via raw DRM UAPI ioctls (no libdrm
// dependency, ioctl() is libc). The kernel <drm/drm_mode.h> lives in the sysroot's
// include_libc/drm which is not reliably on the angle-bracket search path for this
// SDK, so the two structs + ioctl numbers we need are declared inline below — they
// are stable kernel UAPI (drm_mode.h: drm_mode_card_res / drm_mode_get_connector,
// drm.h: DRM_IOCTL_MODE_GETRESOURCES = DRM_IOWR(0xA0), GETCONNECTOR = 0xA7).
#if defined(__linux__) && !defined(__ANDROID__)
#define XL_VK_DRM_DISPLAY 1
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

// ioctl() is declared directly: this SDK's <sys/ioctl.h> wrapper is broken (it
// references an undeclared __cmd) and we only need this one call.
extern "C" int ioctl(int, unsigned long, ...);

namespace {
struct xl_drm_mode_card_res {
	uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
	uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
	uint32_t min_width, max_width, min_height, max_height;
};
struct xl_drm_mode_modeinfo {
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
	uint32_t vrefresh;
	uint32_t flags;
	uint32_t type;
	char name[32];
};
struct xl_drm_mode_get_connector {
	uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
	uint32_t count_modes, count_props, count_encoders;
	uint32_t encoder_id, connector_id, connector_type, connector_type_id;
	uint32_t connection, mm_width, mm_height, subpixel, pad;
};
struct xl_drm_mode_get_encoder {
	uint32_t encoder_id, encoder_type, crtc_id, possible_crtcs, possible_clones;
};
struct xl_drm_mode_crtc {
	uint64_t set_connectors_ptr;
	uint32_t count_connectors;
	uint32_t crtc_id, fb_id;
	uint32_t x, y;
	uint32_t gamma_size;
	uint32_t mode_valid;
	xl_drm_mode_modeinfo mode;
};
static_assert(sizeof(xl_drm_mode_card_res) == 64, "drm_mode_card_res ABI");
static_assert(sizeof(xl_drm_mode_modeinfo) == 68, "drm_mode_modeinfo ABI");
static_assert(sizeof(xl_drm_mode_get_connector) == 80, "drm_mode_get_connector ABI");
static_assert(sizeof(xl_drm_mode_get_encoder) == 20, "drm_mode_get_encoder ABI");
static_assert(sizeof(xl_drm_mode_crtc) == 104, "drm_mode_crtc ABI");
} // namespace

// DRM ioctl numbers, precomputed (SDK's _IOWR macro is unavailable, see above):
//   _IOWR('d', nr, T) = (3u<<30) | (sizeof(T)<<16) | ('d'<<8) | nr
//   GETRESOURCES = _IOWR('d',0xA0, card_res[64B])     = 0xC04064A0
//   GETCRTC      = _IOWR('d',0xA1, crtc[104B])        = 0xC06864A1
//   GETENCODER   = _IOWR('d',0xA6, encoder[20B])      = 0xC01464A6
//   GETCONNECTOR = _IOWR('d',0xA7, get_connector[80B]) = 0xC05064A7
#define XL_DRM_IOCTL_MODE_GETRESOURCES 0xC04064A0ul
#define XL_DRM_IOCTL_MODE_GETCRTC 0xC06864A1ul
#define XL_DRM_IOCTL_MODE_GETENCODER 0xC01464A6ul
#define XL_DRM_IOCTL_MODE_GETCONNECTOR 0xC05064A7ul
#define XL_DRM_MODE_TYPE_PREFERRED (1u << 3)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

SPUNUSED static VkResult s_createDebugUtilsMessengerEXT(VkInstance instance,
		const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)getInstanceProcAddr(instance,
			"vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

SPUNUSED static VKAPI_ATTR VkBool32 VKAPI_CALL s_debugMessageCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
	if (pCallbackData->pMessageIdName
			&& sprt::strcmp(pCallbackData->pMessageIdName,
					   "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274")
					== 0) {
		// this is normal for multithreaded engine
		messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	}
	if (pCallbackData->pMessageIdName
			&& sprt::strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0) {
		if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			if (StringView(pCallbackData->pMessage).starts_with("Instance Extension: ")
					|| StringView(pCallbackData->pMessage).starts_with("Device Extension: ")) {
				return VK_FALSE;
			}
			log::source().verbose("Vk-Validation-Verbose", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			log::source().info("Vk-Validation-Info", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			log::source().warn("Vk-Validation-Warning", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			log::source().error("Vk-Validation-Error", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		}
		return VK_FALSE;
	} else {
		if (messageSeverity < XL_VK_MIN_MESSAGE_SEVERITY) {
			return VK_FALSE;
		}

		if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			if (StringView(pCallbackData->pMessage).starts_with("Device Extension: ")) {
				return VK_FALSE;
			}
			log::source().verbose("Vk-Validation-Verbose", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			log::source().info("Vk-Validation-Info", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			log::source().warn("Vk-Validation-Warning", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			log::source().error("Vk-Validation-Error", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		}
		return VK_FALSE;
	}
}

void InstanceData::enableLayer(const char *str) {
	auto it = sprt::find_if(layersToEnable.begin(), layersToEnable.end(),
			[&](const char *ptr) { return sprt::strcmp(str, ptr) == 0; });
	if (it == layersToEnable.end()) {
		layersToEnable.emplace_back(str);
	}
}

void InstanceData::enableExtension(const char *str) {
	auto it = sprt::find_if(extensionsToEnable.begin(), extensionsToEnable.end(),
			[&](const char *ptr) { return sprt::strcmp(str, ptr) == 0; });
	if (it == extensionsToEnable.end()) {
		extensionsToEnable.emplace_back(str);
	}
}

Instance::Instance(VkInstance inst, const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		uint32_t targetVersion, OptVec &&optionals, sprt::Dso &&vulkanModule,
		PresentSupportCallback &&present, SurfaceBackendMask &&mask, core::InstanceFlags flags)
: core::Instance(core::InstanceApi::Vulkan, flags, sp::move(vulkanModule))
, InstanceTable(getInstanceProcAddr, inst)
, _instance(inst)
, _version(targetVersion)
, _optionals(sp::move(optionals))
, _checkPresentSupport(sp::move(present))
, _surfaceBackendMask(sp::move(mask)) {
	if (hasFlag(flags, core::InstanceFlags::Validation)) {
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.pNext = nullptr;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		debugCreateInfo.pfnUserCallback = s_debugMessageCallback;
		debugCreateInfo.pUserData = this;

		if (s_createDebugUtilsMessengerEXT(_instance, vkGetInstanceProcAddr, &debugCreateInfo,
					nullptr, &debugMessenger)
				!= VK_SUCCESS) {
			log::source().warn("Vk", "failed to set up debug messenger!");
		}
	}

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

	if (deviceCount) {
		Vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		for (auto &device : devices) {
			_devices.emplace_back(DeviceInfoWrapper(device)); //
		}
	} else {
		log::source().info("Vk", "No devices available on this instance");
	}
}

Instance::~Instance() {
	if (debugMessenger != VK_NULL_HANDLE) {
		vkDestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
	}
	vkDestroyInstance(_instance, nullptr);
}

size_t Instance::getDeviceCount() const { return _devices.size(); }

bool Instance::readDeviceProperties(size_t n, sprt::window::gapi::DeviceProperties &prop) {
	if (n >= _devices.size()) {
		return false;
	}

	auto &it = _devices.at(n);

	it.once([&] { getDeviceInfo(it.info, it.info.device); });

	prop.deviceName = it.info.properties.device10.properties.deviceName;
	prop.apiVersion = it.info.properties.device10.properties.apiVersion;
	prop.driverVersion = it.info.properties.device10.properties.driverVersion;
	prop.presentationSupported = it.info.supportsPresentation();

	return true;
}

Rc<core::Loop> Instance::makeLoop(NotNull<sprt::dispatch::Looper> looper,
		Rc<core::LoopInfo> &&info) const {
	return Rc<vk::Loop>::create(looper, const_cast<Instance *>(this), move(info));
}

Rc<Device> Instance::makeDevice(const core::LoopInfo &info) const {
	auto data = info.backend.get_cast<LoopBackendInfo>();
	if (!data) {
		log::source().error("vk::Instance",
				"Fail to create device: loop platform data is not defined");
		return nullptr;
	}

	auto isDeviceSupported = [&](const DeviceInfo &dev) {
		if (data->deviceSupportCallback) {
			if (!data->deviceSupportCallback(dev)) {
				return false;
			}
		} else {
			if (!dev.supportsPresentation()) {
				return false;
			}
		}
		return true;
	};

	auto getDeviceExtensions = [&](const DeviceInfo &dev) {
		Vector<StringView> requiredExtensions;
		if (data->deviceExtensionsCallback) {
			requiredExtensions = data->deviceExtensionsCallback(dev);
		}

		for (auto &ext : s_requiredDeviceExtensions) {
			if (ext
					&& !isPromotedExtension(dev.properties.device10.properties.apiVersion,
							StringView(ext))) {
				requiredExtensions.emplace_back(ext);
			}
		}

		for (auto &ext : dev.optionalExtensions) { requiredExtensions.emplace_back(ext); }

		for (auto &ext : dev.promotedExtensions) {
			if (!isPromotedExtension(dev.properties.device10.properties.apiVersion, ext)) {
				requiredExtensions.emplace_back(ext);
			}
		}

		return requiredExtensions;
	};

	auto isExtensionsSupported = [&](const DeviceInfo &dev,
										 const Vector<StringView> &requiredExtensions) {
		if (!requiredExtensions.empty()) {
			bool found = true;
			for (auto &req : requiredExtensions) {
				auto iit = sprt::find(dev.availableExtensions.begin(),
						dev.availableExtensions.end(), req);
				if (iit == dev.availableExtensions.end()) {
					found = false;
					break;
				}
			}
			if (!found) {
				return false;
			}
		}
		return true;
	};

	auto buildFeaturesList = [&](const DeviceInfo &dev, DeviceInfo::Features &features) {
		if (data->deviceFeaturesCallback) {
			features = data->deviceFeaturesCallback(dev);
		}

		features.enableFromFeatures(DeviceInfo::Features::getRequired());

		if (!dev.features.canEnable(features, dev.properties.device10.properties.apiVersion)) {
			return false;
		}

		features.enableFromFeatures(DeviceInfo::Features::getOptional());
		features.disableFromFeatures(dev.features);
		features.optionals = dev.features.optionals;
		return true;
	};

	if (info.deviceIdx == core::InstanceDefaultDevice) {
		for (auto &it : _devices) {
			it.once([&] { getDeviceInfo(it.info, it.info.device); });

			if (!isDeviceSupported(it.info)) {
				log::source().warn("vk::Instance", "Device rejected: device is not supported");
				continue;
			}

			auto requiredExtensions = getDeviceExtensions(it.info);
			if (!isExtensionsSupported(it.info, requiredExtensions)) {
				log::source().warn("vk::Instance",
						"Device rejected: required extensions is not available");
				continue;
			}

			DeviceInfo::Features targetFeatures;
			if (!buildFeaturesList(it.info, targetFeatures)) {
				log::source().warn("vk::Instance",
						"Device rejected: required features is not available");
				continue;
			}

			if (it.info.features.canEnable(targetFeatures,
						it.info.properties.device10.properties.apiVersion)) {
				return Rc<vk::Device>::create(this, DeviceInfo(it.info), targetFeatures,
						requiredExtensions);
			}
		}
	} else if (info.deviceIdx < _devices.size()) {
		auto &dev = _devices[info.deviceIdx];

		dev.once([&] { getDeviceInfo(dev.info, dev.info.device); });

		if (!isDeviceSupported(dev.info)) {
			log::source().error("vk::Instance", "Fail to create device: device is not supported");
			return nullptr;
		}

		auto requiredExtensions = getDeviceExtensions(dev.info);
		if (!isExtensionsSupported(dev.info, requiredExtensions)) {
			log::source().error("vk::Instance",
					"Fail to create device: required extensions is not available");
			return nullptr;
		}

		DeviceInfo::Features targetFeatures;
		if (!buildFeaturesList(dev.info, targetFeatures)) {
			log::source().error("vk::Instance",
					"Fail to create device: required features is not available");
			return nullptr;
		}

		if (dev.info.features.canEnable(targetFeatures,
					dev.info.properties.device10.properties.apiVersion)) {
			return Rc<vk::Device>::create(this, DeviceInfo(dev.info), targetFeatures,
					requiredExtensions);
		}
	}

	log::source().error("vk::Instance", "Fail to create device: no acceptable devices found");
	return nullptr;
}

static core::PresentMode getGlPresentMode(VkPresentModeKHR presentMode) {
	switch (presentMode) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return core::PresentMode::Immediate; break;
	case VK_PRESENT_MODE_MAILBOX_KHR: return core::PresentMode::Mailbox; break;
	case VK_PRESENT_MODE_FIFO_KHR: return core::PresentMode::Fifo; break;
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return core::PresentMode::FifoRelaxed; break;
	default: return core::PresentMode::Unsupported; break;
	}
}

core::SurfaceInfo Instance::getSurfaceOptions(VkSurfaceKHR surface, VkPhysicalDevice device,
		core::FullScreenExclusiveMode fullscreenMode, void *fullscreenHandle) const {
	VkSurfaceCapabilities2KHR caps{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		nullptr,
	};

	VkPhysicalDeviceSurfaceInfo2KHR info{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		nullptr,
		surface,
	};

#if WIN32
	VkSurfaceCapabilitiesFullScreenExclusiveEXT fullScreenCapabilities{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT,
		nullptr,
		0,
	};

	if (fullscreenMode != core::FullScreenExclusiveMode::Default) {
		fullScreenCapabilities.pNext = caps.pNext;
		caps.pNext = &fullScreenCapabilities;
	}

	VkSurfaceFullScreenExclusiveWin32InfoEXT fullScreenWin32{
		VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,
		nullptr,
		(HMONITOR)fullscreenHandle,
	};

	VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo{
		VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,
		&fullScreenWin32,
		VkFullScreenExclusiveEXT(fullscreenMode),
	};

	if (fullscreenMode != core::FullScreenExclusiveMode::Default) {
		fullScreenWin32.pNext = (void *)info.pNext;
		info.pNext = &fullScreenInfo;
	}
#endif

#if !defined(VK_EXT_full_screen_exclusive)
	void (*vkGetPhysicalDeviceSurfacePresentModes2EXT)(VkPhysicalDevice,
			VkPhysicalDeviceSurfaceInfo2KHR *, uint32_t *, VkPresentModeKHR *) = nullptr;
#endif

	const bool usePresentModes2 = fullscreenMode != core::FullScreenExclusiveMode::Default
			&& vkGetPhysicalDeviceSurfacePresentModes2EXT;

	core::SurfaceInfo ret;

	uint32_t presentModeCount = 0;
	if (usePresentModes2) {
		vkGetPhysicalDeviceSurfacePresentModes2EXT(device, &info, &presentModeCount, nullptr);
	} else {
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	}

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		Vector<VkSurfaceFormatKHR> formats;
		formats.resize(formatCount);

		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());

		ret.formats.reserve(formatCount);
		for (auto &it : formats) {
			ret.formats.emplace_back(core::ImageFormat(it.format), core::ColorSpace(it.colorSpace));
		}
	}

	if (presentModeCount != 0) {
		ret.presentModes.reserve(presentModeCount);
		Vector<VkPresentModeKHR> modes;
		modes.resize(presentModeCount);

		if (usePresentModes2) {
			vkGetPhysicalDeviceSurfacePresentModes2EXT(device, &info, &presentModeCount,
					modes.data());
		} else {
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
					modes.data());
		}

		for (auto &it : modes) { ret.presentModes.emplace_back(getGlPresentMode(it)); }

		sprt::sort(ret.presentModes.begin(), ret.presentModes.end(),
				[&](core::PresentMode l, core::PresentMode r) { return toInt(l) > toInt(r); });
	}

	// index into s_optionalExtension
	if (_optionals.test(toInt(OptionalInstanceExtension::SurfaceCapabilities2))) {
		vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &info, &caps);
	} else {
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps.surfaceCapabilities);
	}

	ret.minImageCount = caps.surfaceCapabilities.minImageCount;
	ret.maxImageCount = caps.surfaceCapabilities.maxImageCount;
	ret.currentExtent = Extent2(caps.surfaceCapabilities.currentExtent.width,
			caps.surfaceCapabilities.currentExtent.height);
	ret.minImageExtent = Extent2(caps.surfaceCapabilities.minImageExtent.width,
			caps.surfaceCapabilities.minImageExtent.height);
	ret.maxImageExtent = Extent2(caps.surfaceCapabilities.maxImageExtent.width,
			caps.surfaceCapabilities.maxImageExtent.height);
	ret.maxImageArrayLayers = caps.surfaceCapabilities.maxImageArrayLayers;
	ret.supportedTransforms =
			core::SurfaceTransformFlags(caps.surfaceCapabilities.supportedTransforms);
	ret.currentTransform = core::SurfaceTransformFlags(caps.surfaceCapabilities.currentTransform);
	ret.supportedCompositeAlpha =
			core::CompositeAlphaFlags(caps.surfaceCapabilities.supportedCompositeAlpha);
	ret.supportedUsageFlags = core::ImageUsage(caps.surfaceCapabilities.supportedUsageFlags);

#if WIN32
	if (fullScreenCapabilities.fullScreenExclusiveSupported) {
		ret.fullscreenMode = fullscreenMode;
		ret.fullscreenHandle = fullscreenHandle;
	}
#endif

	return ret;
}

VkExtent2D Instance::getSurfaceExtent(VkSurfaceKHR surface, VkPhysicalDevice device) const {
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
	return capabilities.currentExtent;
}

VkInstance Instance::getInstance() const { return _instance; }

void Instance::printDevicesInfo(const CallbackStream &out, bool initOnly) const {
	out << "\n";

	auto getDeviceTypeString = [&](VkPhysicalDeviceType type) -> const char * {
		switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU"; break;
		default: return "Other"; break;
		}
		return "Other";
	};

	for (auto &device : _devices) {
		if (initOnly && !device.once.is_set()) {
			continue;
		}

		device.once([&] { getDeviceInfo(device.info, device.info.device); });

		out << "\tDevice: " << device.info.device << " "
			<< getDeviceTypeString(device.info.properties.device10.properties.deviceType) << ": "
			<< device.info.properties.device10.properties.deviceName << " (API: "
			<< getVersionDescription(device.info.properties.device10.properties.apiVersion)
			<< ", Driver: "
			<< getVersionDescription(device.info.properties.device10.properties.driverVersion)
			<< ")\n";

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device.info.device, &queueFamilyCount, nullptr);

		Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device.info.device, &queueFamilyCount,
				queueFamilies.data());

		int i = 0;
		for (const VkQueueFamilyProperties &queueFamily : queueFamilies) {
			bool empty = true;
			out << "\t\t[" << i << "] Queue family; Flags: ";
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Graphics";
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Compute";
			}
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Transfer";
			}
			if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "SparseBinding";
			}
			if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Protected";
			}

			VkBool32 presentSupport = checkPresentationSupport(device.info.device, i).any();
			if (presentSupport) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Present";
			}

			out << "; Count: " << queueFamily.queueCount << "\n";
			i++;
		}
		out << device.info.description();
	}
}

void Instance::getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &features,
		const DeviceInfo::OptVec &flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if (flags.test(toInt(OptionalDeviceExtension::Portability))) {
		features.devicePortability.pNext = next;
		next = &features.devicePortability;
	}
#endif
	features.optionals = flags;
#if VK_VERSION_1_3
	if (api >= VK_API_VERSION_1_3) {
		features.device13.pNext = next;
		features.device12.pNext = &features.device13;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateFrom13();
	} else
#endif
			if (api >= VK_API_VERSION_1_2) {
		features.device12.pNext = next;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateFrom12();
	} else {
		if (flags.test(toInt(OptionalDeviceExtension::Storage16Bit))) {
			features.device16bitStorage.pNext = next;
			next = &features.device16bitStorage;
		}
		if (flags.test(toInt(OptionalDeviceExtension::Storage8Bit))) {
			features.device8bitStorage.pNext = next;
			next = &features.device8bitStorage;
		}
		if (flags.test(toInt(OptionalDeviceExtension::ShaderFloat16Int8))) {
			features.deviceShaderFloat16Int8.pNext = next;
			next = &features.deviceShaderFloat16Int8;
		}
		if (flags.test(toInt(OptionalDeviceExtension::DescriptorIndexing))) {
			features.deviceDescriptorIndexing.pNext = next;
			next = &features.deviceDescriptorIndexing;
		}
		if (flags.test(toInt(OptionalDeviceExtension::DeviceAddress))) {
			features.deviceBufferDeviceAddress.pNext = next;
			next = &features.deviceBufferDeviceAddress;
		}
		if (flags.test(toInt(OptionalDeviceExtension::SwapchainMaintenance1))) {
			features.deviceSwapchainMaintenance1.pNext = next;
			next = &features.deviceSwapchainMaintenance1;
		}
		features.device10.pNext = next;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateTo12(true);
	}

	VkPhysicalDeviceExternalFenceInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

	vkGetPhysicalDeviceExternalFenceProperties(device, &fenceInfo, &features.fenceSyncFd);
}

void Instance::getDeviceProperties(const VkPhysicalDevice &device,
		DeviceInfo::Properties &properties, const DeviceInfo::OptVec &flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if (flags.test(toInt(OptionalDeviceExtension::Portability))) {
		properties.devicePortability.pNext = next;
		next = &properties.devicePortability;
	}
#endif
	if (flags.test(toInt(OptionalDeviceExtension::Maintenance3))) {
		properties.deviceMaintenance3.pNext = next;
		next = &properties.deviceMaintenance3;
	}
	if (flags.test(toInt(OptionalDeviceExtension::DescriptorIndexing))) {
		properties.deviceDescriptorIndexing.pNext = next;
		next = &properties.deviceDescriptorIndexing;
	}

	properties.device10.pNext = next;

	if (vkGetPhysicalDeviceProperties2) {
		vkGetPhysicalDeviceProperties2(device, &properties.device10);
	} else if (vkGetPhysicalDeviceProperties2KHR) {
		vkGetPhysicalDeviceProperties2KHR(device, &properties.device10);
	} else {
		vkGetPhysicalDeviceProperties(device, &properties.device10.properties);
	}
}

void Instance::getDeviceInfo(DeviceInfo &ret, VkPhysicalDevice device) const {
	uint32_t graphicsFamily = maxOf<uint32_t>();
	uint32_t presentFamily = maxOf<uint32_t>();
	uint32_t transferFamily = maxOf<uint32_t>();
	uint32_t computeFamily = maxOf<uint32_t>();

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	Vector<DeviceInfo::QueueFamilyInfo> queueInfo(queueFamilyCount);
	Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const VkQueueFamilyProperties &queueFamily : queueFamilies) {
		auto presentSupport = checkPresentationSupport(device, i);

		queueInfo[i].index = i;
		queueInfo[i].flags = getQueueFlags(queueFamily.queueFlags, presentSupport.any());
		queueInfo[i].count = queueFamily.queueCount;
		queueInfo[i].used = 0;
		queueInfo[i].timestampValidBits = queueFamily.timestampValidBits;
		queueInfo[i].minImageTransferGranularity =
				Extent3(queueFamily.minImageTransferGranularity.width,
						queueFamily.minImageTransferGranularity.height,
						queueFamily.minImageTransferGranularity.depth);
		queueInfo[i].presentSurfaceMask = presentSupport;

		if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				&& graphicsFamily == maxOf<uint32_t>()) {
			graphicsFamily = i;
		}

		if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
				&& transferFamily == maxOf<uint32_t>()) {
			transferFamily = i;
		}

		if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && computeFamily == maxOf<uint32_t>()) {
			computeFamily = i;
		}

		if (presentSupport != 0 && presentFamily == maxOf<uint32_t>()) {
			presentFamily = i;
		}

		i++;
	}

	// try to select different families for transfer and compute (for more concurrency)
	if (computeFamily == graphicsFamily) {
		for (auto &it : queueInfo) {
			if (it.index != graphicsFamily
					&& ((it.flags & core::QueueFlags::Compute) != core::QueueFlags::None)) {
				computeFamily = it.index;
			}
		}
	}

	if (transferFamily == computeFamily || transferFamily == graphicsFamily) {
		for (auto &it : queueInfo) {
			if (it.index != graphicsFamily && it.index != computeFamily
					&& ((it.flags & core::QueueFlags::Transfer) != core::QueueFlags::None)) {
				transferFamily = it.index;
				break;
			}
		}
		if (transferFamily == computeFamily || transferFamily == graphicsFamily) {
			if (graphicsFamily == maxOf<uint32_t>()
					|| queueInfo[computeFamily].count >= queueInfo[graphicsFamily].count) {
				transferFamily = computeFamily;
			} else {
				transferFamily = graphicsFamily;
			}
		}
	}

	// try to map present with graphics
	if (graphicsFamily != maxOf<uint32_t>() && presentFamily != graphicsFamily) {
		if ((queueInfo[graphicsFamily].flags & core::QueueFlags::Present)
				!= core::QueueFlags::None) {
			presentFamily = graphicsFamily;
		}
	}

	// fallback when Transfer or Compute is not defined
	if (transferFamily == maxOf<uint32_t>()) {
		transferFamily = graphicsFamily;
		queueInfo[transferFamily].flags |= core::QueueFlags::Transfer;
	}

	if (computeFamily == maxOf<uint32_t>()) {
		computeFamily = graphicsFamily;
	}

	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	Vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
			availableExtensions.data());

	// we need only API version for now
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// find required device extensions
	bool notFound = false;
	for (auto &extensionName : s_requiredDeviceExtensions) {
		if (!extensionName) {
			break;
		}

		if (isPromotedExtension(deviceProperties.apiVersion, extensionName)) {
			continue;
		}

		bool found = false;
		for (auto &extension : availableExtensions) {
			if (sprt::strcmp(extensionName, extension.extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			if constexpr (s_printVkInfo) {
				log::source().verbose("Vk-Info", "Required device extension not found: %s",
						extensionName);
			}
			notFound = true;
			break;
		}
	}

	if (notFound) {
		ret.requiredExtensionsExists = false;
	} else {
		ret.requiredExtensionsExists = true;
	}

	// check for optionals
	DeviceInfo::OptVec extensionFlags;
	Vector<StringView> enabledOptionals;
	Vector<StringView> promotedOptionals;
	for (auto &extensionName : s_optionalDeviceExtensions) {
		if (!extensionName) {
			break;
		}

		checkIfExtensionAvailable(deviceProperties.apiVersion, extensionName, availableExtensions,
				enabledOptionals, promotedOptionals, extensionFlags);
	}

	ret.device = device;
	ret.graphicsFamily = queueInfo[graphicsFamily];
	ret.presentFamily = (presentFamily == maxOf<uint32_t>()) ? DeviceInfo::QueueFamilyInfo()
															 : queueInfo[presentFamily];
	ret.transferFamily = queueInfo[transferFamily];
	ret.computeFamily = queueInfo[computeFamily];
	ret.optionalExtensions = sp::move(enabledOptionals);
	ret.promotedExtensions = sp::move(promotedOptionals);

	ret.availableExtensions.reserve(availableExtensions.size());
	for (auto &it : availableExtensions) { ret.availableExtensions.emplace_back(it.extensionName); }

	getDeviceProperties(device, ret.properties, extensionFlags, deviceProperties.apiVersion);
	getDeviceFeatures(device, ret.features, extensionFlags, deviceProperties.apiVersion);

	auto requiredFeatures = DeviceInfo::Features::getRequired();
	ret.requiredFeaturesExists =
			ret.features.canEnable(requiredFeatures, deviceProperties.apiVersion);

	if (_optionals.test(toInt(OptionalInstanceExtension::Display))) {
		uint32_t nprops = 0;
		vkGetPhysicalDeviceDisplayPropertiesKHR(device, &nprops, nullptr);

		Vector<VkDisplayPropertiesKHR> props;
		props.resize(nprops);
		vkGetPhysicalDeviceDisplayPropertiesKHR(device, &nprops, props.data());

		for (auto &it : props) {
			ret.displays.emplace_back(DisplayInfo{
				it.display,
				it.displayName,
				Extent2(it.physicalDimensions.width, it.physicalDimensions.height),
				Extent2(it.physicalResolution.width, it.physicalResolution.height),
			});
		}

		uint32_t nplanes = 0;
		vkGetPhysicalDeviceDisplayPlanePropertiesKHR(device, &nplanes, nullptr);

		Vector<VkDisplayPlanePropertiesKHR> planesProperties;
		planesProperties.resize(nplanes);
		vkGetPhysicalDeviceDisplayPlanePropertiesKHR(device, &nplanes, planesProperties.data());

		for (auto &it : planesProperties) {
			auto &plane = ret.planes.emplace_back(PlaneInfo{it.currentStackIndex});
			for (auto &d : ret.displays) {
				if (d.display == it.currentDisplay) {
					plane.current = &d;
					break;
				}
			}
		}

		for (auto &it : ret.planes) {
			emplace_ordered(ret.knownPlanes, it.stackIndex);

			uint32_t ndisplays = 0;
			vkGetDisplayPlaneSupportedDisplaysKHR(device, it.stackIndex, &ndisplays, nullptr);

			Vector<VkDisplayKHR> displays;
			displays.resize(ndisplays);
			vkGetDisplayPlaneSupportedDisplaysKHR(device, it.stackIndex, &ndisplays,
					displays.data());

			for (auto &disp : displays) {
				for (auto &d : ret.displays) {
					if (d.display == disp) {
						d.planes.emplace_back(&it);
						it.displays.emplace_back(&d);
					}
				}
			}
		}

		for (auto &it : ret.displays) {
			uint32_t nmodes = 0;
			vkGetDisplayModePropertiesKHR(device, it.display, &nmodes, nullptr);

			Vector<VkDisplayModePropertiesKHR> modes;
			modes.resize(nmodes);

			vkGetDisplayModePropertiesKHR(device, it.display, &nmodes, modes.data());

			for (auto &iit : modes) {
				auto &mode = it.modes.emplace_back(ModeInfo{iit.displayMode,
					{
						iit.parameters.visibleRegion.width,
						iit.parameters.visibleRegion.height,
						iit.parameters.refreshRate,
					}});

				for (auto &p : ret.knownPlanes) {
					VkDisplayPlaneCapabilitiesKHR caps;
					if (vkGetDisplayPlaneCapabilitiesKHR(device, mode.mode, p, &caps)
							== VK_SUCCESS) {
						auto &plane = mode.planes.emplace_back();
						plane.index = p;
						plane.caps = sp::move(caps);
					}
				}
			}
		}
	}
}

#if XL_VK_DRM_DISPLAY
// Open the first available DRM primary node. Returns fd or -1.
static int s_openDrmDevice() {
	static const char *const paths[] = {"/dev/dri/card0", "/dev/dri/card1", nullptr};
	for (auto p = paths; *p; ++p) {
		int fd = ::open(*p, O_RDWR | O_CLOEXEC);
		if (fd >= 0) {
			return fd;
		}
	}
	return -1;
}

static const char *s_drmConnectionName(uint32_t c) {
	switch (c) {
	case 1: return "connected";
	case 2: return "disconnected";
	case 3: return "unknown";
	default: return "?";
	}
}

static const char *s_drmConnectorTypeName(uint32_t t) {
	switch (t) {
	case 1: return "VGA";
	case 10: return "DP";
	case 11: return "HDMI-A";
	case 12: return "HDMI-B";
	case 14: return "eDP";
	case 15: return "Virtual";
	case 16: return "DSI";
	default: return "type";
	}
}

// Dump every connector + modes so HW logs show whether we painted the wrong
// HDMI (e.g. forced HDMI-A-1 @640x480 vs real HDMI-A-2 @1080p). Diagnostic only.
static void s_logDrmConnectors(int fd) {
	xl_drm_mode_card_res res{};
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETRESOURCES, &res) != 0 || res.count_connectors == 0) {
		log::source().info("Vk", "DRM: no connectors (getresources failed or empty)");
		return;
	}
	Vector<uint32_t> connectors;
	connectors.resize(res.count_connectors);
	xl_drm_mode_card_res res2{};
	res2.connector_id_ptr = reinterpret_cast<uint64_t>(connectors.data());
	res2.count_connectors = res.count_connectors;
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETRESOURCES, &res2) != 0) {
		return;
	}
	log::source().info("Vk", "DRM: ", res2.count_connectors, " connector(s):");
	for (uint32_t i = 0; i < res2.count_connectors; ++i) {
		xl_drm_mode_get_connector conn{};
		conn.connector_id = connectors[i];
		if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0) {
			continue;
		}
		uint32_t curW = 0, curH = 0;
		if (conn.encoder_id != 0) {
			xl_drm_mode_get_encoder enc{};
			enc.encoder_id = conn.encoder_id;
			if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETENCODER, &enc) == 0 && enc.crtc_id != 0) {
				xl_drm_mode_crtc crtc{};
				crtc.crtc_id = enc.crtc_id;
				if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCRTC, &crtc) == 0 && crtc.mode_valid) {
					curW = crtc.mode.hdisplay;
					curH = crtc.mode.vdisplay;
				}
			}
		}
		log::source().info("Vk", "  [", i, "] id=", connectors[i], " ",
				s_drmConnectorTypeName(conn.connector_type), "-", conn.connector_type_id,
				" status=", s_drmConnectionName(conn.connection),
				" modes=", conn.count_modes,
				" crtc=", curW, "x", curH);

		if (conn.count_modes == 0) {
			continue;
		}
		Vector<xl_drm_mode_modeinfo> modes;
		modes.resize(conn.count_modes);
		xl_drm_mode_get_connector conn2{};
		conn2.connector_id = connectors[i];
		conn2.count_modes = conn.count_modes;
		conn2.modes_ptr = reinterpret_cast<uint64_t>(modes.data());
		if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCONNECTOR, &conn2) != 0) {
			continue;
		}
		// Cap log spam: preferred + first few + largest.
		uint32_t shown = 0;
		uint32_t largestIdx = 0;
		uint64_t largestArea = 0;
		for (uint32_t m = 0; m < conn2.count_modes; ++m) {
			const auto area = uint64_t(modes[m].hdisplay) * uint64_t(modes[m].vdisplay);
			if (area > largestArea) {
				largestArea = area;
				largestIdx = m;
			}
		}
		for (uint32_t m = 0; m < conn2.count_modes; ++m) {
			auto &md = modes[m];
			const bool preferred = (md.type & XL_DRM_MODE_TYPE_PREFERRED) != 0;
			const bool largest = (m == largestIdx);
			if (!preferred && !largest && m >= 3) {
				continue;
			}
			log::source().info("Vk", "      mode ", md.hdisplay, "x", md.vdisplay,
					"@", md.vrefresh,
					preferred ? " preferred" : "",
					largest ? " largest" : "");
			if (++shown >= 8) {
				log::source().info("Vk", "      … ", conn2.count_modes - shown, " more mode(s)");
				break;
			}
		}
	}
}

// Find the first connected connector that has at least one mode. Returns its DRM
// object id (>0) or 0 on failure. Pure UAPI two-pass ioctl — no libdrm.
// (Pick policy unchanged — dump above proves which monitor we would skip.)
static uint32_t s_findConnectedDrmConnector(int fd) {
	s_logDrmConnectors(fd);

	xl_drm_mode_card_res res{};
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETRESOURCES, &res) != 0 || res.count_connectors == 0) {
		return 0;
	}

	Vector<uint32_t> connectors;
	connectors.resize(res.count_connectors);

	// Second pass: only ask for the connector id array (leave other arrays empty).
	xl_drm_mode_card_res res2{};
	res2.connector_id_ptr = reinterpret_cast<uint64_t>(connectors.data());
	res2.count_connectors = res.count_connectors;
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETRESOURCES, &res2) != 0) {
		return 0;
	}

	for (uint32_t i = 0; i < res2.count_connectors; ++i) {
		xl_drm_mode_get_connector conn{};
		conn.connector_id = connectors[i];
		// Pass 1 fills connection status + counts without needing any array buffers.
		if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0) {
			continue;
		}
		if (conn.connection == 1 /* DRM_MODE_CONNECTED */ && conn.count_modes > 0) {
			log::source().info("Vk", "DRM: using first connected id=", connectors[i], " ",
					s_drmConnectorTypeName(conn.connector_type), "-", conn.connector_type_id,
					" (index ", i, ")");
			return connectors[i];
		}
	}
	return 0;
}

// Active CRTC mode for a connected connector (what the kernel already modeset —
// QEMU video=1024x768, RPi preferred EDID after boot). Returns false if unbound.
static bool s_queryDrmCurrentMode(int fd, uint32_t *w, uint32_t *h) {
	const uint32_t connectorId = s_findConnectedDrmConnector(fd);
	if (connectorId == 0) {
		return false;
	}
	xl_drm_mode_get_connector conn{};
	conn.connector_id = connectorId;
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0 || conn.encoder_id == 0) {
		return false;
	}
	xl_drm_mode_get_encoder enc{};
	enc.encoder_id = conn.encoder_id;
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETENCODER, &enc) != 0 || enc.crtc_id == 0) {
		return false;
	}
	xl_drm_mode_crtc crtc{};
	crtc.crtc_id = enc.crtc_id;
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCRTC, &crtc) != 0 || !crtc.mode_valid) {
		return false;
	}
	if (crtc.mode.hdisplay == 0 || crtc.mode.vdisplay == 0) {
		return false;
	}
	*w = crtc.mode.hdisplay;
	*h = crtc.mode.vdisplay;
	return true;
}

// EDID preferred (or largest listed) mode on the connected connector.
static bool s_queryDrmPreferredMode(int fd, uint32_t *w, uint32_t *h) {
	const uint32_t connectorId = s_findConnectedDrmConnector(fd);
	if (connectorId == 0) {
		return false;
	}
	xl_drm_mode_get_connector conn{};
	conn.connector_id = connectorId;
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0 || conn.count_modes == 0) {
		return false;
	}
	Vector<xl_drm_mode_modeinfo> modes;
	modes.resize(conn.count_modes);
	xl_drm_mode_get_connector conn2{};
	conn2.connector_id = connectorId;
	conn2.count_modes = conn.count_modes;
	conn2.modes_ptr = reinterpret_cast<uint64_t>(modes.data());
	if (::ioctl(fd, XL_DRM_IOCTL_MODE_GETCONNECTOR, &conn2) != 0 || conn2.count_modes == 0) {
		return false;
	}
	const xl_drm_mode_modeinfo *best = nullptr;
	for (uint32_t i = 0; i < conn2.count_modes; ++i) {
		auto &m = modes[i];
		if (m.hdisplay == 0 || m.vdisplay == 0) {
			continue;
		}
		const bool preferred = (m.type & XL_DRM_MODE_TYPE_PREFERRED) != 0;
		if (!best) {
			best = &m;
			continue;
		}
		const bool bestPreferred = (best->type & XL_DRM_MODE_TYPE_PREFERRED) != 0;
		if (preferred && !bestPreferred) {
			best = &m;
			continue;
		}
		if (preferred == bestPreferred) {
			const auto ma = uint64_t(m.hdisplay) * uint64_t(m.vdisplay);
			const auto ba = uint64_t(best->hdisplay) * uint64_t(best->vdisplay);
			if (ma > ba || (ma == ba && m.vrefresh > best->vrefresh)) {
				best = &m;
			}
		}
	}
	if (!best) {
		return false;
	}
	*w = best->hdisplay;
	*h = best->vdisplay;
	return true;
}

static bool s_queryFbVirtualSize(uint32_t *w, uint32_t *h) {
	int fd = ::open("/sys/class/graphics/fb0/virtual_size", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	char buf[64] = {};
	const auto n = ::read(fd, buf, sizeof(buf) - 1);
	::close(fd);
	if (n <= 0) {
		return false;
	}
	unsigned long ww = 0, hh = 0;
	const char *p = buf;
	while (*p >= '0' && *p <= '9') {
		ww = ww * 10u + unsigned(*p - '0');
		++p;
	}
	if (*p != ',') {
		return false;
	}
	++p;
	while (*p >= '0' && *p <= '9') {
		hh = hh * 10u + unsigned(*p - '0');
		++p;
	}
	if (ww == 0 || hh == 0) {
		return false;
	}
	*w = uint32_t(ww);
	*h = uint32_t(hh);
	return true;
}

// When the caller passes 0x0, do NOT blindly pick the largest Vulkan mode —
// QEMU virtio-gpu advertises bogus EDID modes (e.g. 5120x2160). Prefer the
// kernel's current modeset, then fb0, then EDID preferred.
static void s_resolveDisplaySizeHint(uint32_t &prefW, uint32_t &prefH) {
	if (prefW != 0 && prefH != 0) {
		return;
	}
	uint32_t w = 0, h = 0;
	const char *src = nullptr;
	int fd = s_openDrmDevice();
	if (fd >= 0) {
		if (s_queryDrmCurrentMode(fd, &w, &h)) {
			src = "drm-crtc";
		} else if (s_queryDrmPreferredMode(fd, &w, &h)) {
			src = "drm-preferred";
		}
		::close(fd);
	}
	if (!src && s_queryFbVirtualSize(&w, &h)) {
		src = "fb0";
	}
	if (src) {
		prefW = w;
		prefH = h;
		log::source().info("Vk", "Display mode hint from ", src, ": ", w, "x", h);
	}
}
#endif

// Build a display-plane surface on an already-resolved VkDisplayKHR: pick the
// largest/fastest mode, find a usable plane, then vkCreateDisplayPlaneSurfaceKHR.
VkSurfaceKHR Instance::makeDisplayPlaneSurface(VkPhysicalDevice phys, VkDisplayKHR display,
		StringView name, uint32_t prefW, uint32_t prefH) const {
#if XL_VK_DRM_DISPLAY
	s_resolveDisplaySizeHint(prefW, prefH);
#endif
	uint32_t nModes = 0;
	vkGetDisplayModePropertiesKHR(phys, display, &nModes, nullptr);
	if (nModes == 0) {
		return VK_NULL_HANDLE;
	}
	Vector<VkDisplayModePropertiesKHR> modes;
	modes.resize(nModes);
	vkGetDisplayModePropertiesKHR(phys, display, &nModes, modes.data());

	// With a size hint, pick the mode whose area is nearest to it (QEMU virtio-gpu
	// advertises bogus huge modes like 5120x2160 that OOM the swapchain). Without a
	// hint, fall back to the largest mode.
	const int64_t target = int64_t(prefW) * int64_t(prefH);
	const VkDisplayModePropertiesKHR *best = nullptr;
	for (auto &m : modes) {
		auto &r = m.parameters.visibleRegion;
		if (!best) {
			best = &m;
			continue;
		}
		auto &br = best->parameters.visibleRegion;
		const int64_t ma = int64_t(r.width) * int64_t(r.height);
		const int64_t ba = int64_t(br.width) * int64_t(br.height);
		bool better;
		if (target > 0) {
			int64_t dm = ma - target, db = ba - target;
			if (dm < 0) { dm = -dm; }
			if (db < 0) { db = -db; }
			better = dm < db || (dm == db && m.parameters.refreshRate > best->parameters.refreshRate);
		} else {
			better = ma > ba
					|| (ma == ba && m.parameters.refreshRate > best->parameters.refreshRate);
		}
		if (better) {
			best = &m;
		}
	}
	if (!best) {
		return VK_NULL_HANDLE;
	}

	uint32_t nPlanes = 0;
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR(phys, &nPlanes, nullptr);
	Vector<VkDisplayPlanePropertiesKHR> planes;
	planes.resize(nPlanes);
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR(phys, &nPlanes, planes.data());

	uint32_t planeIndex = maxOf<uint32_t>();
	uint32_t planeStack = 0;
	for (uint32_t i = 0; i < nPlanes; ++i) {
		uint32_t nd = 0;
		vkGetDisplayPlaneSupportedDisplaysKHR(phys, i, &nd, nullptr);
		if (nd == 0) {
			// Plane not bound to any display yet — usable as a fallback.
			if (planeIndex == maxOf<uint32_t>()) {
				planeIndex = i;
				planeStack = planes[i].currentStackIndex;
			}
			continue;
		}
		Vector<VkDisplayKHR> supported;
		supported.resize(nd);
		vkGetDisplayPlaneSupportedDisplaysKHR(phys, i, &nd, supported.data());
		bool ok = false;
		for (auto &d : supported) {
			if (d == display) {
				ok = true;
				break;
			}
		}
		if (ok) {
			planeIndex = i;
			planeStack = planes[i].currentStackIndex;
			break;
		}
	}
	if (planeIndex == maxOf<uint32_t>()) {
		return VK_NULL_HANDLE;
	}

	VkDisplaySurfaceCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
	info.displayMode = best->displayMode;
	info.planeIndex = planeIndex;
	info.planeStackIndex = planeStack;
	info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	info.globalAlpha = 0.0f;
	info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
	info.imageExtent = best->parameters.visibleRegion;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	auto result = vkCreateDisplayPlaneSurfaceKHR(_instance, &info, nullptr, &surface);
	if (result == VK_SUCCESS) {
		log::source().info("Vk", "Direct display surface created: ", name, " ",
				best->parameters.visibleRegion.width, "x", best->parameters.visibleRegion.height,
				"@", best->parameters.refreshRate / 1000, " (plane ", planeIndex, ")");
		return surface;
	}
	log::source().error("Vk", "vkCreateDisplayPlaneSurfaceKHR failed: ", int(result));
	return VK_NULL_HANDLE;
}

VkSurfaceKHR Instance::createDisplayPlaneSurface(uint32_t prefW, uint32_t prefH) const {
#if XL_VK_DRM_DISPLAY
	s_resolveDisplaySizeHint(prefW, prefH);
#endif
	const int64_t target = int64_t(prefW) * int64_t(prefH);
	for (auto &wrapper : _devices) {
		wrapper.once([&] { getDeviceInfo(wrapper.info, wrapper.info.device); });

		auto &dev = wrapper.info;
		VkPhysicalDevice phys = dev.device;

		// --- Path 1: driver auto-enumerated displays (HW drivers with a bound
		// DRM fd, e.g. V3DV on RPi4). dev.displays/modes/planes already filled. ---
		if (!dev.displays.empty()) {
			const DisplayInfo *display = &dev.displays.front();

			const ModeInfo *targetMode = nullptr;
			for (auto &m : display->modes) {
				if (m.planes.empty()) {
					continue;
				}
				if (!targetMode) {
					targetMode = &m;
					continue;
				}
				const int64_t ma = int64_t(m.info.width) * int64_t(m.info.height);
				const int64_t ta = int64_t(targetMode->info.width) * int64_t(targetMode->info.height);
				bool better;
				if (target > 0) {
					int64_t dm = ma - target, dt = ta - target;
					if (dm < 0) { dm = -dm; }
					if (dt < 0) { dt = -dt; }
					better = dm < dt || (dm == dt && m.info.rate > targetMode->info.rate);
				} else {
					better = ma > ta || (ma == ta && m.info.rate > targetMode->info.rate);
				}
				if (better) {
					targetMode = &m;
				}
			}

			if (targetMode) {
				VkDisplaySurfaceCreateInfoKHR info{};
				info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
				info.displayMode = targetMode->mode;
				info.planeIndex = targetMode->planes.front().index;
				info.planeStackIndex = targetMode->planes.front().index;
				info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
				info.globalAlpha = 0.0f;
				info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
				info.imageExtent = VkExtent2D{targetMode->info.width, targetMode->info.height};

				VkSurfaceKHR surface = VK_NULL_HANDLE;
				auto result = vkCreateDisplayPlaneSurfaceKHR(_instance, &info, nullptr, &surface);
				if (result == VK_SUCCESS) {
					log::source().info("Vk", "Direct display surface created: ", display->name, " ",
							targetMode->info.width, "x", targetMode->info.height, "@",
							targetMode->info.rate / 1000);
					return surface;
				}
				log::source().error("Vk", "vkCreateDisplayPlaneSurfaceKHR failed: ", int(result));
			}
		}

#if XL_VK_DRM_DISPLAY
		// --- Path 2: software/headless drivers (lavapipe) enumerate 0 displays
		// because they hold no DRM fd. Open a KMS connector ourselves and acquire
		// it via VK_EXT_acquire_drm_display, then build the surface as usual. ---
		if (vkGetDrmDisplayEXT && vkAcquireDrmDisplayEXT) {
			int fd = s_openDrmDevice();
			if (fd >= 0) {
				uint32_t connectorId = s_findConnectedDrmConnector(fd);
				if (connectorId != 0) {
					VkDisplayKHR display = VK_NULL_HANDLE;
					auto gr = vkGetDrmDisplayEXT(phys, fd, connectorId, &display);
					if (gr == VK_SUCCESS && display != VK_NULL_HANDLE) {
						auto ar = vkAcquireDrmDisplayEXT(phys, fd, display);
						if (ar == VK_SUCCESS) {
							if (auto surface = makeDisplayPlaneSurface(phys, display,
										StringView("drm-connector"), prefW, prefH)) {
								// Keep fd open: it is the DRM master for the acquired
								// display and must outlive the surface. Intentionally
								// leaked (single fd, lives for the app's lifetime).
								return surface;
							}
						} else {
							log::source().error("Vk", "vkAcquireDrmDisplayEXT failed: ", int(ar));
						}
					} else {
						log::source().error("Vk", "vkGetDrmDisplayEXT failed: ", int(gr));
					}
				} else {
					log::source().error("Vk", "No connected DRM connector found on ", fd);
				}
				::close(fd);
			} else {
				log::source().error("Vk", "Fail to open /dev/dri/card* for direct display");
			}
		}
#endif
	}

	log::source().error("Vk", "No VK_KHR_display display available for direct output");
	return VK_NULL_HANDLE;
}

SurfaceBackendMask Instance::checkPresentationSupport(VkPhysicalDevice device,
		uint32_t qIdx) const {
	SurfaceBackendMask ret =
			_checkPresentSupport ? _checkPresentSupport(this, device, qIdx) : SurfaceBackendMask();
	ret.reset(0);
	return ret & _surfaceBackendMask;
}

StringView getSurfaceBackendExtension(SurfaceBackend backend) {
	switch (backend) {
	case SurfaceBackend::Surface: return StringView("VK_KHR_surface"); break;
	case SurfaceBackend::Android: return StringView("VK_KHR_android_surface"); break;
	case SurfaceBackend::Wayland: return StringView("VK_KHR_wayland_surface"); break;
	case SurfaceBackend::Win32: return StringView("VK_KHR_win32_surface"); break;
	case SurfaceBackend::Xcb: return StringView("VK_KHR_xcb_surface"); break;
	case SurfaceBackend::XLib: return StringView("VK_KHR_xlib_surface"); break;
	case SurfaceBackend::DirectFb: return StringView("VK_EXT_directfb_surface"); break;
	case SurfaceBackend::Fuchsia: return StringView("VK_FUCHSIA_imagepipe_surface"); break;
	case SurfaceBackend::GoogleGames: return StringView("VK_GGP_stream_descriptor_surface"); break;
	case SurfaceBackend::IOS: return StringView("VK_MVK_ios_surface"); break;
	case SurfaceBackend::MacOS: return StringView("VK_MVK_macos_surface"); break;
	case SurfaceBackend::VI: return StringView("VK_NN_vi_surface"); break;
	case SurfaceBackend::Metal: return StringView("VK_EXT_metal_surface"); break;
	case SurfaceBackend::QNX: return StringView("VK_QNX_screen_surface"); break;
	case SurfaceBackend::OpenHarmony: return StringView("VK_OHOS_surface"); break;
	case SurfaceBackend::Display: return StringView("VK_KHR_display"); break;
	case SurfaceBackend::Canvas: break;
	case SurfaceBackend::Max: break;
	}
	return StringView();
}

SurfaceBackend getSurfaceBackendForExtension(StringView ext) {
	if (ext == "VK_KHR_surface") {
		return SurfaceBackend::Surface;
	} else if (ext == "VK_KHR_android_surface") {
		return SurfaceBackend::Android;
	} else if (ext == "VK_KHR_wayland_surface") {
		return SurfaceBackend::Wayland;
	} else if (ext == "VK_KHR_win32_surface") {
		return SurfaceBackend::Win32;
	} else if (ext == "VK_KHR_xcb_surface") {
		return SurfaceBackend::Xcb;
	} else if (ext == "VK_KHR_xlib_surface") {
		return SurfaceBackend::XLib;
	} else if (ext == "VK_EXT_directfb_surface") {
		return SurfaceBackend::DirectFb;
	} else if (ext == "VK_FUCHSIA_imagepipe_surface") {
		return SurfaceBackend::Fuchsia;
	} else if (ext == "VK_GGP_stream_descriptor_surface") {
		return SurfaceBackend::GoogleGames;
	} else if (ext == "VK_MVK_ios_surface") {
		return SurfaceBackend::IOS;
	} else if (ext == "VK_MVK_macos_surface") {
		return SurfaceBackend::MacOS;
	} else if (ext == "VK_NN_vi_surface") {
		return SurfaceBackend::VI;
	} else if (ext == "VK_EXT_metal_surface") {
		return SurfaceBackend::Metal;
	} else if (ext == "VK_QNX_screen_surface") {
		return SurfaceBackend::QNX;
	} else if (ext == "VK_OHOS_surface") {
		return SurfaceBackend::OpenHarmony;
	} else if (ext == "VK_KHR_display") {
		return SurfaceBackend::Display;
	}
	return SurfaceBackend::Max;
}

} // namespace stappler::xenolith::vk
