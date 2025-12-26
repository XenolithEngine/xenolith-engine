/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKINFO_H_
#define XENOLITH_BACKEND_VK_XLVKINFO_H_

#include "SPGeometry.h"
#include "XLVk.h"
#include "XlCoreMonitorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

struct DisplayInfo;

struct SP_PUBLIC ModePlaneInfo {
	uint32_t index;
	VkDisplayPlaneCapabilitiesKHR caps;
};

struct SP_PUBLIC ModeInfo {
	VkDisplayModeKHR mode;
	core::ModeInfo info;

	Vector<ModePlaneInfo> planes;
};

struct SP_PUBLIC PlaneInfo {
	uint32_t stackIndex = 0;
	const DisplayInfo *current = nullptr;
	Vector<const DisplayInfo *> displays;
};

struct SP_PUBLIC DisplayInfo {
	VkDisplayKHR display;
	String name;

	Extent2 mm;
	Extent2 extent;

	Vector<const PlaneInfo *> planes;
	Vector<ModeInfo> modes;

	bool operator==(const core::MonitorId &id) const { return id.name == name; }

	operator core::MonitorInfo() const {
		core::MonitorInfo mon;
		mon.name = name;
		mon.rect = IRect{0, 0, extent.width, extent.height};
		mon.mm = mm;

		for (auto &it : modes) { mon.modes.emplace_back(it.info); }

		return mon;
	}
};

struct SP_PUBLIC DeviceInfo {
	using OptVec = std::bitset<toInt(OptionalDeviceExtension::Max)>;

	struct Features {
		static Features getRequired();
		static Features getOptional();

#ifdef VK_ENABLE_BETA_EXTENSIONS
		VkPhysicalDevicePortabilitySubsetFeaturesKHR devicePortability = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR, nullptr};
#endif
		VkPhysicalDevice16BitStorageFeaturesKHR device16bitStorage = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR, nullptr};
		VkPhysicalDevice8BitStorageFeaturesKHR device8bitStorage = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR, nullptr};
		VkPhysicalDeviceShaderFloat16Int8FeaturesKHR deviceShaderFloat16Int8 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR, nullptr};
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT deviceDescriptorIndexing = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr};
		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR deviceBufferDeviceAddress = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR, nullptr};

#if VK_VERSION_1_3
		VkPhysicalDeviceVulkan13Features device13 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, nullptr};
#endif
		VkPhysicalDeviceVulkan12Features device12 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, nullptr};
		VkPhysicalDeviceVulkan11Features device11 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, nullptr};
		VkPhysicalDeviceFeatures2KHR device10 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
			nullptr};

		VkExternalFenceProperties fenceSyncFd = {VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES,
			nullptr, 0, 0, 0};

		OptVec optionals;

		bool canEnable(const Features &, uint32_t version) const;

		// enables all features, that enabled in f
		void enableFromFeatures(const Features &);

		// disables all features, that disabled in f
		void disableFromFeatures(const Features &f);

		void updateFrom13();
		void updateFrom12();
		void updateTo12(bool updateFlags = false);

		void clear();

		Features();
		Features(const Features &);
		Features &operator=(const Features &);
	};

	struct Properties {
#ifdef VK_ENABLE_BETA_EXTENSIONS
		VkPhysicalDevicePortabilitySubsetPropertiesKHR devicePortability = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR, nullptr};
#endif
		VkPhysicalDeviceDescriptorIndexingPropertiesEXT deviceDescriptorIndexing = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT, nullptr};
		VkPhysicalDeviceMaintenance3PropertiesKHR deviceMaintenance3 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES_KHR, nullptr};
		VkPhysicalDeviceProperties2KHR device10 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, nullptr};

		Properties();
		Properties(const Properties &);
		Properties &operator=(const Properties &);
	};

	struct QueueFamilyInfo {
		core::QueueFlags flags = core::QueueFlags::None;
		uint32_t index = 0;
		uint32_t count = 0;
		uint32_t used = 0;
		uint32_t timestampValidBits = 0;
		Extent3 minImageTransferGranularity;
		SurfaceBackendMask presentSurfaceMask;
	};

	VkPhysicalDevice device = VK_NULL_HANDLE;
	QueueFamilyInfo graphicsFamily;
	QueueFamilyInfo presentFamily;
	QueueFamilyInfo transferFamily;
	QueueFamilyInfo computeFamily;

	Vector<StringView> optionalExtensions;
	Vector<StringView> promotedExtensions;
	Vector<String> availableExtensions;

	Properties properties;
	Features features;

	bool requiredExtensionsExists = false;
	bool requiredFeaturesExists = false;

	Vector<uint32_t> knownPlanes;
	Vector<PlaneInfo> planes;
	Vector<DisplayInfo> displays;

	DeviceInfo();
	DeviceInfo(VkPhysicalDevice, QueueFamilyInfo, QueueFamilyInfo, QueueFamilyInfo, QueueFamilyInfo,
			Vector<StringView> &&, Vector<StringView> &&);
	DeviceInfo(const DeviceInfo &) = default;
	DeviceInfo &operator=(const DeviceInfo &) = default;
	DeviceInfo(DeviceInfo &&) = default;
	DeviceInfo &operator=(DeviceInfo &&) = default;

	bool supportsPresentation() const;

	String description() const;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKINFO_H_ */
