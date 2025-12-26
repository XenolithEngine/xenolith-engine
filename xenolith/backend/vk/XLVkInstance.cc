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
#include "XLCoreDevice.h"
#include <vulkan/vulkan_core.h>

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
			&& strcmp(pCallbackData->pMessageIdName,
					   "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274")
					== 0) {
		// this is normal for multithreaded engine
		messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	}
	if (pCallbackData->pMessageIdName
			&& strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0) {
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
	auto it = std::find_if(layersToEnable.begin(), layersToEnable.end(),
			[&](const char *ptr) { return strcmp(str, ptr) == 0; });
	if (it == layersToEnable.end()) {
		layersToEnable.emplace_back(str);
	}
}

void InstanceData::enableExtension(const char *str) {
	auto it = std::find_if(extensionsToEnable.begin(), extensionsToEnable.end(),
			[&](const char *ptr) { return strcmp(str, ptr) == 0; });
	if (it == extensionsToEnable.end()) {
		extensionsToEnable.emplace_back(str);
	}
}

Instance::Instance(VkInstance inst, const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		uint32_t targetVersion, OptVec &&optionals, Dso &&vulkanModule,
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
			auto &it = _devices.emplace_back(getDeviceInfo(device));

			_availableDevices.emplace_back(core::DeviceProperties{
				String(it.properties.device10.properties.deviceName),
				it.properties.device10.properties.apiVersion,
				it.properties.device10.properties.driverVersion, it.supportsPresentation()});
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

Rc<core::Loop> Instance::makeLoop(NotNull<event::Looper> looper, Rc<core::LoopInfo> &&info) const {
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
				auto iit = std::find(dev.availableExtensions.begin(), dev.availableExtensions.end(),
						req);
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
			if (!isDeviceSupported(it)) {
				log::source().warn("vk::Instance", "Device rejected: device is not supported");
				continue;
			}

			auto requiredExtensions = getDeviceExtensions(it);
			if (!isExtensionsSupported(it, requiredExtensions)) {
				log::source().warn("vk::Instance",
						"Device rejected: required extensions is not available");
				continue;
			}

			DeviceInfo::Features targetFeatures;
			if (!buildFeaturesList(it, targetFeatures)) {
				log::source().warn("vk::Instance",
						"Device rejected: required features is not available");
				continue;
			}

			if (it.features.canEnable(targetFeatures,
						it.properties.device10.properties.apiVersion)) {
				return Rc<vk::Device>::create(this, DeviceInfo(it), targetFeatures,
						requiredExtensions);
			}
		}
	} else if (info.deviceIdx < _devices.size()) {
		auto &dev = _devices[info.deviceIdx];
		if (!isDeviceSupported(dev)) {
			log::source().error("vk::Instance", "Fail to create device: device is not supported");
			return nullptr;
		}

		auto requiredExtensions = getDeviceExtensions(dev);
		if (!isExtensionsSupported(dev, requiredExtensions)) {
			log::source().error("vk::Instance",
					"Fail to create device: required extensions is not available");
			return nullptr;
		}

		DeviceInfo::Features targetFeatures;
		if (!buildFeaturesList(dev, targetFeatures)) {
			log::source().error("vk::Instance",
					"Fail to create device: required features is not available");
			return nullptr;
		}

		if (dev.features.canEnable(targetFeatures, dev.properties.device10.properties.apiVersion)) {
			return Rc<vk::Device>::create(this, DeviceInfo(dev), targetFeatures,
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

		std::sort(ret.presentModes.begin(), ret.presentModes.end(),
				[&](core::PresentMode l, core::PresentMode r) { return toInt(l) > toInt(r); });
	}

	// index into s_optionalExtension
	if (_optionals[toInt(OptionalInstanceExtension::SurfaceCapabilities2)]) {
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

void Instance::printDevicesInfo(std::ostream &out) const {
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
		out << "\tDevice: " << device.device << " "
			<< getDeviceTypeString(device.properties.device10.properties.deviceType) << ": "
			<< device.properties.device10.properties.deviceName
			<< " (API: " << getVersionDescription(device.properties.device10.properties.apiVersion)
			<< ", Driver: "
			<< getVersionDescription(device.properties.device10.properties.driverVersion) << ")\n";

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount, nullptr);

		Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount,
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

			VkBool32 presentSupport = checkPresentationSupport(device.device, i).any();
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
		out << device.description();
	}
}

void Instance::getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &features,
		const DeviceInfo::OptVec &flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if (flags[toInt(OptionalDeviceExtension::Portability)]) {
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
		if (flags[toInt(OptionalDeviceExtension::Storage16Bit)]) {
			features.device16bitStorage.pNext = next;
			next = &features.device16bitStorage;
		}
		if (flags[toInt(OptionalDeviceExtension::Storage8Bit)]) {
			features.device8bitStorage.pNext = next;
			next = &features.device8bitStorage;
		}
		if (flags[toInt(OptionalDeviceExtension::ShaderFloat16Int8)]) {
			features.deviceShaderFloat16Int8.pNext = next;
			next = &features.deviceShaderFloat16Int8;
		}
		if (flags[toInt(OptionalDeviceExtension::DescriptorIndexing)]) {
			features.deviceDescriptorIndexing.pNext = next;
			next = &features.deviceDescriptorIndexing;
		}
		if (flags[toInt(OptionalDeviceExtension::DeviceAddress)]) {
			features.deviceBufferDeviceAddress.pNext = next;
			next = &features.deviceBufferDeviceAddress;
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
	if (flags[toInt(OptionalDeviceExtension::Portability)]) {
		properties.devicePortability.pNext = next;
		next = &properties.devicePortability;
	}
#endif
	if (flags[toInt(OptionalDeviceExtension::Maintenance3)]) {
		properties.deviceMaintenance3.pNext = next;
		next = &properties.deviceMaintenance3;
	}
	if (flags[toInt(OptionalDeviceExtension::DescriptorIndexing)]) {
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

DeviceInfo Instance::getDeviceInfo(VkPhysicalDevice device) const {
	DeviceInfo ret;
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
			if (queueInfo[computeFamily].count >= queueInfo[graphicsFamily].count) {
				transferFamily = computeFamily;
			} else {
				transferFamily = graphicsFamily;
			}
		}
	}

	// try to map present with graphics
	if (presentFamily != graphicsFamily) {
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
			if (strcmp(extensionName, extension.extensionName) == 0) {
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

	return ret;
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
