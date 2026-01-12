/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

// clang-format off

#ifndef XENOLITH_BACKEND_VK_XLVK_H_
#define XENOLITH_BACKEND_VK_XLVK_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLCoreEnum.h"
#include "XLCoreDescriptorInfo.h"
#include "XLVkHeader.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

#if LINUX
//#define VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR 0x00000400
//#define VK_BUFFER_USAGE_RAY_TRACING_BIT_NV VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR
#endif

#ifndef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
#define VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME "VK_EXT_full_screen_exclusive"
#endif

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

#define XL_VK_MIN_LOADER_MESSAGE_SEVERITY VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
#define XL_VK_MIN_MESSAGE_SEVERITY VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT

#if DEBUG
#define XL_VKAPI_DEBUG 0
#define VK_HOOK_DEBUG 0 // enable engine hooks for Vulkan calls
static constexpr bool s_printVkInfo = true;
#else
#define VK_HOOK_DEBUG 0 // enable engine hooks for Vulkan calls
static constexpr bool s_printVkInfo = true;
#endif

SPUNUSED static  const char * const s_validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};

}

// Debug tools

// Для кадров с ошибками ожидать освобождения устройства (vkDeviceWaitIdle) при завершении
#define XL_VK_FINALIZE_INVALID_FRAMES 0

#include "XLVkConfig.h" // IWYU pragma: keep
#include "XLVkTable.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

using core::BufferInfo;
using core::ImageInfo;
using core::ImageInfoData;
using core::ImageViewInfo;
using core::SamplerInfo;
using core::DescriptorData;

class Instance;

using sprt::window::SurfaceBackend;
using sprt::window::SurfaceBackendMask;

static const char * const s_requiredExtension[] = {
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#if MACOS
	VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
	nullptr
};

enum class OptionalInstanceExtension {
	Display,
	SurfaceCapabilities2,
	SurfaceMaintenance1,
	Max
};

static const char * const s_optionalExtension[] = {
	VK_KHR_DISPLAY_EXTENSION_NAME,
	VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
	VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
	nullptr
};

static const char * const s_requiredDeviceExtensions[] = {
	VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
#if MACOS
	VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
	nullptr
};

enum class OptionalDeviceExtension {
	Maintenance3,
	DescriptorIndexing,
	DrawIndirectCount,
	Storage16Bit,
	Storage8Bit,
	ShaderFloat16Int8,
	DeviceAddress,
	MemoryBudget,
	GetMemoryRequirements2,
	DedicatedAllocation,
	ExternalFenceFd,
	SwapchainMaintenance1,
	FullscreenExclusive,
	DisplayTiming,
	Portability,
	Max
};

static const char * const s_optionalDeviceExtensions[] = {
	// Descriptor indexing
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,

	// DrawInderectCount
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,

	// 16-bit, 8-bit shader storage
	VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,

	// BufferDeviceAddress
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
	VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
	VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
	VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
	VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
	nullptr
};

static const char * const s_promotedVk11Extensions[] = {
	VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
	VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
	VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	VK_KHR_MAINTENANCE1_EXTENSION_NAME,
	VK_KHR_MAINTENANCE2_EXTENSION_NAME,
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	VK_KHR_MULTIVIEW_EXTENSION_NAME,
	VK_KHR_RELAXED_BLOCK_LAYOUT_EXTENSION_NAME,
	VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
	VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
	VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
	VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME,
	nullptr
};

static const char * const s_promotedVk12Extensions[] = {
	VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
	VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
	VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
	VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
	VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
	VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
	VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
	VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
	VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME,
	VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
	VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
	VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
	VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
	VK_EXT_SEPARATE_STENCIL_USAGE_EXTENSION_NAME,
	VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
	nullptr
};

static const char * const s_promotedVk13Extensions[] = {
	VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME,
	VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
	VK_KHR_SHADER_INTEGER_DOT_PRODUCT_EXTENSION_NAME,
	VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
	VK_KHR_SHADER_TERMINATE_INVOCATION_EXTENSION_NAME,
	VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME,
	VK_EXT_4444_FORMATS_EXTENSION_NAME,
	VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
	VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
	VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME,
	VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,
	VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
	VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
	VK_EXT_PRIVATE_DATA_EXTENSION_NAME,
	VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
	VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
	VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME,
	VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME,
	VK_EXT_TOOLING_INFO_EXTENSION_NAME,
	VK_EXT_YCBCR_2PLANE_444_FORMATS_EXTENSION_NAME,
	nullptr
};

#if defined(VK_VERSION_1_4)
static const char * const s_promotedVk14Extensions[] = {
    VK_KHR_LOAD_STORE_OP_NONE_EXTENSION_NAME,
    VK_KHR_SHADER_SUBGROUP_ROTATE_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_2_EXTENSION_NAME,
    VK_KHR_SHADER_EXPECT_ASSUME_EXTENSION_NAME,
    VK_KHR_LINE_RASTERIZATION_EXTENSION_NAME,
    VK_KHR_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
    VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME,
    VK_KHR_MAP_MEMORY_2_EXTENSION_NAME,
    VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	nullptr
};
#endif

// clang-format on

template <typename VkType, typename CoreType>
struct XType {
	constexpr ~XType() = default;

	constexpr XType() : value(0) { }
	constexpr XType(VkType v) : value(v) { }
	constexpr XType(CoreType v) : value(VkType(v)) { }

	constexpr XType(const XType &) = default;
	constexpr XType &operator=(const XType &) = default;

	constexpr operator VkType() const { return value; }
	constexpr operator CoreType() const { return CoreType(value); }

	constexpr auto operator<=>(const XType &) const = default;

	constexpr auto operator==(int v) const { return value == VkType(v); }
	constexpr auto operator!=(int v) const { return value != VkType(v); }
	constexpr auto operator==(VkType v) const { return value == v; }
	constexpr auto operator!=(VkType v) const { return value != v; }
	constexpr auto operator==(CoreType v) const { return value == VkType(v); }
	constexpr auto operator!=(CoreType v) const { return value != VkType(v); }

	constexpr inline XType operator|(const XType &r) { return XType(value | r.value); }
	constexpr inline XType operator&(const XType &r) { return XType(value & r.value); }
	constexpr inline XType operator^(const XType &r) { return XType(value ^ r.value); }

	constexpr inline XType &operator|=(const XType &r) {
		value |= r.value;
		return *this;
	}
	constexpr inline XType &operator&=(const XType &r) {
		value &= r.value;
		return *this;
	}
	constexpr inline XType &operator^=(const XType &r) {
		value ^= r.value;
		return *this;
	}

	constexpr XType operator~() const { return XType(~value); }

	VkType value;
};

using XAccessFlags = XType<VkAccessFlags, core::AccessType>;
using XImageLayout = XType<VkImageLayout, core::AttachmentLayout>;
using XPipelineStage = XType<VkPipelineStageFlags, core::PipelineStage>;

SP_PUBLIC core::QueueFlags getQueueFlags(VkQueueFlags, bool present);
SP_PUBLIC core::QueueFlags getQueueFlags(core::PassType);
SP_PUBLIC VkShaderStageFlagBits getVkStageBits(core::ProgramStage);

SP_PUBLIC StringView getVkFormatName(VkFormat fmt);
SP_PUBLIC StringView getVkColorSpaceName(VkColorSpaceKHR fmt);
SP_PUBLIC StringView getVkResultName(VkResult res);

SP_PUBLIC String getVkMemoryPropertyFlags(VkMemoryPropertyFlags);

SP_PUBLIC bool checkIfExtensionAvailable(uint32_t apiVersion, const char *name,
		const Vector<VkExtensionProperties> &available, Vector<StringView> &optionals,
		Vector<StringView> &promoted, std::bitset<toInt(OptionalDeviceExtension::Max)> &flags);

SP_PUBLIC bool isPromotedExtension(uint32_t apiVersion, StringView name);

SP_PUBLIC VkPresentModeKHR getVkPresentMode(core::PresentMode presentMode);

template <typename T>
SP_PUBLIC void sanitizeVkStruct(T &t) {
	::__sprt_memset(&t, 0, sizeof(T));
}

SP_PUBLIC Status getStatus(VkResult res);

SP_PUBLIC std::ostream &operator<<(std::ostream &stream, VkResult res);

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVK_H_ */
