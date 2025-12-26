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

#include "XLVkInfo.h"
#include "XLVk.h"
#include "XLVkInstance.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

DeviceInfo::Features DeviceInfo::Features::getRequired() {
	Features ret;
	ret.device10.features.independentBlend = VK_TRUE;
	return ret;
}

DeviceInfo::Features DeviceInfo::Features::getOptional() {
	Features ret;
	ret.device10.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.fillModeNonSolid = VK_TRUE;
	ret.device10.features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.multiDrawIndirect = VK_TRUE;
	ret.device10.features.shaderFloat64 = VK_TRUE;
	ret.device10.features.shaderInt64 = VK_TRUE;
	ret.device10.features.shaderInt16 = VK_TRUE;
	ret.deviceShaderFloat16Int8.shaderFloat16 = VK_TRUE;
	ret.deviceShaderFloat16Int8.shaderInt8 = VK_TRUE;
	ret.device16bitStorage.storageBuffer16BitAccess = VK_TRUE;
	//ret.device16bitStorage.uniformAndStorageBuffer16BitAccess = VK_TRUE;
	//ret.device16bitStorage.storageInputOutput16 = VK_TRUE;
	ret.device8bitStorage.storageBuffer8BitAccess = VK_TRUE;
	//ret.device8bitStorage.uniformAndStorageBuffer8BitAccess = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
	ret.deviceDescriptorIndexing.runtimeDescriptorArray = VK_TRUE;
	ret.deviceBufferDeviceAddress.bufferDeviceAddress = VK_TRUE;

	ret.optionals.set(toInt(OptionalDeviceExtension::Maintenance3));
	ret.optionals.set(toInt(OptionalDeviceExtension::DescriptorIndexing));
	ret.optionals.set(toInt(OptionalDeviceExtension::DrawIndirectCount));
	ret.optionals.set(toInt(OptionalDeviceExtension::Storage16Bit));
	ret.optionals.set(toInt(OptionalDeviceExtension::Storage8Bit));
	ret.optionals.set(toInt(OptionalDeviceExtension::ShaderFloat16Int8));
	ret.optionals.set(toInt(OptionalDeviceExtension::MemoryBudget));
	ret.optionals.set(toInt(OptionalDeviceExtension::DedicatedAllocation));
	ret.optionals.set(toInt(OptionalDeviceExtension::GetMemoryRequirements2));
	ret.optionals.set(toInt(OptionalDeviceExtension::ExternalFenceFd));


#ifdef VK_ENABLE_BETA_EXTENSIONS
	ret.devicePortability.constantAlphaColorBlendFactors = VK_TRUE;
	ret.devicePortability.events = VK_TRUE;
	ret.devicePortability.imageViewFormatSwizzle = VK_TRUE;
	ret.devicePortability.shaderSampleRateInterpolationFunctions = VK_TRUE;

	ret.optionals.set(toInt(OptionalDeviceExtension::Portability));
#endif

	ret.updateTo12();
	return ret;
}

bool DeviceInfo::Features::canEnable(const Features &features, uint32_t version) const {
	auto doCheck = [](SpanView<VkBool32> src, SpanView<VkBool32> trg) {
		for (size_t i = 0; i < src.size(); ++i) {
			if (trg[i] && !src[i]) {
				return false;
			}
		}
		return true;
	};

	if (!doCheck(SpanView<VkBool32>(&device10.features.robustBufferAccess,
						 sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)),
				SpanView<VkBool32>(&features.device10.features.robustBufferAccess,
						sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)))) {
		return false;
	}

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#ifdef VK_ENABLE_BETA_EXTENSIONS
	if (!doCheck(SP_VK_BOOL_ARRAY(devicePortability, constantAlphaColorBlendFactors,
						 VkPhysicalDevicePortabilitySubsetFeaturesKHR),
				SP_VK_BOOL_ARRAY(features.devicePortability, constantAlphaColorBlendFactors,
						VkPhysicalDevicePortabilitySubsetFeaturesKHR))) {
		return false;
	}
#endif

#if VK_VERSION_1_2
	if (version >= VK_API_VERSION_1_2) {
		if (!doCheck(SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess,
							 VkPhysicalDeviceVulkan11Features),
					SP_VK_BOOL_ARRAY(features.device11, storageBuffer16BitAccess,
							VkPhysicalDeviceVulkan11Features))) {
			return false;
		}

		if (!doCheck(SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge,
							 VkPhysicalDeviceVulkan12Features),
					SP_VK_BOOL_ARRAY(features.device12, samplerMirrorClampToEdge,
							VkPhysicalDeviceVulkan12Features))) {
			return false;
		}
	}
#endif

	if (!doCheck(SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess,
						 VkPhysicalDevice16BitStorageFeaturesKHR),
				SP_VK_BOOL_ARRAY(features.device16bitStorage, storageBuffer16BitAccess,
						VkPhysicalDevice16BitStorageFeaturesKHR))) {
		return false;
	}

	if (!doCheck(SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess,
						 VkPhysicalDevice8BitStorageFeaturesKHR),
				SP_VK_BOOL_ARRAY(features.device8bitStorage, storageBuffer8BitAccess,
						VkPhysicalDevice8BitStorageFeaturesKHR))) {
		return false;
	}

	if (!doCheck(SP_VK_BOOL_ARRAY(deviceDescriptorIndexing,
						 shaderInputAttachmentArrayDynamicIndexing,
						 VkPhysicalDeviceDescriptorIndexingFeaturesEXT),
				SP_VK_BOOL_ARRAY(features.deviceDescriptorIndexing,
						shaderInputAttachmentArrayDynamicIndexing,
						VkPhysicalDeviceDescriptorIndexingFeaturesEXT))) {
		return false;
	}

	if (!doCheck(SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress,
						 VkPhysicalDeviceBufferDeviceAddressFeaturesKHR),
				SP_VK_BOOL_ARRAY(features.deviceBufferDeviceAddress, bufferDeviceAddress,
						VkPhysicalDeviceBufferDeviceAddressFeaturesKHR))) {
		return false;
	}
	if (!doCheck(SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16,
						 VkPhysicalDeviceShaderFloat16Int8FeaturesKHR),
				SP_VK_BOOL_ARRAY(features.deviceShaderFloat16Int8, shaderFloat16,
						VkPhysicalDeviceShaderFloat16Int8FeaturesKHR))) {
		return false;
	}
#undef SP_VK_BOOL_ARRAY

	return true;
}

void DeviceInfo::Features::enableFromFeatures(const Features &features) {
	auto doCheck = [](SpanView<VkBool32> src, SpanView<VkBool32> trg) {
		for (size_t i = 0; i < src.size(); ++i) {
			if (trg[i]) {
				const_cast<VkBool32 &>(src[i]) = trg[i];
			}
		}
	};

	doCheck(SpanView<VkBool32>(&device10.features.robustBufferAccess,
					sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)),
			SpanView<VkBool32>(&features.device10.features.robustBufferAccess,
					sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)));

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#ifdef VK_ENABLE_BETA_EXTENSIONS
	doCheck(SP_VK_BOOL_ARRAY(devicePortability, constantAlphaColorBlendFactors,
					VkPhysicalDevicePortabilitySubsetFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.devicePortability, constantAlphaColorBlendFactors,
					VkPhysicalDevicePortabilitySubsetFeaturesKHR));
#endif

#if VK_VERSION_1_2
	doCheck(SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features),
			SP_VK_BOOL_ARRAY(features.device11, storageBuffer16BitAccess,
					VkPhysicalDeviceVulkan11Features));

	doCheck(SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features),
			SP_VK_BOOL_ARRAY(features.device12, samplerMirrorClampToEdge,
					VkPhysicalDeviceVulkan12Features));
#endif

	doCheck(SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess,
					VkPhysicalDevice16BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device16bitStorage, storageBuffer16BitAccess,
					VkPhysicalDevice16BitStorageFeaturesKHR));

	doCheck(SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess,
					VkPhysicalDevice8BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device8bitStorage, storageBuffer8BitAccess,
					VkPhysicalDevice8BitStorageFeaturesKHR));

	doCheck(SP_VK_BOOL_ARRAY(deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing,
					VkPhysicalDeviceDescriptorIndexingFeaturesEXT),
			SP_VK_BOOL_ARRAY(features.deviceDescriptorIndexing,
					shaderInputAttachmentArrayDynamicIndexing,
					VkPhysicalDeviceDescriptorIndexingFeaturesEXT));

	doCheck(SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress,
					VkPhysicalDeviceBufferDeviceAddressFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceBufferDeviceAddress, bufferDeviceAddress,
					VkPhysicalDeviceBufferDeviceAddressFeaturesKHR));

	doCheck(SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16,
					VkPhysicalDeviceShaderFloat16Int8FeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceShaderFloat16Int8, shaderFloat16,
					VkPhysicalDeviceShaderFloat16Int8FeaturesKHR));
#undef SP_VK_BOOL_ARRAY
}

void DeviceInfo::Features::disableFromFeatures(const Features &features) {
	auto doCheck = [](SpanView<VkBool32> src, SpanView<VkBool32> trg) {
		for (size_t i = 0; i < src.size(); ++i) {
			if (!trg[i]) {
				const_cast<VkBool32 &>(src[i]) = trg[i];
			}
		}
	};

	doCheck(SpanView<VkBool32>(&device10.features.robustBufferAccess,
					sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)),
			SpanView<VkBool32>(&features.device10.features.robustBufferAccess,
					sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)));

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#ifdef VK_ENABLE_BETA_EXTENSIONS
	doCheck(SP_VK_BOOL_ARRAY(devicePortability, constantAlphaColorBlendFactors,
					VkPhysicalDevicePortabilitySubsetFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.devicePortability, constantAlphaColorBlendFactors,
					VkPhysicalDevicePortabilitySubsetFeaturesKHR));
#endif

#if VK_VERSION_1_2
	doCheck(SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features),
			SP_VK_BOOL_ARRAY(features.device11, storageBuffer16BitAccess,
					VkPhysicalDeviceVulkan11Features));

	doCheck(SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features),
			SP_VK_BOOL_ARRAY(features.device12, samplerMirrorClampToEdge,
					VkPhysicalDeviceVulkan12Features));
#endif

	doCheck(SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess,
					VkPhysicalDevice16BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device16bitStorage, storageBuffer16BitAccess,
					VkPhysicalDevice16BitStorageFeaturesKHR));

	doCheck(SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess,
					VkPhysicalDevice8BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device8bitStorage, storageBuffer8BitAccess,
					VkPhysicalDevice8BitStorageFeaturesKHR));

	doCheck(SP_VK_BOOL_ARRAY(deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing,
					VkPhysicalDeviceDescriptorIndexingFeaturesEXT),
			SP_VK_BOOL_ARRAY(features.deviceDescriptorIndexing,
					shaderInputAttachmentArrayDynamicIndexing,
					VkPhysicalDeviceDescriptorIndexingFeaturesEXT));

	doCheck(SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress,
					VkPhysicalDeviceBufferDeviceAddressFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceBufferDeviceAddress, bufferDeviceAddress,
					VkPhysicalDeviceBufferDeviceAddressFeaturesKHR));

	doCheck(SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16,
					VkPhysicalDeviceShaderFloat16Int8FeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceShaderFloat16Int8, shaderFloat16,
					VkPhysicalDeviceShaderFloat16Int8FeaturesKHR));
#undef SP_VK_BOOL_ARRAY
}


void DeviceInfo::Features::updateFrom13() {
#if VK_VERSION_1_3
#endif
	updateFrom12();
}

void DeviceInfo::Features::updateFrom12() {
#if VK_VERSION_1_2
	if (device11.storageBuffer16BitAccess == VK_TRUE) {
		optionals.set(toInt(OptionalDeviceExtension::Storage16Bit));
	} else {
		optionals.reset(toInt(OptionalDeviceExtension::Storage16Bit));
	}

	device16bitStorage.storageBuffer16BitAccess = device11.storageBuffer16BitAccess;
	device16bitStorage.uniformAndStorageBuffer16BitAccess =
			device11.uniformAndStorageBuffer16BitAccess;
	device16bitStorage.storagePushConstant16 = device11.storagePushConstant16;
	device16bitStorage.storageInputOutput16 = device11.storageInputOutput16;

	if (device12.drawIndirectCount == VK_TRUE) {
		optionals.set(toInt(OptionalDeviceExtension::DrawIndirectCount));
	} else {
		optionals.reset(toInt(OptionalDeviceExtension::DrawIndirectCount));
	}

	if (device12.storageBuffer8BitAccess == VK_TRUE) {
		optionals.set(toInt(OptionalDeviceExtension::Storage8Bit));
	} else {
		optionals.reset(toInt(OptionalDeviceExtension::Storage8Bit));
	}

	device8bitStorage.storageBuffer8BitAccess = device12.storageBuffer8BitAccess;
	device8bitStorage.uniformAndStorageBuffer8BitAccess =
			device12.uniformAndStorageBuffer8BitAccess;
	device8bitStorage.storagePushConstant8 = device12.storagePushConstant8;

	deviceShaderFloat16Int8.shaderFloat16 = device12.shaderFloat16;
	deviceShaderFloat16Int8.shaderInt8 = device12.shaderInt8;

	if (device12.shaderFloat16 == VK_TRUE && device12.shaderInt8 == VK_TRUE) {
		optionals.set(toInt(OptionalDeviceExtension::ShaderFloat16Int8));
	} else {
		optionals.reset(toInt(OptionalDeviceExtension::ShaderFloat16Int8));
	}

	if (device12.descriptorIndexing == VK_TRUE) {
		optionals.set(toInt(OptionalDeviceExtension::DescriptorIndexing));
	} else {
		optionals.reset(toInt(OptionalDeviceExtension::DescriptorIndexing));
	}

	deviceDescriptorIndexing.shaderInputAttachmentArrayDynamicIndexing =
			device12.shaderInputAttachmentArrayDynamicIndexing;
	deviceDescriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing =
			device12.shaderUniformTexelBufferArrayDynamicIndexing;
	deviceDescriptorIndexing.shaderStorageTexelBufferArrayDynamicIndexing =
			device12.shaderStorageTexelBufferArrayDynamicIndexing;
	deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing =
			device12.shaderUniformBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing =
			device12.shaderSampledImageArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing =
			device12.shaderStorageBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing =
			device12.shaderStorageImageArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderInputAttachmentArrayNonUniformIndexing =
			device12.shaderInputAttachmentArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderUniformTexelBufferArrayNonUniformIndexing =
			device12.shaderUniformTexelBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderStorageTexelBufferArrayNonUniformIndexing =
			device12.shaderStorageTexelBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind =
			device12.descriptorBindingUniformBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind =
			device12.descriptorBindingSampledImageUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind =
			device12.descriptorBindingStorageImageUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind =
			device12.descriptorBindingStorageBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind =
			device12.descriptorBindingUniformTexelBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind =
			device12.descriptorBindingStorageTexelBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending =
			device12.descriptorBindingUpdateUnusedWhilePending;
	deviceDescriptorIndexing.descriptorBindingPartiallyBound =
			device12.descriptorBindingPartiallyBound;
	deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount =
			device12.descriptorBindingVariableDescriptorCount;
	deviceDescriptorIndexing.runtimeDescriptorArray = device12.runtimeDescriptorArray;

	if (device12.bufferDeviceAddress == VK_TRUE) {
		optionals.set(toInt(OptionalDeviceExtension::DeviceAddress));
	} else {
		optionals.reset(toInt(OptionalDeviceExtension::DeviceAddress));
	}

	deviceBufferDeviceAddress.bufferDeviceAddress = device12.bufferDeviceAddress;
	deviceBufferDeviceAddress.bufferDeviceAddressCaptureReplay =
			device12.bufferDeviceAddressCaptureReplay;
	deviceBufferDeviceAddress.bufferDeviceAddressMultiDevice =
			device12.bufferDeviceAddressMultiDevice;
#endif
}

void DeviceInfo::Features::updateTo12(bool updateFlags) {
	if (updateFlags) {
		if (optionals[toInt(OptionalDeviceExtension::Storage16Bit)]) {
			if (device16bitStorage.storageBuffer16BitAccess == VK_TRUE) {
				optionals.set(toInt(OptionalDeviceExtension::Storage16Bit));
			} else {
				optionals.reset(toInt(OptionalDeviceExtension::Storage16Bit));
			}
		}
		if (optionals[toInt(OptionalDeviceExtension::Storage8Bit)]) {
			if (device8bitStorage.storageBuffer8BitAccess == VK_TRUE) {
				optionals.set(toInt(OptionalDeviceExtension::Storage8Bit));
			} else {
				optionals.reset(toInt(OptionalDeviceExtension::Storage8Bit));
			}
		}

		if (optionals[toInt(OptionalDeviceExtension::ShaderFloat16Int8)]) {
			if (deviceShaderFloat16Int8.shaderInt8 == VK_TRUE
					&& deviceShaderFloat16Int8.shaderFloat16 == VK_TRUE) {
				optionals.set(toInt(OptionalDeviceExtension::ShaderFloat16Int8));
			} else {
				optionals.reset(toInt(OptionalDeviceExtension::ShaderFloat16Int8));
			}
		}

		if (optionals[toInt(OptionalDeviceExtension::DeviceAddress)]) {
			if (deviceBufferDeviceAddress.bufferDeviceAddress == VK_TRUE) {
				optionals.set(toInt(OptionalDeviceExtension::DeviceAddress));
			} else {
				optionals.reset(toInt(OptionalDeviceExtension::DeviceAddress));
			}
		}
	}

#if VK_VERSION_1_2
	device11.storageBuffer16BitAccess = device16bitStorage.storageBuffer16BitAccess;
	device11.uniformAndStorageBuffer16BitAccess = device16bitStorage.storageBuffer16BitAccess;
	device11.storagePushConstant16 = device16bitStorage.storageBuffer16BitAccess;
	device11.storageInputOutput16 = device16bitStorage.storageBuffer16BitAccess;

	if (optionals[toInt(OptionalDeviceExtension::DrawIndirectCount)]) {
		device12.drawIndirectCount = VK_TRUE;
	}

	device12.storageBuffer8BitAccess = device8bitStorage.storageBuffer8BitAccess;
	device12.uniformAndStorageBuffer8BitAccess =
			device8bitStorage.uniformAndStorageBuffer8BitAccess;
	device12.storagePushConstant8 = device8bitStorage.storagePushConstant8;

	device12.shaderFloat16 = deviceShaderFloat16Int8.shaderFloat16;
	device12.shaderInt8 = deviceShaderFloat16Int8.shaderInt8;

	if (optionals[toInt(OptionalDeviceExtension::DescriptorIndexing)]) {
		device12.descriptorIndexing = VK_TRUE;
	}

	device12.shaderInputAttachmentArrayDynamicIndexing =
			deviceDescriptorIndexing.shaderInputAttachmentArrayDynamicIndexing;
	device12.shaderUniformTexelBufferArrayDynamicIndexing =
			deviceDescriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing;
	device12.shaderStorageTexelBufferArrayDynamicIndexing =
			deviceDescriptorIndexing.shaderStorageTexelBufferArrayDynamicIndexing;
	device12.shaderUniformBufferArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing;
	device12.shaderSampledImageArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing;
	device12.shaderStorageBufferArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing;
	device12.shaderStorageImageArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing;
	device12.shaderInputAttachmentArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderInputAttachmentArrayNonUniformIndexing;
	device12.shaderUniformTexelBufferArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderUniformTexelBufferArrayNonUniformIndexing;
	device12.shaderStorageTexelBufferArrayNonUniformIndexing =
			deviceDescriptorIndexing.shaderStorageTexelBufferArrayNonUniformIndexing;
	device12.descriptorBindingUniformBufferUpdateAfterBind =
			deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind;
	device12.descriptorBindingSampledImageUpdateAfterBind =
			deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
	device12.descriptorBindingStorageImageUpdateAfterBind =
			deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind;
	device12.descriptorBindingStorageBufferUpdateAfterBind =
			deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind;
	device12.descriptorBindingUniformTexelBufferUpdateAfterBind =
			deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind;
	device12.descriptorBindingStorageTexelBufferUpdateAfterBind =
			deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind;
	device12.descriptorBindingUpdateUnusedWhilePending =
			deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending;
	device12.descriptorBindingPartiallyBound =
			deviceDescriptorIndexing.descriptorBindingPartiallyBound;
	device12.descriptorBindingVariableDescriptorCount =
			deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount;
	device12.runtimeDescriptorArray = deviceDescriptorIndexing.runtimeDescriptorArray;

	device12.bufferDeviceAddress = deviceBufferDeviceAddress.bufferDeviceAddress;
	device12.bufferDeviceAddressCaptureReplay =
			deviceBufferDeviceAddress.bufferDeviceAddressCaptureReplay;
	device12.bufferDeviceAddressMultiDevice =
			deviceBufferDeviceAddress.bufferDeviceAddressMultiDevice;
#endif
}

void DeviceInfo::Features::clear() {
	auto doClear = [](SpanView<VkBool32> src) {
		for (size_t i = 0; i < src.size(); ++i) { const_cast<VkBool32 &>(src[i]) = VK_FALSE; }
	};

	doClear(SpanView<VkBool32>(&device10.features.robustBufferAccess,
			sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)));

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#ifdef VK_ENABLE_BETA_EXTENSIONS
	doClear(SP_VK_BOOL_ARRAY(devicePortability, constantAlphaColorBlendFactors,
			VkPhysicalDevicePortabilitySubsetFeaturesKHR));
#endif

#if VK_VERSION_1_2
	doClear(SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features));
	doClear(SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features));
#endif

#if VK_VERSION_1_3
	doClear(SP_VK_BOOL_ARRAY(device13, robustImageAccess, VkPhysicalDeviceVulkan13Features));
#endif

	doClear(SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess,
			VkPhysicalDevice16BitStorageFeaturesKHR));
	doClear(SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess,
			VkPhysicalDevice8BitStorageFeaturesKHR));
	doClear(SP_VK_BOOL_ARRAY(deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing,
			VkPhysicalDeviceDescriptorIndexingFeaturesEXT));
	doClear(SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress,
			VkPhysicalDeviceBufferDeviceAddressFeaturesKHR));
	doClear(SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16,
			VkPhysicalDeviceShaderFloat16Int8FeaturesKHR));
#undef SP_VK_BOOL_ARRAY
}

DeviceInfo::Features::Features() { clear(); }

DeviceInfo::Features::Features(const Features &f) { memcpy((void *)this, &f, sizeof(Features)); }

DeviceInfo::Features &DeviceInfo::Features::operator=(const Features &f) {
	memcpy((void *)this, &f, sizeof(Features));
	return *this;
}

DeviceInfo::Properties::Properties() { }

DeviceInfo::Properties::Properties(const Properties &p) {
	memcpy((void *)this, &p, sizeof(Properties));
}

DeviceInfo::Properties &DeviceInfo::Properties::operator=(const Properties &p) {
	memcpy((void *)this, &p, sizeof(Properties));
	return *this;
}

DeviceInfo::DeviceInfo() { }

DeviceInfo::DeviceInfo(VkPhysicalDevice dev, QueueFamilyInfo gr, QueueFamilyInfo pres,
		QueueFamilyInfo tr, QueueFamilyInfo comp, Vector<StringView> &&optionals,
		Vector<StringView> &&promoted)
: device(dev)
, graphicsFamily(gr)
, presentFamily(pres)
, transferFamily(tr)
, computeFamily(comp)
, optionalExtensions(sp::move(optionals))
, promotedExtensions(sp::move(promoted)) { }

bool DeviceInfo::supportsPresentation() const {
	// transferFamily and computeFamily can be same as graphicsFamily
	bool supportsGraphics =
			(graphicsFamily.flags & core::QueueFlags::Graphics) != core::QueueFlags::None;
	bool supportsPresent =
			(presentFamily.flags & core::QueueFlags::Present) != core::QueueFlags::None;
	bool supportsTransfer =
			(transferFamily.flags & core::QueueFlags::Transfer) != core::QueueFlags::None;
	bool supportsCompute =
			(computeFamily.flags & core::QueueFlags::Compute) != core::QueueFlags::None;
	if (supportsGraphics && supportsPresent && supportsTransfer && supportsCompute
			&& requiredFeaturesExists && requiredExtensionsExists) {
		return true;
	}
	return false;
}

String DeviceInfo::description() const {
	StringStream stream;
	stream << "\t\t[Queue] ";

	if ((graphicsFamily.flags & core::QueueFlags::Graphics) != core::QueueFlags::None) {
		stream << "Graphics: [" << graphicsFamily.index << "]; ";
	} else {
		stream << "Graphics: [Not available]; ";
	}

	if ((presentFamily.flags & core::QueueFlags::Present) != core::QueueFlags::None) {
		stream << "Presentation: [" << presentFamily.index << "]; ";
	} else {
		stream << "Presentation: [Not available]; ";
	}

	if ((transferFamily.flags & core::QueueFlags::Transfer) != core::QueueFlags::None) {
		stream << "Transfer: [" << transferFamily.index << "]; ";
	} else {
		stream << "Transfer: [Not available]; ";
	}

	if ((computeFamily.flags & core::QueueFlags::Compute) != core::QueueFlags::None) {
		stream << "Compute: [" << computeFamily.index << "];\n";
	} else {
		stream << "Compute: [Not available];\n";
	}

	stream << "\t\t[Limits: Samplers]" " PerSet: "
		   << properties.device10.properties.limits.maxDescriptorSetSamplers << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindSamplers
		   << ");" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageDescriptorSamplers << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindSamplers
		   << ");" "\n";

	stream << "\t\t[Limits: UniformBuffers]" " PerSet: "
		   << properties.device10.properties.limits.maxDescriptorSetUniformBuffers << " dyn: "
		   << properties.device10.properties.limits.maxDescriptorSetUniformBuffersDynamic
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindUniformBuffers
		   << " dyn: "
		   << properties.deviceDescriptorIndexing
					  .maxDescriptorSetUpdateAfterBindUniformBuffersDynamic
		   << ");" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageDescriptorUniformBuffers
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindUniformBuffers
		   << ");"
		   << (properties.deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexingNative
							  ? StringView(" NonUniformIndexingNative;")
							  : StringView())
		   << "\n";

	stream << "\t\t[Limits: StorageBuffers]" " PerSet: "
		   << properties.device10.properties.limits.maxDescriptorSetStorageBuffers << " dyn: "
		   << properties.device10.properties.limits.maxDescriptorSetStorageBuffersDynamic
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindStorageBuffers
		   << " dyn: "
		   << properties.deviceDescriptorIndexing
					  .maxDescriptorSetUpdateAfterBindStorageBuffersDynamic
		   << ");" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageDescriptorStorageBuffers
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindStorageBuffers
		   << ");"
		   << (properties.deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexingNative
							  ? StringView(" NonUniformIndexingNative;")
							  : StringView())
		   << "\n";

	stream << "\t\t[Limits: SampledImages]" " PerSet: "
		   << properties.device10.properties.limits.maxDescriptorSetSampledImages << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindSampledImages
		   << ");" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageDescriptorSampledImages
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindSampledImages
		   << ");"
		   << (properties.deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexingNative
							  ? StringView(" NonUniformIndexingNative;")
							  : StringView())
		   << "\n";

	stream << "\t\t[Limits: StorageImages]" " PerSet: "
		   << properties.device10.properties.limits.maxDescriptorSetStorageImages << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindStorageImages
		   << ");" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageDescriptorStorageImages
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindStorageImages
		   << ");"
		   << (properties.deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexingNative
							  ? StringView(" NonUniformIndexingNative;")
							  : StringView())
		   << "\n";

	stream << "\t\t[Limits: InputAttachments]" " PerSet: "
		   << properties.device10.properties.limits.maxDescriptorSetInputAttachments
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindInputAttachments
		   << ");" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageDescriptorInputAttachments
		   << " (updatable: "
		   << properties.deviceDescriptorIndexing
					  .maxPerStageDescriptorUpdateAfterBindInputAttachments
		   << ");"
		   << (properties.deviceDescriptorIndexing
									  .shaderInputAttachmentArrayNonUniformIndexingNative
							  ? StringView(" NonUniformIndexingNative;")
							  : StringView())
		   << "\n";

	stream << "\t\t[Limits: Resources]" " PerStage: "
		   << properties.device10.properties.limits.maxPerStageResources << " (updatable: "
		   << properties.deviceDescriptorIndexing.maxPerStageUpdateAfterBindResources << ");" "\n";
	stream << "\t\t[Limits: Allocations] "
		   << properties.device10.properties.limits.maxMemoryAllocationCount << " blocks, "
		   << properties.device10.properties.limits.maxSamplerAllocationCount << " samplers;\n";
	stream << "\t\t[Limits: Ranges] Uniform: "
		   << properties.device10.properties.limits.maxUniformBufferRange
		   << ", Storage: " << properties.device10.properties.limits.maxStorageBufferRange << ";\n";
	stream << "\t\t[Limits: DrawIndirectCount] "
		   << properties.device10.properties.limits.maxDrawIndirectCount << ";\n";

	/*uint32_t extensionCount;
	instance->vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	Vector<VkExtensionProperties> availableExtensions(extensionCount);
	instance->vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	stream << "\t[Extensions]\n";
	for (auto &it : availableExtensions) {
		stream << "\t\t" << it.extensionName << ": " << vk::Instance::getVersionDescription(it.specVersion) << "\n";
	}*/

	return stream.str();
}

core::QueueFlags getQueueFlags(VkQueueFlags flags, bool present) {
	core::QueueFlags ret = core::QueueFlags::None;
	if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) {
		ret |= core::QueueFlags::Graphics;
	}
	if ((flags & VK_QUEUE_COMPUTE_BIT) != 0) {
		ret |= core::QueueFlags::Compute;
	}
	if ((flags & VK_QUEUE_TRANSFER_BIT) != 0) {
		ret |= core::QueueFlags::Transfer;
	}
	if ((flags & VK_QUEUE_SPARSE_BINDING_BIT) != 0) {
		ret |= core::QueueFlags::SparceBinding;
	}
	if ((flags & VK_QUEUE_PROTECTED_BIT) != 0) {
		ret |= core::QueueFlags::Protected;
	}
	if ((flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0) {
		ret |= core::QueueFlags::VideoDecode;
	}
#if VK_ENABLE_BETA_EXTENSIONS
	if ((flags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) != 0) {
		ret |= core::QueueFlags::VideoEncode;
	}
#endif

	if (present) {
		ret |= core::QueueFlags::Present;
	}
	return ret;
}

core::QueueFlags getQueueFlags(core::PassType type) {
	switch (type) {
	case core::PassType::Graphics: return core::QueueFlags::Graphics; break;
	case core::PassType::Compute: return core::QueueFlags::Compute; break;
	case core::PassType::Transfer: return core::QueueFlags::Transfer; break;
	case core::PassType::Generic: return core::QueueFlags::None; break;
	}
	return core::QueueFlags::None;
}

VkShaderStageFlagBits getVkStageBits(core::ProgramStage stage) {
	return VkShaderStageFlagBits(stage);
}

StringView getVkFormatName(VkFormat fmt) {
	switch (fmt) {
	case VK_FORMAT_UNDEFINED: return "UNDEFINED"; break;
	case VK_FORMAT_R4G4_UNORM_PACK8: return "R4G4_UNORM_PACK8"; break;
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return "R4G4B4A4_UNORM_PACK16"; break;
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return "B4G4R4A4_UNORM_PACK16"; break;
	case VK_FORMAT_R5G6B5_UNORM_PACK16: return "R5G6B5_UNORM_PACK16"; break;
	case VK_FORMAT_B5G6R5_UNORM_PACK16: return "B5G6R5_UNORM_PACK16"; break;
	case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return "R5G5B5A1_UNORM_PACK16"; break;
	case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return "B5G5R5A1_UNORM_PACK16"; break;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return "A1R5G5B5_UNORM_PACK16"; break;
	case VK_FORMAT_R8_UNORM: return "R8_UNORM"; break;
	case VK_FORMAT_R8_SNORM: return "R8_SNORM"; break;
	case VK_FORMAT_R8_USCALED: return "R8_USCALED"; break;
	case VK_FORMAT_R8_SSCALED: return "R8_SSCALED"; break;
	case VK_FORMAT_R8_UINT: return "R8_UINT"; break;
	case VK_FORMAT_R8_SINT: return "R8_SINT"; break;
	case VK_FORMAT_R8_SRGB: return "R8_SRGB"; break;
	case VK_FORMAT_R8G8_UNORM: return "R8G8_UNORM"; break;
	case VK_FORMAT_R8G8_SNORM: return "R8G8_SNORM"; break;
	case VK_FORMAT_R8G8_USCALED: return "R8G8_USCALED"; break;
	case VK_FORMAT_R8G8_SSCALED: return "R8G8_SSCALED"; break;
	case VK_FORMAT_R8G8_UINT: return "R8G8_UINT"; break;
	case VK_FORMAT_R8G8_SINT: return "R8G8_SINT"; break;
	case VK_FORMAT_R8G8_SRGB: return "R8G8_SRGB"; break;
	case VK_FORMAT_R8G8B8_UNORM: return "R8G8B8_UNORM"; break;
	case VK_FORMAT_R8G8B8_SNORM: return "R8G8B8_SNORM"; break;
	case VK_FORMAT_R8G8B8_USCALED: return "R8G8B8_USCALED"; break;
	case VK_FORMAT_R8G8B8_SSCALED: return "R8G8B8_SSCALED"; break;
	case VK_FORMAT_R8G8B8_UINT: return "R8G8B8_UINT"; break;
	case VK_FORMAT_R8G8B8_SINT: return "R8G8B8_SINT"; break;
	case VK_FORMAT_R8G8B8_SRGB: return "R8G8B8_SRGB"; break;
	case VK_FORMAT_B8G8R8_UNORM: return "B8G8R8_UNORM"; break;
	case VK_FORMAT_B8G8R8_SNORM: return "B8G8R8_SNORM"; break;
	case VK_FORMAT_B8G8R8_USCALED: return "B8G8R8_USCALED"; break;
	case VK_FORMAT_B8G8R8_SSCALED: return "B8G8R8_SSCALED"; break;
	case VK_FORMAT_B8G8R8_UINT: return "B8G8R8_UINT"; break;
	case VK_FORMAT_B8G8R8_SINT: return "B8G8R8_SINT"; break;
	case VK_FORMAT_B8G8R8_SRGB: return "B8G8R8_SRGB"; break;
	case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM"; break;
	case VK_FORMAT_R8G8B8A8_SNORM: return "R8G8B8A8_SNORM"; break;
	case VK_FORMAT_R8G8B8A8_USCALED: return "R8G8B8A8_USCALED"; break;
	case VK_FORMAT_R8G8B8A8_SSCALED: return "R8G8B8A8_SSCALED"; break;
	case VK_FORMAT_R8G8B8A8_UINT: return "R8G8B8A8_UINT"; break;
	case VK_FORMAT_R8G8B8A8_SINT: return "R8G8B8A8_SINT"; break;
	case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB"; break;
	case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM"; break;
	case VK_FORMAT_B8G8R8A8_SNORM: return "B8G8R8A8_SNORM"; break;
	case VK_FORMAT_B8G8R8A8_USCALED: return "B8G8R8A8_USCALED"; break;
	case VK_FORMAT_B8G8R8A8_SSCALED: return "B8G8R8A8_SSCALED"; break;
	case VK_FORMAT_B8G8R8A8_UINT: return "B8G8R8A8_UINT"; break;
	case VK_FORMAT_B8G8R8A8_SINT: return "B8G8R8A8_SINT"; break;
	case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB"; break;
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "A8B8G8R8_UNORM_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "A8B8G8R8_SNORM_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return "A8B8G8R8_USCALED_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return "A8B8G8R8_SSCALED_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "A8B8G8R8_UINT_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "A8B8G8R8_SINT_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "A8B8G8R8_SRGB_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "A2R10G10B10_UNORM_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return "A2R10G10B10_SNORM_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return "A2R10G10B10_USCALED_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return "A2R10G10B10_SSCALED_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_UINT_PACK32: return "A2R10G10B10_UINT_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_SINT_PACK32: return "A2R10G10B10_SINT_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "A2B10G10R10_UNORM_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return "A2B10G10R10_SNORM_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return "A2B10G10R10_USCALED_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return "A2B10G10R10_SSCALED_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_UINT_PACK32: return "A2B10G10R10_UINT_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_SINT_PACK32: return "A2B10G10R10_SINT_PACK32"; break;
	case VK_FORMAT_R16_UNORM: return "R16_UNORM"; break;
	case VK_FORMAT_R16_SNORM: return "R16_SNORM"; break;
	case VK_FORMAT_R16_USCALED: return "R16_USCALED"; break;
	case VK_FORMAT_R16_SSCALED: return "R16_SSCALED"; break;
	case VK_FORMAT_R16_UINT: return "R16_UINT"; break;
	case VK_FORMAT_R16_SINT: return "R16_SINT"; break;
	case VK_FORMAT_R16_SFLOAT: return "R16_SFLOAT"; break;
	case VK_FORMAT_R16G16_UNORM: return "R16G16_UNORM"; break;
	case VK_FORMAT_R16G16_SNORM: return "R16G16_SNORM"; break;
	case VK_FORMAT_R16G16_USCALED: return "R16G16_USCALED"; break;
	case VK_FORMAT_R16G16_SSCALED: return "R16G16_SSCALED"; break;
	case VK_FORMAT_R16G16_UINT: return "R16G16_UINT"; break;
	case VK_FORMAT_R16G16_SINT: return "R16G16_SINT"; break;
	case VK_FORMAT_R16G16_SFLOAT: return "R16G16_SFLOAT"; break;
	case VK_FORMAT_R16G16B16_UNORM: return "R16G16B16_UNORM"; break;
	case VK_FORMAT_R16G16B16_SNORM: return "R16G16B16_SNORM"; break;
	case VK_FORMAT_R16G16B16_USCALED: return "R16G16B16_USCALED"; break;
	case VK_FORMAT_R16G16B16_SSCALED: return "R16G16B16_SSCALED"; break;
	case VK_FORMAT_R16G16B16_UINT: return "R16G16B16_UINT"; break;
	case VK_FORMAT_R16G16B16_SINT: return "R16G16B16_SINT"; break;
	case VK_FORMAT_R16G16B16_SFLOAT: return "R16G16B16_SFLOAT"; break;
	case VK_FORMAT_R16G16B16A16_UNORM: return "R16G16B16A16_UNORM"; break;
	case VK_FORMAT_R16G16B16A16_SNORM: return "R16G16B16A16_SNORM"; break;
	case VK_FORMAT_R16G16B16A16_USCALED: return "R16G16B16A16_USCALED"; break;
	case VK_FORMAT_R16G16B16A16_SSCALED: return "R16G16B16A16_SSCALED"; break;
	case VK_FORMAT_R16G16B16A16_UINT: return "R16G16B16A16_UINT"; break;
	case VK_FORMAT_R16G16B16A16_SINT: return "R16G16B16A16_SINT"; break;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT"; break;
	case VK_FORMAT_R32_UINT: return "R32_UINT"; break;
	case VK_FORMAT_R32_SINT: return "R32_SINT"; break;
	case VK_FORMAT_R32_SFLOAT: return "R32_SFLOAT"; break;
	case VK_FORMAT_R32G32_UINT: return "R32G32_UINT"; break;
	case VK_FORMAT_R32G32_SINT: return "R32G32_SINT"; break;
	case VK_FORMAT_R32G32_SFLOAT: return "R32G32_SFLOAT"; break;
	case VK_FORMAT_R32G32B32_UINT: return "R32G32B32_UINT"; break;
	case VK_FORMAT_R32G32B32_SINT: return "R32G32B32_SINT"; break;
	case VK_FORMAT_R32G32B32_SFLOAT: return "R32G32B32_SFLOAT"; break;
	case VK_FORMAT_R32G32B32A32_UINT: return "R32G32B32A32_UINT"; break;
	case VK_FORMAT_R32G32B32A32_SINT: return "R32G32B32A32_SINT"; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return "R32G32B32A32_SFLOAT"; break;
	case VK_FORMAT_R64_UINT: return "R64_UINT"; break;
	case VK_FORMAT_R64_SINT: return "R64_SINT"; break;
	case VK_FORMAT_R64_SFLOAT: return "R64_SFLOAT"; break;
	case VK_FORMAT_R64G64_UINT: return "R64G64_UINT"; break;
	case VK_FORMAT_R64G64_SINT: return "R64G64_SINT"; break;
	case VK_FORMAT_R64G64_SFLOAT: return "R64G64_SFLOAT"; break;
	case VK_FORMAT_R64G64B64_UINT: return "R64G64B64_UINT"; break;
	case VK_FORMAT_R64G64B64_SINT: return "R64G64B64_SINT"; break;
	case VK_FORMAT_R64G64B64_SFLOAT: return "R64G64B64_SFLOAT"; break;
	case VK_FORMAT_R64G64B64A64_UINT: return "R64G64B64A64_UINT"; break;
	case VK_FORMAT_R64G64B64A64_SINT: return "R64G64B64A64_SINT"; break;
	case VK_FORMAT_R64G64B64A64_SFLOAT: return "R64G64B64A64_SFLOAT"; break;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "B10G11R11_UFLOAT_PACK32"; break;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return "E5B9G9R9_UFLOAT_PACK32"; break;
	case VK_FORMAT_D16_UNORM: return "D16_UNORM"; break;
	case VK_FORMAT_X8_D24_UNORM_PACK32: return "X8_D24_UNORM_PACK32"; break;
	case VK_FORMAT_D32_SFLOAT: return "D32_SFLOAT"; break;
	case VK_FORMAT_S8_UINT: return "S8_UINT"; break;
	case VK_FORMAT_D16_UNORM_S8_UINT: return "D16_UNORM_S8_UINT"; break;
	case VK_FORMAT_D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT"; break;
	case VK_FORMAT_D32_SFLOAT_S8_UINT: return "D32_SFLOAT_S8_UINT"; break;
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return "BC1_RGB_UNORM_BLOCK"; break;
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return "BC1_RGB_SRGB_BLOCK"; break;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return "BC1_RGBA_UNORM_BLOCK"; break;
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return "BC1_RGBA_SRGB_BLOCK"; break;
	case VK_FORMAT_BC2_UNORM_BLOCK: return "BC2_UNORM_BLOCK"; break;
	case VK_FORMAT_BC2_SRGB_BLOCK: return "BC2_SRGB_BLOCK"; break;
	case VK_FORMAT_BC3_UNORM_BLOCK: return "BC3_UNORM_BLOCK"; break;
	case VK_FORMAT_BC3_SRGB_BLOCK: return "BC3_SRGB_BLOCK"; break;
	case VK_FORMAT_BC4_UNORM_BLOCK: return "BC4_UNORM_BLOCK"; break;
	case VK_FORMAT_BC4_SNORM_BLOCK: return "BC4_SNORM_BLOCK"; break;
	case VK_FORMAT_BC5_UNORM_BLOCK: return "BC5_UNORM_BLOCK"; break;
	case VK_FORMAT_BC5_SNORM_BLOCK: return "BC5_SNORM_BLOCK"; break;
	case VK_FORMAT_BC6H_UFLOAT_BLOCK: return "BC6H_UFLOAT_BLOCK"; break;
	case VK_FORMAT_BC6H_SFLOAT_BLOCK: return "BC6H_SFLOAT_BLOCK"; break;
	case VK_FORMAT_BC7_UNORM_BLOCK: return "BC7_UNORM_BLOCK"; break;
	case VK_FORMAT_BC7_SRGB_BLOCK: return "BC7_SRGB_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return "ETC2_R8G8B8_UNORM_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return "ETC2_R8G8B8_SRGB_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return "ETC2_R8G8B8A1_UNORM_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return "ETC2_R8G8B8A1_SRGB_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return "ETC2_R8G8B8A8_UNORM_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return "ETC2_R8G8B8A8_SRGB_BLOCK"; break;
	case VK_FORMAT_EAC_R11_UNORM_BLOCK: return "EAC_R11_UNORM_BLOCK"; break;
	case VK_FORMAT_EAC_R11_SNORM_BLOCK: return "EAC_R11_SNORM_BLOCK"; break;
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return "EAC_R11G11_UNORM_BLOCK"; break;
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return "EAC_R11G11_SNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return "ASTC_4x4_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return "ASTC_4x4_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return "ASTC_5x4_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return "ASTC_5x4_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return "ASTC_5x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return "ASTC_5x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return "ASTC_6x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return "ASTC_6x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return "ASTC_6x6_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return "ASTC_6x6_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return "ASTC_8x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return "ASTC_8x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return "ASTC_8x6_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return "ASTC_8x6_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return "ASTC_8x8_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return "ASTC_8x8_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return "ASTC_10x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return "ASTC_10x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return "ASTC_10x6_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return "ASTC_10x6_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return "ASTC_10x8_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return "ASTC_10x8_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return "ASTC_10x10_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return "ASTC_10x10_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return "ASTC_12x10_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return "ASTC_12x10_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return "ASTC_12x12_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return "ASTC_12x12_SRGB_BLOCK"; break;
	case VK_FORMAT_G8B8G8R8_422_UNORM: return "G8B8G8R8_422_UNORM"; break;
	case VK_FORMAT_B8G8R8G8_422_UNORM: return "B8G8R8G8_422_UNORM"; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return "G8_B8_R8_3PLANE_420_UNORM"; break;
	case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return "G8_B8R8_2PLANE_420_UNORM"; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return "G8_B8_R8_3PLANE_422_UNORM"; break;
	case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return "G8_B8R8_2PLANE_422_UNORM"; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return "G8_B8_R8_3PLANE_444_UNORM"; break;
	case VK_FORMAT_R10X6_UNORM_PACK16: return "R10X6_UNORM_PACK16"; break;
	case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return "R10X6G10X6_UNORM_2PACK16"; break;
	case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		return "R10X6G10X6B10X6A10X6_UNORM_4PACK16";
		break;
	case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		return "G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
		break;
	case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		return "B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
		break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		return "G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
		break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		return "G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
		break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		return "G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
		break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		return "G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
		break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		return "G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
		break;
	case VK_FORMAT_R12X4_UNORM_PACK16: return "R12X4_UNORM_PACK16"; break;
	case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return "R12X4G12X4_UNORM_2PACK16"; break;
	case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		return "R12X4G12X4B12X4A12X4_UNORM_4PACK16";
		break;
	case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		return "G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
		break;
	case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		return "B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
		break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		return "G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
		break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		return "G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
		break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		return "G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
		break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		return "G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
		break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		return "G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
		break;
	case VK_FORMAT_G16B16G16R16_422_UNORM: return "G16B16G16R16_422_UNORM"; break;
	case VK_FORMAT_B16G16R16G16_422_UNORM: return "B16G16R16G16_422_UNORM"; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return "G16_B16_R16_3PLANE_420_UNORM"; break;
	case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return "G16_B16R16_2PLANE_420_UNORM"; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return "G16_B16_R16_3PLANE_422_UNORM"; break;
	case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return "G16_B16R16_2PLANE_422_UNORM"; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return "G16_B16_R16_3PLANE_444_UNORM"; break;
	case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return "PVRTC1_2BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return "PVRTC1_4BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return "PVRTC2_2BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return "PVRTC2_4BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return "PVRTC1_2BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return "PVRTC1_4BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return "PVRTC2_2BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return "PVRTC2_4BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT: return "ASTC_4x4_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT: return "ASTC_5x4_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT: return "ASTC_5x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT: return "ASTC_6x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT: return "ASTC_6x6_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT: return "ASTC_8x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT: return "ASTC_8x6_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT: return "ASTC_8x8_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT: return "ASTC_10x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT: return "ASTC_10x6_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT: return "ASTC_10x8_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT: return "ASTC_10x10_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT: return "ASTC_12x10_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT: return "ASTC_12x12_SFLOAT_BLOCK_EXT"; break;
	default: return "UNDEFINED"; break;
	}
	return "UNDEFINED";
}

StringView getVkColorSpaceName(VkColorSpaceKHR fmt) {
	switch (fmt) {
	case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "SRGB_NONLINEAR"; break;
	case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "DISPLAY_P3_NONLINEAR"; break;
	case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "EXTENDED_SRGB_LINEAR"; break;
	case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: return "DISPLAY_P3_LINEAR"; break;
	case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: return "DCI_P3_NONLINEAR"; break;
	case VK_COLOR_SPACE_BT709_LINEAR_EXT: return "BT709_LINEAR"; break;
	case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: return "BT709_NONLINEAR"; break;
	case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "BT2020_LINEAR"; break;
	case VK_COLOR_SPACE_HDR10_ST2084_EXT: return "HDR10_ST2084"; break;
	case VK_COLOR_SPACE_DOLBYVISION_EXT: return "DOLBYVISION"; break;
	case VK_COLOR_SPACE_HDR10_HLG_EXT: return "HDR10_HLG"; break;
	case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: return "ADOBERGB_LINEAR"; break;
	case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: return "ADOBERGB_NONLINEAR"; break;
	case VK_COLOR_SPACE_PASS_THROUGH_EXT: return "PASS_THROUGH"; break;
	case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: return "EXTENDED_SRGB_NONLINEAR"; break;
	case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD: return "DISPLAY_NATIVE"; break;
	default: return "UNKNOWN"; break;
	}
	return "UNKNOWN";
}

StringView getVkResultName(VkResult res) {
	switch (res) {
	case VK_SUCCESS: return "VK_SUCCESS"; break;
	case VK_NOT_READY: return "VK_NOT_READY"; break;
	case VK_TIMEOUT: return "VK_TIMEOUT"; break;
	case VK_EVENT_SET: return "VK_EVENT_SET"; break;
	case VK_EVENT_RESET: return "VK_EVENT_RESET"; break;
	case VK_INCOMPLETE: return "VK_INCOMPLETE"; break;
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED"; break;
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST"; break;
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED"; break;
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT"; break;
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT"; break;
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS"; break;
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL"; break;
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN"; break;
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
	case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION"; break;
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
		return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		break;
#if VK_VERSION_1_3
	case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED"; break;
#endif
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR"; break;
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR"; break;
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR"; break;
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
	case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT"; break;
	case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV"; break;
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
		break;
#if VK_VERSION_1_3
	case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR"; break;
#endif
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
		return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		break;
	case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR"; break;
	case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR"; break;
	case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR"; break;
	case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR"; break;
	case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM"; break;
	default: break;
	}
	return "UNKNOWN";
}

String getVkMemoryPropertyFlags(VkMemoryPropertyFlags flags) {
	StringStream ret;
	if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
		ret << " DEVICE_LOCAL";
	}
	if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		ret << " HOST_VISIBLE";
	}
	if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
		ret << " HOST_COHERENT";
	}
	if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
		ret << " HOST_CACHED";
	}
	if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
		ret << " LAZILY_ALLOCATED";
	}
	if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT) {
		ret << " PROTECTED";
	}
	if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) {
		ret << " DEVICE_COHERENT_AMD";
	}
	if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) {
		ret << " DEVICE_UNCACHED_AMD";
	}
	return ret.str();
}

static uint32_t getIndexForExtension(const char *name) {
	uint32_t ret = 0;
	for (auto &it : s_optionalDeviceExtensions) {
		if (it && strcmp(name, it) == 0) {
			return ret;
		}
		++ret;
	}
	return maxOf<uint32_t>();
}

bool checkIfExtensionAvailable(uint32_t apiVersion, const char *name,
		const Vector<VkExtensionProperties> &available, Vector<StringView> &optionals,
		Vector<StringView> &promoted, std::bitset<toInt(OptionalDeviceExtension::Max)> &flags) {
	auto index = getIndexForExtension(name);
	if (index == maxOf<uint32_t>()) {
		log::source().error("Vk", "Extension is not registered as optional: ", name);
		return false;
	}

#if VK_VERSION_1_4
	if (apiVersion >= VK_API_VERSION_1_4) {
		for (auto &it : s_promotedVk14Extensions) {
			if (it && strcmp(name, it) == 0) {
				flags.set(index);
				promoted.emplace_back(StringView(name));
				return true;
			}
		}
	}
#endif
#if VK_VERSION_1_3
	if (apiVersion >= VK_API_VERSION_1_3) {
		for (auto &it : s_promotedVk13Extensions) {
			if (it && strcmp(name, it) == 0) {
				flags.set(index);
				promoted.emplace_back(StringView(name));
				return true;
			}
		}
	}
#endif
	if (apiVersion >= VK_API_VERSION_1_2) {
		for (auto &it : s_promotedVk12Extensions) {
			if (it && strcmp(name, it) == 0) {
				flags.set(index);
				promoted.emplace_back(StringView(name));
				return true;
			}
		}
	}
	if (apiVersion >= VK_API_VERSION_1_1) {
		for (auto &it : s_promotedVk11Extensions) {
			if (it && strcmp(name, it) == 0) {
				flags.set(index);
				promoted.emplace_back(StringView(name));
				return true;
			}
		}
	}

	for (auto &it : available) {
		if (strcmp(name, it.extensionName) == 0) {
			flags.set(index);
			optionals.emplace_back(StringView(name));
			return true;
		}
	}
	return false;
}

bool isPromotedExtension(uint32_t apiVersion, StringView name) {
	if (apiVersion >= VK_API_VERSION_1_2) {
		for (auto &it : s_promotedVk12Extensions) {
			if (it) {
				if (strcmp(name.data(), it) == 0) {
					return true;
				}
			}
		}
	}
	if (apiVersion >= VK_API_VERSION_1_1) {
		for (auto &it : s_promotedVk11Extensions) {
			if (it) {
				if (strcmp(name.data(), it) == 0) {
					return true;
				}
			}
		}
	}
	return false;
}

size_t getFormatBlockSize(VkFormat format) {
	switch (format) {
	case VK_FORMAT_UNDEFINED: return 0; break;
	case VK_FORMAT_MAX_ENUM: return 0; break;
	case VK_FORMAT_R4G4_UNORM_PACK8: return 1; break;
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return 2; break;
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R5G6B5_UNORM_PACK16: return 2; break;
	case VK_FORMAT_B5G6R5_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return 2; break;
	case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return 2; break;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R8_UNORM: return 1; break;
	case VK_FORMAT_R8_SNORM: return 1; break;
	case VK_FORMAT_R8_USCALED: return 1; break;
	case VK_FORMAT_R8_SSCALED: return 1; break;
	case VK_FORMAT_R8_UINT: return 1; break;
	case VK_FORMAT_R8_SINT: return 1; break;
	case VK_FORMAT_R8_SRGB: return 1; break;
	case VK_FORMAT_R8G8_UNORM: return 2; break;
	case VK_FORMAT_R8G8_SNORM: return 2; break;
	case VK_FORMAT_R8G8_USCALED: return 2; break;
	case VK_FORMAT_R8G8_SSCALED: return 2; break;
	case VK_FORMAT_R8G8_UINT: return 2; break;
	case VK_FORMAT_R8G8_SINT: return 2; break;
	case VK_FORMAT_R8G8_SRGB: return 2; break;
	case VK_FORMAT_R8G8B8_UNORM: return 3; break;
	case VK_FORMAT_R8G8B8_SNORM: return 3; break;
	case VK_FORMAT_R8G8B8_USCALED: return 3; break;
	case VK_FORMAT_R8G8B8_SSCALED: return 3; break;
	case VK_FORMAT_R8G8B8_UINT: return 3; break;
	case VK_FORMAT_R8G8B8_SINT: return 3; break;
	case VK_FORMAT_R8G8B8_SRGB: return 3; break;
	case VK_FORMAT_B8G8R8_UNORM: return 3; break;
	case VK_FORMAT_B8G8R8_SNORM: return 3; break;
	case VK_FORMAT_B8G8R8_USCALED: return 3; break;
	case VK_FORMAT_B8G8R8_SSCALED: return 3; break;
	case VK_FORMAT_B8G8R8_UINT: return 3; break;
	case VK_FORMAT_B8G8R8_SINT: return 3; break;
	case VK_FORMAT_B8G8R8_SRGB: return 3; break;
	case VK_FORMAT_R8G8B8A8_UNORM: return 4; break;
	case VK_FORMAT_R8G8B8A8_SNORM: return 4; break;
	case VK_FORMAT_R8G8B8A8_USCALED: return 4; break;
	case VK_FORMAT_R8G8B8A8_SSCALED: return 4; break;
	case VK_FORMAT_R8G8B8A8_UINT: return 4; break;
	case VK_FORMAT_R8G8B8A8_SINT: return 4; break;
	case VK_FORMAT_R8G8B8A8_SRGB: return 4; break;
	case VK_FORMAT_B8G8R8A8_UNORM: return 4; break;
	case VK_FORMAT_B8G8R8A8_SNORM: return 4; break;
	case VK_FORMAT_B8G8R8A8_USCALED: return 4; break;
	case VK_FORMAT_B8G8R8A8_SSCALED: return 4; break;
	case VK_FORMAT_B8G8R8A8_UINT: return 4; break;
	case VK_FORMAT_B8G8R8A8_SINT: return 4; break;
	case VK_FORMAT_B8G8R8A8_SRGB: return 4; break;
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_UINT_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SINT_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_UINT_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_SINT_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_UINT_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_SINT_PACK32: return 4; break;
	case VK_FORMAT_R16_UNORM: return 2; break;
	case VK_FORMAT_R16_SNORM: return 2; break;
	case VK_FORMAT_R16_USCALED: return 2; break;
	case VK_FORMAT_R16_SSCALED: return 2; break;
	case VK_FORMAT_R16_UINT: return 2; break;
	case VK_FORMAT_R16_SINT: return 2; break;
	case VK_FORMAT_R16_SFLOAT: return 2; break;
	case VK_FORMAT_R16G16_UNORM: return 4; break;
	case VK_FORMAT_R16G16_SNORM: return 4; break;
	case VK_FORMAT_R16G16_USCALED: return 4; break;
	case VK_FORMAT_R16G16_SSCALED: return 4; break;
	case VK_FORMAT_R16G16_UINT: return 4; break;
	case VK_FORMAT_R16G16_SINT: return 4; break;
	case VK_FORMAT_R16G16_SFLOAT: return 4; break;
	case VK_FORMAT_R16G16B16_UNORM: return 6; break;
	case VK_FORMAT_R16G16B16_SNORM: return 6; break;
	case VK_FORMAT_R16G16B16_USCALED: return 6; break;
	case VK_FORMAT_R16G16B16_SSCALED: return 6; break;
	case VK_FORMAT_R16G16B16_UINT: return 6; break;
	case VK_FORMAT_R16G16B16_SINT: return 6; break;
	case VK_FORMAT_R16G16B16_SFLOAT: return 6; break;
	case VK_FORMAT_R16G16B16A16_UNORM: return 8; break;
	case VK_FORMAT_R16G16B16A16_SNORM: return 8; break;
	case VK_FORMAT_R16G16B16A16_USCALED: return 8; break;
	case VK_FORMAT_R16G16B16A16_SSCALED: return 8; break;
	case VK_FORMAT_R16G16B16A16_UINT: return 8; break;
	case VK_FORMAT_R16G16B16A16_SINT: return 8; break;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return 8; break;
	case VK_FORMAT_R32_UINT: return 4; break;
	case VK_FORMAT_R32_SINT: return 4; break;
	case VK_FORMAT_R32_SFLOAT: return 4; break;
	case VK_FORMAT_R32G32_UINT: return 8; break;
	case VK_FORMAT_R32G32_SINT: return 8; break;
	case VK_FORMAT_R32G32_SFLOAT: return 8; break;
	case VK_FORMAT_R32G32B32_UINT: return 12; break;
	case VK_FORMAT_R32G32B32_SINT: return 12; break;
	case VK_FORMAT_R32G32B32_SFLOAT: return 12; break;
	case VK_FORMAT_R32G32B32A32_UINT: return 16; break;
	case VK_FORMAT_R32G32B32A32_SINT: return 16; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return 16; break;
	case VK_FORMAT_R64_UINT: return 8; break;
	case VK_FORMAT_R64_SINT: return 8; break;
	case VK_FORMAT_R64_SFLOAT: return 8; break;
	case VK_FORMAT_R64G64_UINT: return 16; break;
	case VK_FORMAT_R64G64_SINT: return 16; break;
	case VK_FORMAT_R64G64_SFLOAT: return 16; break;
	case VK_FORMAT_R64G64B64_UINT: return 24; break;
	case VK_FORMAT_R64G64B64_SINT: return 24; break;
	case VK_FORMAT_R64G64B64_SFLOAT: return 24; break;
	case VK_FORMAT_R64G64B64A64_UINT: return 32; break;
	case VK_FORMAT_R64G64B64A64_SINT: return 32; break;
	case VK_FORMAT_R64G64B64A64_SFLOAT: return 32; break;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return 4; break;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return 4; break;
	case VK_FORMAT_D16_UNORM: return 2; break;
	case VK_FORMAT_X8_D24_UNORM_PACK32: return 4; break;
	case VK_FORMAT_D32_SFLOAT: return 4; break;
	case VK_FORMAT_S8_UINT: return 1; break;
	case VK_FORMAT_D16_UNORM_S8_UINT: return 3; break;
	case VK_FORMAT_D24_UNORM_S8_UINT: return 4; break;
	case VK_FORMAT_D32_SFLOAT_S8_UINT: return 5; break;
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_BC2_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC2_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_BC3_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC3_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_BC4_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC4_SNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC5_SNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC6H_UFLOAT_BLOCK: return 16; break;
	case VK_FORMAT_BC6H_SFLOAT_BLOCK: return 16; break;
	case VK_FORMAT_BC7_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC7_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_EAC_R11_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_EAC_R11_SNORM_BLOCK: return 8; break;
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_G8B8G8R8_422_UNORM: return 4; break;
	case VK_FORMAT_B8G8R8G8_422_UNORM: return 4; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return 3; break;
	case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return 3; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return 3; break;
	case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return 3; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return 3; break;
	case VK_FORMAT_R10X6_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return 4; break;
	case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return 4; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_R12X4_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return 4; break;
	case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G16B16G16R16_422_UNORM: return 8; break;
	case VK_FORMAT_B16G16R16G16_422_UNORM: return 8; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return 6; break;
	case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return 6; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return 6; break;
	case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return 6; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return 6; break;
	case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT: return 3; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT: return 6; break;
	case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT: return 2; break;
	case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT: return 2; break;
	default: break;
	}
	return 0;
}

VkPresentModeKHR getVkPresentMode(core::PresentMode presentMode) {
	switch (presentMode) {
	case core::PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR; break;
	case core::PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR; break;
	case core::PresentMode::Fifo: return VK_PRESENT_MODE_FIFO_KHR; break;
	case core::PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR; break;
	default: break;
	}
	return VkPresentModeKHR(0);
}

Status getStatus(VkResult res) {
	switch (res) {
	case VK_SUCCESS: return Status::Ok; break;
	case VK_NOT_READY: return Status::Declined; break;
	case VK_TIMEOUT: return Status::Timeout; break;
	case VK_EVENT_SET: return Status::EventSet; break;
	case VK_EVENT_RESET: return Status::EventReset; break;
	case VK_INCOMPLETE: return Status::Incomplete; break;
	case VK_ERROR_OUT_OF_HOST_MEMORY: return Status::ErrorOutOfHostMemory; break;
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return Status::ErrorOutOfDeviceMemory; break;
	case VK_ERROR_INITIALIZATION_FAILED: return Status::ErrorInvalidArguemnt; break;
	case VK_ERROR_DEVICE_LOST: return Status::ErrorDeviceLost; break;
	case VK_ERROR_MEMORY_MAP_FAILED: return Status::ErrorMemoryMapFailed; break;
	case VK_ERROR_LAYER_NOT_PRESENT: return Status::ErrorLayerNotPresent; break;
	case VK_ERROR_EXTENSION_NOT_PRESENT: return Status::ErrorExtensionNotPresent; break;
	case VK_ERROR_FEATURE_NOT_PRESENT: return Status::ErrorFeatureNotPresent; break;
	case VK_ERROR_INCOMPATIBLE_DRIVER: return Status::ErrorIncompatibleDevice; break;
	case VK_ERROR_TOO_MANY_OBJECTS: return Status::ErrorTooManyObjects; break;
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return Status::ErrorNotSupported; break;
	case VK_ERROR_FRAGMENTED_POOL: return Status::ErrorFragmentedPool; break;
	case VK_ERROR_UNKNOWN: return Status::ErrorUnknown; break;
	case VK_ERROR_OUT_OF_POOL_MEMORY: return Status::ErrorOutOfPoolMemory; break;
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return Status::ErrorInvalidExternalHandle; break;
	case VK_ERROR_FRAGMENTATION: return Status::ErrorFragmentation; break;
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return Status::ErrorInvalidCaptureAddress; break;
#if VK_VERSION_1_3
	case VK_PIPELINE_COMPILE_REQUIRED: return Status::ErrorPipelineCompileRequired; break;
#endif
	case VK_ERROR_SURFACE_LOST_KHR: return Status::ErrorSurfaceLost; break;
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return Status::ErrorNativeWindowInUse; break;
	case VK_SUBOPTIMAL_KHR: return Status::Suboptimal; break;
	case VK_ERROR_OUT_OF_DATE_KHR: return Status::ErrorCancelled; break;
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return Status::ErrorIncompatibleDisplay; break;
	case VK_ERROR_VALIDATION_FAILED_EXT: return Status::ErrorValidationFailed; break;
	case VK_ERROR_INVALID_SHADER_NV: return Status::ErrorInvalidShader; break;
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return Status::ErrorInvalidDrmFormat;
		break;
#if VK_VERSION_1_3
	case VK_ERROR_NOT_PERMITTED_KHR: return Status::ErrorNotPermitted; break;
#endif
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return Status::ErrorFullscreenLost; break;
	case VK_THREAD_IDLE_KHR: return Status::ThreadIdle; break;
	case VK_THREAD_DONE_KHR:
		return Status::ThreadDone;
		;
		break;
	case VK_OPERATION_DEFERRED_KHR: return Status::OperationDeferred; break;
	case VK_OPERATION_NOT_DEFERRED_KHR: return Status::OperationNotDeferred; break;
	default: break;
	}
	return Status::ErrorUnknown;
}

std::ostream &operator<<(std::ostream &stream, VkResult res) {
	stream << STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::getVkResultName(res);
	return stream;
}

} // namespace stappler::xenolith::vk
