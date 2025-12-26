/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLVkPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

SPUNUSED static VKAPI_ATTR VkBool32 VKAPI_CALL s_debugMessageCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

Rc<Instance> FunctionTable::createInstance(NotNull<core::InstanceInfo> instanceInfo,
		NotNull<InstanceBackendInfo> backend, Dso &&vulkanModule) const {
	InstanceInfo info = loadInfo();
	info.flags = instanceInfo->flags;

	bool validationEnabled = false;

	InstanceData data;
	if (!prepareData(data, info)) {
		return nullptr;
	}

	if (!backend->setup(data, info)) {
		log::source().warn("Vk", "VkInstance creation was aborted by client");
		return nullptr;
	}

	if (!validateData(data, info, validationEnabled)) {
		return nullptr;
	}

	return doCreateInstance(data, info, sp::move(vulkanModule), validationEnabled);
}

InstanceInfo FunctionTable::loadInfo() const {
	InstanceInfo ret;

	if (_instanceVersion == 0) {
		if (vkEnumerateInstanceVersion) {
			vkEnumerateInstanceVersion(&_instanceVersion);
		} else {
			_instanceVersion = VK_API_VERSION_1_0;
		}
	}

	if (_instanceAvailableLayers.empty()) {
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		_instanceAvailableLayers.resize(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, _instanceAvailableLayers.data());
	}

	if (_instanceAvailableExtensions.empty()) {
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		_instanceAvailableExtensions.resize(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
				_instanceAvailableExtensions.data());
	}

	ret.targetVersion = _instanceVersion;
	ret.availableLayers = _instanceAvailableLayers;
	ret.availableExtensions = _instanceAvailableExtensions;

	for (auto &extension : ret.availableExtensions) {
		if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			ret.hasSurfaceExtension = true;
		} else {
			auto b = getSurfaceBackendForExtension(StringView(extension.extensionName));
			if (b != SurfaceBackend::Max) {
				ret.availableBackends.set(toInt(b));
			}
		}
	}

	return ret;
}

bool FunctionTable::prepareData(InstanceData &data, const InstanceInfo &info) const {
	data.targetVulkanVersion = info.targetVersion;
	return true;
}

bool FunctionTable::validateData(InstanceData &data, const InstanceInfo &info,
		bool &validationEnabled) const {
	if ((info.availableBackends & data.enableBackends) != data.enableBackends) {
		log::source().error("Vk", "Invalid flags for surface backends");
		return false;
	}

	bool validationLayerFound = false;
	if (hasFlag(info.flags, core::InstanceFlags::Validation)) {
		for (const char *layerName : vk::s_validationLayers) {
			for (const auto &layerProperties : _instanceAvailableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					data.enableLayer(layerName);
					validationLayerFound = true;
					break;
				}
			}

			if (!validationLayerFound) {
				log::source().error("Vk", "Validation layer not found: ", layerName);
				if (hasFlag(info.flags, core::InstanceFlags::ForcedValidation)) {
					log::source().error("Vk", "Forced validation flag is set: aborting");
					return false;
				}
			} else {
				validationEnabled = true;
			}
		}
	}

	// search for a debug extension
	const char *debugExt = nullptr;
	for (auto &extension : _instanceAvailableExtensions) {
		if (hasFlag(info.flags, core::InstanceFlags::Validation)) {
			if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension.extensionName) == 0) {
				data.enableExtension(extension.extensionName);
				debugExt = extension.extensionName;
				continue;
			}
		}
	}

	if (hasFlag(info.flags, core::InstanceFlags::Validation)) {
		if (!debugExt && validationLayerFound) {
			for (const auto &layerName : vk::s_validationLayers) {
				uint32_t layerExtCount = 0;
				vkEnumerateInstanceExtensionProperties(layerName, &layerExtCount, nullptr);
				if (layerExtCount == 0) {
					continue;
				}

				VkExtensionProperties layer_exts[layerExtCount];
				vkEnumerateInstanceExtensionProperties(layerName, &layerExtCount, layer_exts);

				for (auto &extension : layer_exts) {
					if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension.extensionName) == 0) {
						data.extensionsToEnable.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
						debugExt = extension.extensionName;
						break;
					}
				}

				if (debugExt) {
					break;
				}
			}
		}

		if (!debugExt) {
			log::source().error("Vk",
					"Required extension not found: ", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
	}

	// drop Surface bit
	data.enableBackends.reset(0);

	if (data.enableBackends.any()) {
		data.enableExtension(getSurfaceBackendExtension(SurfaceBackend::Surface).data());
		for (auto it : sp::each<SurfaceBackend>()) {
			if (data.enableBackends.test(toInt(it))) {
				auto ext = getSurfaceBackendExtension(it);
				if (!ext.empty()) {
					data.enableExtension(ext.data());
				}
			}
		}
	}

	bool completeExt = true;
	for (auto &it : vk::s_requiredExtension) {
		if (!it) {
			break;
		}

		bool found = false;
		for (auto &extension : info.availableExtensions) {
			if (strcmp(it, extension.extensionName) == 0) {
				found = true;
				data.enableExtension(it);
			}
		}
		if (!found) {
			log::source().error("Vk", "Required extension not found: ", it);
			completeExt = false;
		}
	}

	if (!completeExt) {
		log::source().error("Vk", "Not all required extensions found, fail to create VkInstance");
		return false;
	}

	return true;
}

Rc<Instance> FunctionTable::doCreateInstance(InstanceData &data, const InstanceInfo &info,
		Dso &&vulkanModule, bool validationEnabled) const {
	Instance::OptVec enabledOptionals;

	uint32_t optIdx = 0;
	for (auto &opt : s_optionalExtension) {
		if (!opt) {
			break;
		}
		for (auto &extension : info.availableExtensions) {
			if (strcmp(opt, extension.extensionName) == 0) {
				enabledOptionals.set(optIdx);
				data.enableExtension(opt);
				break;
			}
		}
		++optIdx;
	}

	VkInstance instance = VK_NULL_HANDLE;

	VkApplicationInfo appInfo{};
	vk::sanitizeVkStruct(appInfo);
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = data.applicationName.data();
	appInfo.applicationVersion = XL_MAKE_API_VERSION(data.applicationVersion);
	appInfo.pEngineName = xenolith::getEngineName();
	appInfo.engineVersion = xenolith::getVersionIndex();
	appInfo.apiVersion = data.targetVulkanVersion;

	VkInstanceCreateInfo createInfo{};
	vk::sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
#if MACOS
	createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#else
	createInfo.flags = 0;
#endif
	createInfo.pApplicationInfo = &appInfo;

	createInfo.enabledExtensionCount = uint32_t(data.extensionsToEnable.size());
	createInfo.ppEnabledExtensionNames = data.extensionsToEnable.data();

	enum VkResult ret = VK_SUCCESS;
	if (validationEnabled) {
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = s_debugMessageCallback;
		createInfo.pNext = &debugCreateInfo;
	} else {
		createInfo.pNext = nullptr;
	}

	createInfo.enabledLayerCount = uint32_t(data.layersToEnable.size());
	createInfo.ppEnabledLayerNames = data.layersToEnable.data();

	if (validationEnabled && hasFlag(info.flags, core::InstanceFlags::ValidateSynchromization)) {
		VkValidationFeaturesEXT validationExt;
		validationExt.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		validationExt.pNext = createInfo.pNext;

		VkValidationFeatureEnableEXT feature =
				VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;

		validationExt.enabledValidationFeatureCount = 1;
		validationExt.pEnabledValidationFeatures = &feature;

		validationExt.disabledValidationFeatureCount = 0;
		validationExt.pDisabledValidationFeatures = nullptr;

		createInfo.pNext = &validationExt;
	}

	ret = vkCreateInstance(&createInfo, nullptr, &instance);

	if (ret != VK_SUCCESS) {
		log::source().error("Vk", "Fail to create Vulkan instance");
		return nullptr;
	}

	auto flags = info.flags;
	if (!validationEnabled) {
		flags &= ~core::InstanceFlags::Validation;
	}

	auto vkInstance = Rc<vk::Instance>::alloc(instance, vkGetInstanceProcAddr,
			data.targetVulkanVersion, sp::move(enabledOptionals), sp::move(vulkanModule),
			sp::move(data.checkPresentationSupport), sp::move(data.enableBackends), flags);

	if constexpr (vk::s_printVkInfo) {
		StringStream out;
		out << "\n\tVulkan: " << getVersionDescription(_instanceVersion) << "\n\tLayers:\n";
		for (const auto &layerProperties : _instanceAvailableLayers) {
			out << "\t\t" << layerProperties.layerName << " ("
				<< getVersionDescription(layerProperties.specVersion) << "/"
				<< getVersionDescription(layerProperties.implementationVersion) << ")\t - "
				<< layerProperties.description << "\n";
		}

		out << "\tExtensions:\n";
		for (const auto &extension : _instanceAvailableExtensions) {
			out << "\t\t" << extension.extensionName << ": "
				<< getVersionDescription(extension.specVersion) << "\n";
		}

		vkInstance->printDevicesInfo(out);

		log::source().verbose("Vk-Info", out.str());
	}

	return vkInstance;
}

} // namespace stappler::xenolith::vk::platform
