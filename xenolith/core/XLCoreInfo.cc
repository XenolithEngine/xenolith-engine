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

#include "XLCommon.h" // IWYU pragma: keep
#include "XLCoreObject.h" // IWYU pragma: keep
#include "XLCoreInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

const ColorMode ColorMode::SolidColor = ColorMode();
const ColorMode ColorMode::IntensityChannel(core::ComponentMapping::R, core::ComponentMapping::One);
const ColorMode ColorMode::AlphaChannel(core::ComponentMapping::One, core::ComponentMapping::R);

SubresourceRangeInfo::SubresourceRangeInfo(ObjectType t) : type(t), buffer{0, maxOf<uint64_t>()} { }

SubresourceRangeInfo::SubresourceRangeInfo(ObjectType t, uint64_t offset, uint64_t size)
: type(t), buffer{offset, size} { }

// assume image data
SubresourceRangeInfo::SubresourceRangeInfo(ObjectType t, ImageAspects a)
: type(t), image{a, 0, maxOf<uint32_t>(), 0, maxOf<uint32_t>()} { }

SubresourceRangeInfo::SubresourceRangeInfo(ObjectType t, ImageAspects a, uint32_t ml, uint32_t nml,
		uint32_t al, uint32_t nal)
: type(t), image{a, ml, nml, al, nal} { }

String getBufferFlagsDescription(BufferFlags fmt) {
	StringStream stream;
	if ((fmt & BufferFlags::SparceBinding) != BufferFlags::None) {
		stream << " SparceBinding";
	}
	if ((fmt & BufferFlags::SparceResidency) != BufferFlags::None) {
		stream << " SparceResidency";
	}
	if ((fmt & BufferFlags::SparceAliased) != BufferFlags::None) {
		stream << " SparceAliased";
	}
	if ((fmt & BufferFlags::Protected) != BufferFlags::None) {
		stream << " Protected";
	}
	return stream.str();
}

String getBufferUsageDescription(BufferUsage fmt) {
	StringStream stream;
	if ((fmt & BufferUsage::TransferSrc) != BufferUsage::None) {
		stream << " TransferSrc";
	}
	if ((fmt & BufferUsage::TransferDst) != BufferUsage::None) {
		stream << " TransferDst";
	}
	if ((fmt & BufferUsage::UniformTexelBuffer) != BufferUsage::None) {
		stream << " UniformTexelBuffer";
	}
	if ((fmt & BufferUsage::StorageTexelBuffer) != BufferUsage::None) {
		stream << " StorageTexelBuffer";
	}
	if ((fmt & BufferUsage::UniformBuffer) != BufferUsage::None) {
		stream << " UniformBuffer";
	}
	if ((fmt & BufferUsage::StorageBuffer) != BufferUsage::None) {
		stream << " StorageBuffer";
	}
	if ((fmt & BufferUsage::IndexBuffer) != BufferUsage::None) {
		stream << " IndexBuffer";
	}
	if ((fmt & BufferUsage::VertexBuffer) != BufferUsage::None) {
		stream << " VertexBuffer";
	}
	if ((fmt & BufferUsage::IndirectBuffer) != BufferUsage::None) {
		stream << " IndirectBuffer";
	}
	if ((fmt & BufferUsage::ShaderDeviceAddress) != BufferUsage::None) {
		stream << " ShaderDeviceAddress";
	}
	if ((fmt & BufferUsage::TransformFeedback) != BufferUsage::None) {
		stream << " TransformFeedback";
	}
	if ((fmt & BufferUsage::TransformFeedbackCounter) != BufferUsage::None) {
		stream << " TransformFeedbackCounter";
	}
	if ((fmt & BufferUsage::ConditionalRendering) != BufferUsage::None) {
		stream << " ConditionalRendering";
	}
	if ((fmt & BufferUsage::AccelerationStructureBuildInputReadOnly) != BufferUsage::None) {
		stream << " AccelerationStructureBuildInputReadOnly";
	}
	if ((fmt & BufferUsage::AccelerationStructureStorage) != BufferUsage::None) {
		stream << " AccelerationStructureStorage";
	}
	if ((fmt & BufferUsage::ShaderBindingTable) != BufferUsage::None) {
		stream << " ShaderBindingTable";
	}
	return stream.str();
}

String getImageFlagsDescription(ImageFlags fmt) {
	StringStream stream;
	if ((fmt & ImageFlags::SparceBinding) != ImageFlags::None) {
		stream << " SparceBinding";
	}
	if ((fmt & ImageFlags::SparceResidency) != ImageFlags::None) {
		stream << " SparceResidency";
	}
	if ((fmt & ImageFlags::SparceAliased) != ImageFlags::None) {
		stream << " SparceAliased";
	}
	if ((fmt & ImageFlags::MutableFormat) != ImageFlags::None) {
		stream << " MutableFormat";
	}
	if ((fmt & ImageFlags::CubeCompatible) != ImageFlags::None) {
		stream << " CubeCompatible";
	}
	if ((fmt & ImageFlags::Alias) != ImageFlags::None) {
		stream << " Alias";
	}
	if ((fmt & ImageFlags::SplitInstanceBindRegions) != ImageFlags::None) {
		stream << " SplitInstanceBindRegions";
	}
	if ((fmt & ImageFlags::Array2dCompatible) != ImageFlags::None) {
		stream << " Array2dCompatible";
	}
	if ((fmt & ImageFlags::BlockTexelViewCompatible) != ImageFlags::None) {
		stream << " BlockTexelViewCompatible";
	}
	if ((fmt & ImageFlags::ExtendedUsage) != ImageFlags::None) {
		stream << " ExtendedUsage";
	}
	if ((fmt & ImageFlags::Protected) != ImageFlags::None) {
		stream << " Protected";
	}
	if ((fmt & ImageFlags::Disjoint) != ImageFlags::None) {
		stream << " Disjoint";
	}
	return stream.str();
}

String getSampleCountDescription(SampleCount fmt) {
	StringStream stream;
	if ((fmt & SampleCount::X1) != SampleCount::None) {
		stream << " x1";
	}
	if ((fmt & SampleCount::X2) != SampleCount::None) {
		stream << " x2";
	}
	if ((fmt & SampleCount::X4) != SampleCount::None) {
		stream << " x4";
	}
	if ((fmt & SampleCount::X8) != SampleCount::None) {
		stream << " x8";
	}
	if ((fmt & SampleCount::X16) != SampleCount::None) {
		stream << " x16";
	}
	if ((fmt & SampleCount::X32) != SampleCount::None) {
		stream << " x32";
	}
	if ((fmt & SampleCount::X64) != SampleCount::None) {
		stream << " x64";
	}
	return stream.str();
}

StringView getImageTypeName(ImageType type) {
	switch (type) {
	case ImageType::Image1D: return StringView("1D"); break;
	case ImageType::Image2D: return StringView("2D"); break;
	case ImageType::Image3D: return StringView("3D"); break;
	}
	return StringView("Unknown");
}

StringView getImageViewTypeName(ImageViewType type) {
	switch (type) {
	case ImageViewType::ImageView1D: return StringView("1D"); break;
	case ImageViewType::ImageView1DArray: return StringView("1DArray"); break;
	case ImageViewType::ImageView2D: return StringView("2D"); break;
	case ImageViewType::ImageView2DArray: return StringView("2DArray"); break;
	case ImageViewType::ImageView3D: return StringView("3D"); break;
	case ImageViewType::ImageViewCube: return StringView("Cube"); break;
	case ImageViewType::ImageViewCubeArray: return StringView("CubeArray"); break;
	}
	return StringView("Unknown");
}

StringView getImageFormatName(ImageFormat fmt) {
	switch (fmt) {
	case ImageFormat::Undefined: return StringView("Undefined"); break;
	case ImageFormat::R4G4_UNORM_PACK8: return StringView("R4G4_UNORM_PACK8"); break;
	case ImageFormat::R4G4B4A4_UNORM_PACK16: return StringView("R4G4B4A4_UNORM_PACK16"); break;
	case ImageFormat::B4G4R4A4_UNORM_PACK16: return StringView("B4G4R4A4_UNORM_PACK16"); break;
	case ImageFormat::R5G6B5_UNORM_PACK16: return StringView("R5G6B5_UNORM_PACK16"); break;
	case ImageFormat::B5G6R5_UNORM_PACK16: return StringView("B5G6R5_UNORM_PACK16"); break;
	case ImageFormat::R5G5B5A1_UNORM_PACK16: return StringView("R5G5B5A1_UNORM_PACK16"); break;
	case ImageFormat::B5G5R5A1_UNORM_PACK16: return StringView("B5G5R5A1_UNORM_PACK16"); break;
	case ImageFormat::A1R5G5B5_UNORM_PACK16: return StringView("A1R5G5B5_UNORM_PACK16"); break;
	case ImageFormat::R8_UNORM: return StringView("R8_UNORM"); break;
	case ImageFormat::R8_SNORM: return StringView("R8_SNORM"); break;
	case ImageFormat::R8_USCALED: return StringView("R8_USCALED"); break;
	case ImageFormat::R8_SSCALED: return StringView("R8_SSCALED"); break;
	case ImageFormat::R8_UINT: return StringView("R8_UINT"); break;
	case ImageFormat::R8_SINT: return StringView("R8_SINT"); break;
	case ImageFormat::R8_SRGB: return StringView("R8_SRGB"); break;
	case ImageFormat::R8G8_UNORM: return StringView("R8G8_UNORM"); break;
	case ImageFormat::R8G8_SNORM: return StringView("R8G8_SNORM"); break;
	case ImageFormat::R8G8_USCALED: return StringView("R8G8_USCALED"); break;
	case ImageFormat::R8G8_SSCALED: return StringView("R8G8_SSCALED"); break;
	case ImageFormat::R8G8_UINT: return StringView("R8G8_UINT"); break;
	case ImageFormat::R8G8_SINT: return StringView("R8G8_SINT"); break;
	case ImageFormat::R8G8_SRGB: return StringView("R8G8_SRGB"); break;
	case ImageFormat::R8G8B8_UNORM: return StringView("R8G8B8_UNORM"); break;
	case ImageFormat::R8G8B8_SNORM: return StringView("R8G8B8_SNORM"); break;
	case ImageFormat::R8G8B8_USCALED: return StringView("R8G8B8_USCALED"); break;
	case ImageFormat::R8G8B8_SSCALED: return StringView("R8G8B8_SSCALED"); break;
	case ImageFormat::R8G8B8_UINT: return StringView("R8G8B8_UINT"); break;
	case ImageFormat::R8G8B8_SINT: return StringView("R8G8B8_SINT"); break;
	case ImageFormat::R8G8B8_SRGB: return StringView("R8G8B8_SRGB"); break;
	case ImageFormat::B8G8R8_UNORM: return StringView("B8G8R8_UNORM"); break;
	case ImageFormat::B8G8R8_SNORM: return StringView("B8G8R8_SNORM"); break;
	case ImageFormat::B8G8R8_USCALED: return StringView("B8G8R8_USCALED"); break;
	case ImageFormat::B8G8R8_SSCALED: return StringView("B8G8R8_SSCALED"); break;
	case ImageFormat::B8G8R8_UINT: return StringView("B8G8R8_UINT"); break;
	case ImageFormat::B8G8R8_SINT: return StringView("B8G8R8_SINT"); break;
	case ImageFormat::B8G8R8_SRGB: return StringView("B8G8R8_SRGB"); break;
	case ImageFormat::R8G8B8A8_UNORM: return StringView("R8G8B8A8_UNORM"); break;
	case ImageFormat::R8G8B8A8_SNORM: return StringView("R8G8B8A8_SNORM"); break;
	case ImageFormat::R8G8B8A8_USCALED: return StringView("R8G8B8A8_USCALED"); break;
	case ImageFormat::R8G8B8A8_SSCALED: return StringView("R8G8B8A8_SSCALED"); break;
	case ImageFormat::R8G8B8A8_UINT: return StringView("R8G8B8A8_UINT"); break;
	case ImageFormat::R8G8B8A8_SINT: return StringView("R8G8B8A8_SINT"); break;
	case ImageFormat::R8G8B8A8_SRGB: return StringView("R8G8B8A8_SRGB"); break;
	case ImageFormat::B8G8R8A8_UNORM: return StringView("B8G8R8A8_UNORM"); break;
	case ImageFormat::B8G8R8A8_SNORM: return StringView("B8G8R8A8_SNORM"); break;
	case ImageFormat::B8G8R8A8_USCALED: return StringView("B8G8R8A8_USCALED"); break;
	case ImageFormat::B8G8R8A8_SSCALED: return StringView("B8G8R8A8_SSCALED"); break;
	case ImageFormat::B8G8R8A8_UINT: return StringView("B8G8R8A8_UINT"); break;
	case ImageFormat::B8G8R8A8_SINT: return StringView("B8G8R8A8_SINT"); break;
	case ImageFormat::B8G8R8A8_SRGB: return StringView("B8G8R8A8_SRGB"); break;
	case ImageFormat::A8B8G8R8_UNORM_PACK32: return StringView("A8B8G8R8_UNORM_PACK32"); break;
	case ImageFormat::A8B8G8R8_SNORM_PACK32: return StringView("A8B8G8R8_SNORM_PACK32"); break;
	case ImageFormat::A8B8G8R8_USCALED_PACK32: return StringView("A8B8G8R8_USCALED_PACK32"); break;
	case ImageFormat::A8B8G8R8_SSCALED_PACK32: return StringView("A8B8G8R8_SSCALED_PACK32"); break;
	case ImageFormat::A8B8G8R8_UINT_PACK32: return StringView("A8B8G8R8_UINT_PACK32"); break;
	case ImageFormat::A8B8G8R8_SINT_PACK32: return StringView("A8B8G8R8_SINT_PACK32"); break;
	case ImageFormat::A8B8G8R8_SRGB_PACK32: return StringView("A8B8G8R8_SRGB_PACK32"); break;
	case ImageFormat::A2R10G10B10_UNORM_PACK32:
		return StringView("A2R10G10B10_UNORM_PACK32");
		break;
	case ImageFormat::A2R10G10B10_SNORM_PACK32:
		return StringView("A2R10G10B10_SNORM_PACK32");
		break;
	case ImageFormat::A2R10G10B10_USCALED_PACK32:
		return StringView("A2R10G10B10_USCALED_PACK32");
		break;
	case ImageFormat::A2R10G10B10_SSCALED_PACK32:
		return StringView("A2R10G10B10_SSCALED_PACK32");
		break;
	case ImageFormat::A2R10G10B10_UINT_PACK32: return StringView("A2R10G10B10_UINT_PACK32"); break;
	case ImageFormat::A2R10G10B10_SINT_PACK32: return StringView("A2R10G10B10_SINT_PACK32"); break;
	case ImageFormat::A2B10G10R10_UNORM_PACK32:
		return StringView("A2B10G10R10_UNORM_PACK32");
		break;
	case ImageFormat::A2B10G10R10_SNORM_PACK32:
		return StringView("A2B10G10R10_SNORM_PACK32");
		break;
	case ImageFormat::A2B10G10R10_USCALED_PACK32:
		return StringView("A2B10G10R10_USCALED_PACK32");
		break;
	case ImageFormat::A2B10G10R10_SSCALED_PACK32:
		return StringView("A2B10G10R10_SSCALED_PACK32");
		break;
	case ImageFormat::A2B10G10R10_UINT_PACK32: return StringView("A2B10G10R10_UINT_PACK32"); break;
	case ImageFormat::A2B10G10R10_SINT_PACK32: return StringView("A2B10G10R10_SINT_PACK32"); break;
	case ImageFormat::R16_UNORM: return StringView("R16_UNORM"); break;
	case ImageFormat::R16_SNORM: return StringView("R16_SNORM"); break;
	case ImageFormat::R16_USCALED: return StringView("R16_USCALED"); break;
	case ImageFormat::R16_SSCALED: return StringView("R16_SSCALED"); break;
	case ImageFormat::R16_UINT: return StringView("R16_UINT"); break;
	case ImageFormat::R16_SINT: return StringView("R16_SINT"); break;
	case ImageFormat::R16_SFLOAT: return StringView("R16_SFLOAT"); break;
	case ImageFormat::R16G16_UNORM: return StringView("R16G16_UNORM"); break;
	case ImageFormat::R16G16_SNORM: return StringView("R16G16_SNORM"); break;
	case ImageFormat::R16G16_USCALED: return StringView("R16G16_USCALED"); break;
	case ImageFormat::R16G16_SSCALED: return StringView("R16G16_SSCALED"); break;
	case ImageFormat::R16G16_UINT: return StringView("R16G16_UINT"); break;
	case ImageFormat::R16G16_SINT: return StringView("R16G16_SINT"); break;
	case ImageFormat::R16G16_SFLOAT: return StringView("R16G16_SFLOAT"); break;
	case ImageFormat::R16G16B16_UNORM: return StringView("R16G16B16_UNORM"); break;
	case ImageFormat::R16G16B16_SNORM: return StringView("R16G16B16_SNORM"); break;
	case ImageFormat::R16G16B16_USCALED: return StringView("R16G16B16_USCALED"); break;
	case ImageFormat::R16G16B16_SSCALED: return StringView("R16G16B16_SSCALED"); break;
	case ImageFormat::R16G16B16_UINT: return StringView("R16G16B16_UINT"); break;
	case ImageFormat::R16G16B16_SINT: return StringView("R16G16B16_SINT"); break;
	case ImageFormat::R16G16B16_SFLOAT: return StringView("R16G16B16_SFLOAT"); break;
	case ImageFormat::R16G16B16A16_UNORM: return StringView("R16G16B16A16_UNORM"); break;
	case ImageFormat::R16G16B16A16_SNORM: return StringView("R16G16B16A16_SNORM"); break;
	case ImageFormat::R16G16B16A16_USCALED: return StringView("R16G16B16A16_USCALED"); break;
	case ImageFormat::R16G16B16A16_SSCALED: return StringView("R16G16B16A16_SSCALED"); break;
	case ImageFormat::R16G16B16A16_UINT: return StringView("R16G16B16A16_UINT"); break;
	case ImageFormat::R16G16B16A16_SINT: return StringView("R16G16B16A16_SINT"); break;
	case ImageFormat::R16G16B16A16_SFLOAT: return StringView("R16G16B16A16_SFLOAT"); break;
	case ImageFormat::R32_UINT: return StringView("R32_UINT"); break;
	case ImageFormat::R32_SINT: return StringView("R32_SINT"); break;
	case ImageFormat::R32_SFLOAT: return StringView("R32_SFLOAT"); break;
	case ImageFormat::R32G32_UINT: return StringView("R32G32_UINT"); break;
	case ImageFormat::R32G32_SINT: return StringView("R32G32_SINT"); break;
	case ImageFormat::R32G32_SFLOAT: return StringView("R32G32_SFLOAT"); break;
	case ImageFormat::R32G32B32_UINT: return StringView("R32G32B32_UINT"); break;
	case ImageFormat::R32G32B32_SINT: return StringView("R32G32B32_SINT"); break;
	case ImageFormat::R32G32B32_SFLOAT: return StringView("R32G32B32_SFLOAT"); break;
	case ImageFormat::R32G32B32A32_UINT: return StringView("R32G32B32A32_UINT"); break;
	case ImageFormat::R32G32B32A32_SINT: return StringView("R32G32B32A32_SINT"); break;
	case ImageFormat::R32G32B32A32_SFLOAT: return StringView("R32G32B32A32_SFLOAT"); break;
	case ImageFormat::R64_UINT: return StringView("R64_UINT"); break;
	case ImageFormat::R64_SINT: return StringView("R64_SINT"); break;
	case ImageFormat::R64_SFLOAT: return StringView("R64_SFLOAT"); break;
	case ImageFormat::R64G64_UINT: return StringView("R64G64_UINT"); break;
	case ImageFormat::R64G64_SINT: return StringView("R64G64_SINT"); break;
	case ImageFormat::R64G64_SFLOAT: return StringView("R64G64_SFLOAT"); break;
	case ImageFormat::R64G64B64_UINT: return StringView("R64G64B64_UINT"); break;
	case ImageFormat::R64G64B64_SINT: return StringView("R64G64B64_SINT"); break;
	case ImageFormat::R64G64B64_SFLOAT: return StringView("R64G64B64_SFLOAT"); break;
	case ImageFormat::R64G64B64A64_UINT: return StringView("R64G64B64A64_UINT"); break;
	case ImageFormat::R64G64B64A64_SINT: return StringView("R64G64B64A64_SINT"); break;
	case ImageFormat::R64G64B64A64_SFLOAT: return StringView("R64G64B64A64_SFLOAT"); break;
	case ImageFormat::B10G11R11_UFLOAT_PACK32: return StringView("B10G11R11_UFLOAT_PACK32"); break;
	case ImageFormat::E5B9G9R9_UFLOAT_PACK32: return StringView("E5B9G9R9_UFLOAT_PACK32"); break;
	case ImageFormat::D16_UNORM: return StringView("D16_UNORM"); break;
	case ImageFormat::X8_D24_UNORM_PACK32: return StringView("X8_D24_UNORM_PACK32"); break;
	case ImageFormat::D32_SFLOAT: return StringView("D32_SFLOAT"); break;
	case ImageFormat::S8_UINT: return StringView("S8_UINT"); break;
	case ImageFormat::D16_UNORM_S8_UINT: return StringView("D16_UNORM_S8_UINT"); break;
	case ImageFormat::D24_UNORM_S8_UINT: return StringView("D24_UNORM_S8_UINT"); break;
	case ImageFormat::D32_SFLOAT_S8_UINT: return StringView("D32_SFLOAT_S8_UINT"); break;
	case ImageFormat::BC1_RGB_UNORM_BLOCK: return StringView("BC1_RGB_UNORM_BLOCK"); break;
	case ImageFormat::BC1_RGB_SRGB_BLOCK: return StringView("BC1_RGB_SRGB_BLOCK"); break;
	case ImageFormat::BC1_RGBA_UNORM_BLOCK: return StringView("BC1_RGBA_UNORM_BLOCK"); break;
	case ImageFormat::BC1_RGBA_SRGB_BLOCK: return StringView("BC1_RGBA_SRGB_BLOCK"); break;
	case ImageFormat::BC2_UNORM_BLOCK: return StringView("BC2_UNORM_BLOCK"); break;
	case ImageFormat::BC2_SRGB_BLOCK: return StringView("BC2_SRGB_BLOCK"); break;
	case ImageFormat::BC3_UNORM_BLOCK: return StringView("BC3_UNORM_BLOCK"); break;
	case ImageFormat::BC3_SRGB_BLOCK: return StringView("BC3_SRGB_BLOCK"); break;
	case ImageFormat::BC4_UNORM_BLOCK: return StringView("BC4_UNORM_BLOCK"); break;
	case ImageFormat::BC4_SNORM_BLOCK: return StringView("BC4_SNORM_BLOCK"); break;
	case ImageFormat::BC5_UNORM_BLOCK: return StringView("BC5_UNORM_BLOCK"); break;
	case ImageFormat::BC5_SNORM_BLOCK: return StringView("BC5_SNORM_BLOCK"); break;
	case ImageFormat::BC6H_UFLOAT_BLOCK: return StringView("BC6H_UFLOAT_BLOCK"); break;
	case ImageFormat::BC6H_SFLOAT_BLOCK: return StringView("BC6H_SFLOAT_BLOCK"); break;
	case ImageFormat::BC7_UNORM_BLOCK: return StringView("BC7_UNORM_BLOCK"); break;
	case ImageFormat::BC7_SRGB_BLOCK: return StringView("BC7_SRGB_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8_UNORM_BLOCK: return StringView("ETC2_R8G8B8_UNORM_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8_SRGB_BLOCK: return StringView("ETC2_R8G8B8_SRGB_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK:
		return StringView("ETC2_R8G8B8A1_UNORM_BLOCK");
		break;
	case ImageFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
		return StringView("ETC2_R8G8B8A1_SRGB_BLOCK");
		break;
	case ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK:
		return StringView("ETC2_R8G8B8A8_UNORM_BLOCK");
		break;
	case ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
		return StringView("ETC2_R8G8B8A8_SRGB_BLOCK");
		break;
	case ImageFormat::EAC_R11_UNORM_BLOCK: return StringView("EAC_R11_UNORM_BLOCK"); break;
	case ImageFormat::EAC_R11_SNORM_BLOCK: return StringView("EAC_R11_SNORM_BLOCK"); break;
	case ImageFormat::EAC_R11G11_UNORM_BLOCK: return StringView("EAC_R11G11_UNORM_BLOCK"); break;
	case ImageFormat::EAC_R11G11_SNORM_BLOCK: return StringView("EAC_R11G11_SNORM_BLOCK"); break;
	case ImageFormat::ASTC_4x4_UNORM_BLOCK: return StringView("ASTC_4x4_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_4x4_SRGB_BLOCK: return StringView("ASTC_4x4_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_5x4_UNORM_BLOCK: return StringView("ASTC_5x4_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_5x4_SRGB_BLOCK: return StringView("ASTC_5x4_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_5x5_UNORM_BLOCK: return StringView("ASTC_5x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_5x5_SRGB_BLOCK: return StringView("ASTC_5x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_6x5_UNORM_BLOCK: return StringView("ASTC_6x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_6x5_SRGB_BLOCK: return StringView("ASTC_6x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_6x6_UNORM_BLOCK: return StringView("ASTC_6x6_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_6x6_SRGB_BLOCK: return StringView("ASTC_6x6_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_8x5_UNORM_BLOCK: return StringView("ASTC_8x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_8x5_SRGB_BLOCK: return StringView("ASTC_8x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_8x6_UNORM_BLOCK: return StringView("ASTC_8x6_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_8x6_SRGB_BLOCK: return StringView("ASTC_8x6_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_8x8_UNORM_BLOCK: return StringView("ASTC_8x8_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_8x8_SRGB_BLOCK: return StringView("ASTC_8x8_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x5_UNORM_BLOCK: return StringView("ASTC_10x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x5_SRGB_BLOCK: return StringView("ASTC_10x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x6_UNORM_BLOCK: return StringView("ASTC_10x6_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x6_SRGB_BLOCK: return StringView("ASTC_10x6_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x8_UNORM_BLOCK: return StringView("ASTC_10x8_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x8_SRGB_BLOCK: return StringView("ASTC_10x8_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x10_UNORM_BLOCK: return StringView("ASTC_10x10_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x10_SRGB_BLOCK: return StringView("ASTC_10x10_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_12x10_UNORM_BLOCK: return StringView("ASTC_12x10_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_12x10_SRGB_BLOCK: return StringView("ASTC_12x10_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_12x12_UNORM_BLOCK: return StringView("ASTC_12x12_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_12x12_SRGB_BLOCK: return StringView("ASTC_12x12_SRGB_BLOCK"); break;
	case ImageFormat::G8B8G8R8_422_UNORM: return StringView("G8B8G8R8_422_UNORM"); break;
	case ImageFormat::B8G8R8G8_422_UNORM: return StringView("B8G8R8G8_422_UNORM"); break;
	case ImageFormat::G8_B8_R8_3PLANE_420_UNORM:
		return StringView("G8_B8_R8_3PLANE_420_UNORM");
		break;
	case ImageFormat::G8_B8R8_2PLANE_420_UNORM:
		return StringView("G8_B8R8_2PLANE_420_UNORM");
		break;
	case ImageFormat::G8_B8_R8_3PLANE_422_UNORM:
		return StringView("G8_B8_R8_3PLANE_422_UNORM");
		break;
	case ImageFormat::G8_B8R8_2PLANE_422_UNORM:
		return StringView("G8_B8R8_2PLANE_422_UNORM");
		break;
	case ImageFormat::G8_B8_R8_3PLANE_444_UNORM:
		return StringView("G8_B8_R8_3PLANE_444_UNORM");
		break;
	case ImageFormat::R10X6_UNORM_PACK16: return StringView("R10X6_UNORM_PACK16"); break;
	case ImageFormat::R10X6G10X6_UNORM_2PACK16:
		return StringView("R10X6G10X6_UNORM_2PACK16");
		break;
	case ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		return StringView("R10X6G10X6B10X6A10X6_UNORM_4PACK16");
		break;
	case ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		return StringView("G10X6B10X6G10X6R10X6_422_UNORM_4PACK16");
		break;
	case ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		return StringView("B10X6G10X6R10X6G10X6_422_UNORM_4PACK16");
		break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		return StringView("G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16");
		break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		return StringView("G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16");
		break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		return StringView("G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16");
		break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		return StringView("G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16");
		break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		return StringView("G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16");
		break;
	case ImageFormat::R12X4_UNORM_PACK16: return StringView("R12X4_UNORM_PACK16"); break;
	case ImageFormat::R12X4G12X4_UNORM_2PACK16:
		return StringView("R12X4G12X4_UNORM_2PACK16");
		break;
	case ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		return StringView("R12X4G12X4B12X4A12X4_UNORM_4PACK16");
		break;
	case ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		return StringView("G12X4B12X4G12X4R12X4_422_UNORM_4PACK16");
		break;
	case ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		return StringView("B12X4G12X4R12X4G12X4_422_UNORM_4PACK16");
		break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		return StringView("G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16");
		break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		return StringView("G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16");
		break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		return StringView("G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16");
		break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		return StringView("G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16");
		break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		return StringView("G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16");
		break;
	case ImageFormat::G16B16G16R16_422_UNORM: return StringView("G16B16G16R16_422_UNORM"); break;
	case ImageFormat::B16G16R16G16_422_UNORM: return StringView("B16G16R16G16_422_UNORM"); break;
	case ImageFormat::G16_B16_R16_3PLANE_420_UNORM:
		return StringView("G16_B16_R16_3PLANE_420_UNORM");
		break;
	case ImageFormat::G16_B16R16_2PLANE_420_UNORM:
		return StringView("G16_B16R16_2PLANE_420_UNORM");
		break;
	case ImageFormat::G16_B16_R16_3PLANE_422_UNORM:
		return StringView("G16_B16_R16_3PLANE_422_UNORM");
		break;
	case ImageFormat::G16_B16R16_2PLANE_422_UNORM:
		return StringView("G16_B16R16_2PLANE_422_UNORM");
		break;
	case ImageFormat::G16_B16_R16_3PLANE_444_UNORM:
		return StringView("G16_B16_R16_3PLANE_444_UNORM");
		break;
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG:
		return StringView("PVRTC1_2BPP_UNORM_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG:
		return StringView("PVRTC1_4BPP_UNORM_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG:
		return StringView("PVRTC2_2BPP_UNORM_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG:
		return StringView("PVRTC2_4BPP_UNORM_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG:
		return StringView("PVRTC1_2BPP_SRGB_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG:
		return StringView("PVRTC1_4BPP_SRGB_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG:
		return StringView("PVRTC2_2BPP_SRGB_BLOCK_IMG");
		break;
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG:
		return StringView("PVRTC2_4BPP_SRGB_BLOCK_IMG");
		break;
	case ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_4x4_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_5x4_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_5x4_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_5x5_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_5x5_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_6x5_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_6x5_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_6x6_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_6x6_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_8x5_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_8x5_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_8x6_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_8x6_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_8x8_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_8x8_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_10x5_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_10x5_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_10x6_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_10x6_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_10x8_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_10x8_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_10x10_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_10x10_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_12x10_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_12x10_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT:
		return StringView("ASTC_12x12_SFLOAT_BLOCK_EXT");
		break;
	case ImageFormat::G8_B8R8_2PLANE_444_UNORM_EXT:
		return StringView("G8_B8R8_2PLANE_444_UNORM_EXT");
		break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
		return StringView("G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT");
		break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
		return StringView("G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT");
		break;
	case ImageFormat::G16_B16R16_2PLANE_444_UNORM_EXT:
		return StringView("G16_B16R16_2PLANE_444_UNORM_EXT");
		break;
	case ImageFormat::A4R4G4B4_UNORM_PACK16_EXT:
		return StringView("A4R4G4B4_UNORM_PACK16_EXT");
		break;
	case ImageFormat::A4B4G4R4_UNORM_PACK16_EXT:
		return StringView("A4B4G4R4_UNORM_PACK16_EXT");
		break;
	}
	return StringView("Unknown");
}

StringView getImageTilingName(ImageTiling type) {
	switch (type) {
	case ImageTiling::Optimal: return StringView("Optimal"); break;
	case ImageTiling::Linear: return StringView("Linear"); break;
	}
	return StringView("Unknown");
}

StringView getComponentMappingName(ComponentMapping mapping) {
	switch (mapping) {
	case ComponentMapping::Identity: return StringView("Id"); break;
	case ComponentMapping::Zero: return StringView("0"); break;
	case ComponentMapping::One: return StringView("1"); break;
	case ComponentMapping::R: return StringView("R"); break;
	case ComponentMapping::G: return StringView("G"); break;
	case ComponentMapping::B: return StringView("B"); break;
	case ComponentMapping::A: return StringView("A"); break;
	}
	return StringView("Unknown");
}

StringView getPresentModeName(PresentMode mode) {
	switch (mode) {
	case PresentMode::Immediate: return "IMMEDIATE"; break;
	case PresentMode::Mailbox: return "MAILBOX"; break;
	case PresentMode::Fifo: return "FIFO"; break;
	case PresentMode::FifoRelaxed: return "FIFO_RELAXED"; break;
	default: return "UNKNOWN"; break;
	}
	return StringView();
}

StringView getColorSpaceName(ColorSpace fmt) {
	switch (fmt) {
	case ColorSpace::SRGB_NONLINEAR_KHR: return StringView("SRGB_NONLINEAR_KHR"); break;
	case ColorSpace::DISPLAY_P3_NONLINEAR_EXT: return StringView("DISPLAY_P3_NONLINEAR_EXT"); break;
	case ColorSpace::EXTENDED_SRGB_LINEAR_EXT: return StringView("EXTENDED_SRGB_LINEAR_EXT"); break;
	case ColorSpace::DISPLAY_P3_LINEAR_EXT: return StringView("DISPLAY_P3_LINEAR_EXT"); break;
	case ColorSpace::DCI_P3_NONLINEAR_EXT: return StringView("DCI_P3_NONLINEAR_EXT"); break;
	case ColorSpace::BT709_LINEAR_EXT: return StringView("BT709_LINEAR_EXT"); break;
	case ColorSpace::BT709_NONLINEAR_EXT: return StringView("BT709_NONLINEAR_EXT"); break;
	case ColorSpace::BT2020_LINEAR_EXT: return StringView("BT2020_LINEAR_EXT"); break;
	case ColorSpace::HDR10_ST2084_EXT: return StringView("HDR10_ST2084_EXT"); break;
	case ColorSpace::DOLBYVISION_EXT: return StringView("DOLBYVISION_EXT"); break;
	case ColorSpace::HDR10_HLG_EXT: return StringView("HDR10_HLG_EXT"); break;
	case ColorSpace::ADOBERGB_LINEAR_EXT: return StringView("ADOBERGB_LINEAR_EXT"); break;
	case ColorSpace::ADOBERGB_NONLINEAR_EXT: return StringView("ADOBERGB_NONLINEAR_EXT"); break;
	case ColorSpace::PASS_THROUGH_EXT: return StringView("PASS_THROUGH_EXT"); break;
	case ColorSpace::EXTENDED_SRGB_NONLINEAR_EXT:
		return StringView("EXTENDED_SRGB_NONLINEAR_EXT");
		break;
	case ColorSpace::DISPLAY_NATIVE_AMD: return StringView("DISPLAY_NATIVE_AMD"); break;
	}
	return StringView();
}

String getCompositeAlphaFlagsDescription(CompositeAlphaFlags fmt) {
	StringStream stream;
	if ((fmt & CompositeAlphaFlags::Opaque) != CompositeAlphaFlags::None) {
		stream << " Opaque";
	}
	if ((fmt & CompositeAlphaFlags::Premultiplied) != CompositeAlphaFlags::None) {
		stream << " Premultiplied";
	}
	if ((fmt & CompositeAlphaFlags::Postmultiplied) != CompositeAlphaFlags::None) {
		stream << " Postmultiplied";
	}
	if ((fmt & CompositeAlphaFlags::Inherit) != CompositeAlphaFlags::None) {
		stream << " Inherit";
	}
	return stream.str();
}

String getSurfaceTransformFlagsDescription(SurfaceTransformFlags fmt) {
	StringStream stream;
	if ((fmt & SurfaceTransformFlags::Identity) != SurfaceTransformFlags::None) {
		stream << " Identity";
	}
	if ((fmt & SurfaceTransformFlags::Rotate90) != SurfaceTransformFlags::None) {
		stream << " Rotate90";
	}
	if ((fmt & SurfaceTransformFlags::Rotate180) != SurfaceTransformFlags::None) {
		stream << " Rotate180";
	}
	if ((fmt & SurfaceTransformFlags::Rotate270) != SurfaceTransformFlags::None) {
		stream << " Rotate270";
	}
	if ((fmt & SurfaceTransformFlags::Mirror) != SurfaceTransformFlags::None) {
		stream << " Mirror";
	}
	if ((fmt & SurfaceTransformFlags::MirrorRotate90) != SurfaceTransformFlags::None) {
		stream << " MirrorRotate90";
	}
	if ((fmt & SurfaceTransformFlags::MirrorRotate180) != SurfaceTransformFlags::None) {
		stream << " MirrorRotate180";
	}
	if ((fmt & SurfaceTransformFlags::MirrorRotate270) != SurfaceTransformFlags::None) {
		stream << " MirrorRotate270";
	}
	if ((fmt & SurfaceTransformFlags::Inherit) != SurfaceTransformFlags::None) {
		stream << " Inherit";
	}
	if ((fmt & SurfaceTransformFlags::PreRotated) != SurfaceTransformFlags::None) {
		stream << " PreRotated";
	}
	return stream.str();
}

String getImageUsageDescription(ImageUsage fmt) {
	StringStream stream;
	if ((fmt & ImageUsage::TransferSrc) != ImageUsage::None) {
		stream << " TransferSrc";
	}
	if ((fmt & ImageUsage::TransferDst) != ImageUsage::None) {
		stream << " TransferDst";
	}
	if ((fmt & ImageUsage::Sampled) != ImageUsage::None) {
		stream << " Sampled";
	}
	if ((fmt & ImageUsage::Storage) != ImageUsage::None) {
		stream << " Storage";
	}
	if ((fmt & ImageUsage::ColorAttachment) != ImageUsage::None) {
		stream << " ColorAttachment";
	}
	if ((fmt & ImageUsage::DepthStencilAttachment) != ImageUsage::None) {
		stream << " DepthStencilAttachment";
	}
	if ((fmt & ImageUsage::TransientAttachment) != ImageUsage::None) {
		stream << " TransientAttachment";
	}
	if ((fmt & ImageUsage::InputAttachment) != ImageUsage::None) {
		stream << " InputAttachment";
	}
	return stream.str();
}

SamplerIndex SamplerIndex::DefaultFilterNearest = SamplerIndex(0);
SamplerIndex SamplerIndex::DefaultFilterLinear = SamplerIndex(1);
SamplerIndex SamplerIndex::DefaultFilterLinearClamped = SamplerIndex(2);

String BufferInfo::description() const {
	StringStream stream;

	stream << "BufferInfo: " << size << " bytes; Flags:";
	if (flags != BufferFlags::None) {
		stream << getBufferFlagsDescription(flags);
	} else {
		stream << " None";
	}
	stream << ";  Usage:";
	if (usage != BufferUsage::None) {
		stream << getBufferUsageDescription(usage);
	} else {
		stream << " None";
	}
	stream << ";";
	if (persistent) {
		stream << " Persistent;";
	}
	return stream.str();
}

size_t BufferData::writeData(uint8_t *mem, size_t expected) const {
	if (size > expected) {
		log::source().error("core::BufferData", "Not enoudh space for buffer: ", size,
				" required, ", expected, " allocated");
		return 0;
	}

	if (!data.empty()) {
		auto outsize = data.size();
		memcpy(mem, data.data(), size);
		return outsize;
	} else if (memCallback) {
		size_t outsize = size;
		memCallback(mem, expected, [&, this](BytesView data) {
			outsize = data.size();
			memcpy(mem, data.data(), size);
		});
		return outsize;
	} else if (stdCallback) {
		size_t outsize = size;
		stdCallback(mem, expected, [&, this](BytesView data) {
			outsize = data.size();
			memcpy(mem, data.data(), size);
		});
		return outsize;
	}
	return 0;
}

ImageViewInfo ImageInfoData::getViewInfo(const ImageViewInfo &info) const {
	ImageViewInfo ret(info);
	if (ret.format == ImageFormat::Undefined) {
		ret.format = format;
	}
	if (ret.layerCount.get() == maxOf<uint32_t>()) {
		ret.layerCount = ArrayLayers(arrayLayers.get() - ret.baseArrayLayer.get());
	}
	return ret;
}

bool ImageInfo::isCompatible(const ImageInfo &img) const {
	if (img.format == format && img.flags == flags && img.imageType == imageType
			&& img.mipLevels == img.mipLevels && img.arrayLayers == arrayLayers
			&& img.samples == samples && img.tiling == tiling && img.usage == usage) {
		return true;
	}
	return true;
}

String ImageInfo::description() const {
	StringStream stream;
	stream << "ImageInfo: " << getImageFormatName(format) << " (" << getImageTypeName(imageType)
		   << "); ";
	stream << extent.width << " x " << extent.height << " x " << extent.depth << "; Flags:";

	if (flags != ImageFlags::None) {
		stream << getImageFlagsDescription(flags);
	} else {
		stream << " None";
	}

	stream << "; MipLevels: " << mipLevels.get() << "; ArrayLayers: " << arrayLayers.get()
		   << "; Samples:" << getSampleCountDescription(samples)
		   << "; Tiling: " << getImageTilingName(tiling) << "; Usage:";

	if (usage != ImageUsage::None) {
		stream << getImageUsageDescription(usage);
	} else {
		stream << " None";
	}
	stream << ";";
	return stream.str();
}

size_t ImageData::writeData(uint8_t *mem, size_t expected) const {
	uint64_t expectedSize = getFormatBlockSize(format) * extent.width * extent.height * extent.depth
			* arrayLayers.get();
	if (expectedSize > expected) {
		log::source().error("core::ImageData", "Not enoudh space for image: ", expectedSize,
				" required, ", expected, " allocated");
		return 0;
	}

	if (!data.empty()) {
		auto size = data.size();
		memcpy(mem, data.data(), size);
		return size;
	} else if (memCallback) {
		size_t size = expectedSize;
		size_t writeSize = 0;
		memCallback(mem, expectedSize, [&](BytesView data) {
			writeSize += data.size();
			memcpy(mem, data.data(), size);
			mem += data.size();
		});
		return writeSize != 0 ? writeSize : size;
	} else if (stdCallback) {
		size_t size = expectedSize;
		size_t writeSize = 0;
		stdCallback(mem, expectedSize, [&](BytesView data) {
			writeSize += data.size();
			memcpy(mem, data.data(), size);
			mem += data.size();
		});
		return writeSize != 0 ? writeSize : size;
	}
	return 0;
}

void ImageViewInfo::setup(const ImageViewInfo &value) { *this = value; }

void ImageViewInfo::setup(const ImageInfoData &value) {
	format = value.format;
	baseArrayLayer = BaseArrayLayer(0);
	if (layerCount.get() > 1) {
		setup(value.imageType, value.arrayLayers);
		layerCount = value.arrayLayers;
	} else {
		layerCount = value.arrayLayers;
		setup(value.imageType, value.arrayLayers);
	}
}

void ImageViewInfo::setup(ColorMode value, bool allowSwizzle) {
	switch (value.getMode()) {
	case ColorMode::Solid: {
		if (!allowSwizzle) {
			r = ComponentMapping::Identity;
			g = ComponentMapping::Identity;
			b = ComponentMapping::Identity;
			a = ComponentMapping::Identity;
			return;
		}
		auto f = getImagePixelFormat(format);
		switch (f) {
		case PixelFormat::Unknown: break;
		case PixelFormat::A:
			r = ComponentMapping::One;
			g = ComponentMapping::One;
			b = ComponentMapping::One;
			a = ComponentMapping::R;
			break;
		case PixelFormat::IA:
			r = ComponentMapping::R;
			g = ComponentMapping::R;
			b = ComponentMapping::R;
			a = ComponentMapping::G;
			break;
		case PixelFormat::RGB:
			r = ComponentMapping::Identity;
			g = ComponentMapping::Identity;
			b = ComponentMapping::Identity;
			a = ComponentMapping::One;
			break;
		case PixelFormat::RGBA:
		case PixelFormat::D:
		case PixelFormat::DS:
		case PixelFormat::S:
			r = ComponentMapping::Identity;
			g = ComponentMapping::Identity;
			b = ComponentMapping::Identity;
			a = ComponentMapping::Identity;
			break;
		}

		break;
	}
	case ColorMode::Custom:
		r = value.getR();
		g = value.getG();
		b = value.getB();
		a = value.getA();
		break;
	}
}

void ImageViewInfo::setup(ImageType t, ArrayLayers layers) {
	layerCount = layers;
	type = getImageViewType(t, layers);
}

ColorMode ImageViewInfo::getColorMode() const {
	auto f = getImagePixelFormat(format);
	switch (f) {
	case PixelFormat::Unknown: return ColorMode(); break;
	case PixelFormat::A:
		if (r == ComponentMapping::One && g == ComponentMapping::One && b == ComponentMapping::One
				&& a == ComponentMapping::R) {
			return ColorMode();
		}
		break;
	case PixelFormat::IA:
		if (r == ComponentMapping::R && g == ComponentMapping::R && b == ComponentMapping::R
				&& a == ComponentMapping::G) {
			return ColorMode();
		}
		break;
	case PixelFormat::RGB:
		if (r == ComponentMapping::Identity && g == ComponentMapping::Identity
				&& b == ComponentMapping::Identity && a == ComponentMapping::One) {
			return ColorMode();
		}
		break;
	case PixelFormat::RGBA:
	case PixelFormat::D:
	case PixelFormat::DS:
	case PixelFormat::S:
		if (r == ComponentMapping::Identity && g == ComponentMapping::Identity
				&& b == ComponentMapping::Identity && a == ComponentMapping::Identity) {
			return ColorMode();
		}
		break;
	}
	return ColorMode(r, g, b, a);
}

bool ImageViewInfo::isCompatible(const ImageInfo &info) const {
	// not perfect, multi-planar format not tracked, bun enough for now
	if (format != ImageFormat::Undefined
			&& getFormatBlockSize(info.format) != getFormatBlockSize(format)) {
		return false;
	}

	// check type compatibility
	switch (type) {
	case ImageViewType::ImageView1D:
		if (info.imageType != ImageType::Image1D) {
			return false;
		}
		break;
	case ImageViewType::ImageView1DArray:
		if (info.imageType != ImageType::Image1D) {
			return false;
		}
		break;
	case ImageViewType::ImageView2D:
		if (info.imageType != ImageType::Image2D && info.imageType != ImageType::Image3D) {
			return false;
		}
		break;
	case ImageViewType::ImageView2DArray:
		if (info.imageType != ImageType::Image2D && info.imageType != ImageType::Image3D) {
			return false;
		}
		break;
	case ImageViewType::ImageView3D:
		if (info.imageType != ImageType::Image3D) {
			return false;
		}
		break;
	case ImageViewType::ImageViewCube:
		if (info.imageType != ImageType::Image2D) {
			return false;
		}
		break;
	case ImageViewType::ImageViewCubeArray:
		if (info.imageType != ImageType::Image2D) {
			return false;
		}
		break;
	}

	// check array size compatibility
	if (baseArrayLayer.get() >= info.arrayLayers.get()) {
		return false;
	}

	if (layerCount.get() != maxOf<uint32_t>()
			&& baseArrayLayer.get() + layerCount.get() > info.arrayLayers.get()) {
		return false;
	}

	return true;
}

String ImageViewInfo::description() const {
	StringStream stream;
	stream << "ImageViewInfo: " << getImageFormatName(format) << " (" << getImageViewTypeName(type)
		   << "); ";
	stream << "ArrayLayers: " << baseArrayLayer.get() << " (" << layerCount.get() << "); ";
	stream << "R -> " << getComponentMappingName(r) << "; ";
	stream << "G -> " << getComponentMappingName(g) << "; ";
	stream << "B -> " << getComponentMappingName(b) << "; ";
	stream << "A -> " << getComponentMappingName(a) << "; ";
	return stream.str();
}

String SwapchainConfig::description() const {
	StringStream stream;
	stream << "\nSurfaceInfo:\n";
	stream << "\tPresentMode: " << getPresentModeName(presentMode);
	if (presentModeFast != PresentMode::Unsupported) {
		stream << " (" << getPresentModeName(presentModeFast) << ")";
	}
	stream << "\n";
	stream << "\tSurface format: (" << getImageFormatName(imageFormat) << ":"
		   << getColorSpaceName(colorSpace) << ")\n";
	stream << "\tTransform:" << getSurfaceTransformFlagsDescription(transform) << "\n";
	stream << "\tAlpha:" << getCompositeAlphaFlagsDescription(alpha) << "\n";
	stream << "\tImage count: " << imageCount << "\n";
	stream << "\tExtent: " << extent.width << "x" << extent.height << "\n";
	return stream.str();
}

bool SurfaceInfo::isSupported(const SwapchainConfig &cfg) const {
	if (std::find(presentModes.begin(), presentModes.end(), cfg.presentMode)
			== presentModes.end()) {
		log::source().error("Vk-Error", "SurfaceInfo: presentMode is not supported");
		return false;
	}

	if (cfg.presentModeFast != PresentMode::Unsupported
			&& std::find(presentModes.begin(), presentModes.end(), cfg.presentModeFast)
					== presentModes.end()) {
		log::source().error("Vk-Error", "SurfaceInfo: presentModeFast is not supported");
		return false;
	}

	if (std::find(formats.begin(), formats.end(), pair(cfg.imageFormat, cfg.colorSpace))
			== formats.end()) {
		log::source().error("Vk-Error", "SurfaceInfo: imageFormat or colorSpace is not supported");
		return false;
	}

	if ((supportedCompositeAlpha & cfg.alpha) == CompositeAlphaFlags::None) {
		log::source().error("Vk-Error", "SurfaceInfo: alpha is not supported");
		return false;
	}

	if ((supportedTransforms & cfg.transform) == SurfaceTransformFlags::None) {
		log::source().error("Vk-Error", "SurfaceInfo: transform is not supported");
		return false;
	}

	if (cfg.imageCount < minImageCount || (maxImageCount != 0 && cfg.imageCount > maxImageCount)) {
		log::source().error("Vk-Error", "SurfaceInfo: imageCount is not supported");
		return false;
	}

	if (cfg.extent.width < minImageExtent.width || cfg.extent.width > maxImageExtent.width
			|| cfg.extent.height < minImageExtent.height
			|| cfg.extent.height > maxImageExtent.height) {
		log::source().error("Vk-Error", "SurfaceInfo: extent is not supported");
		return false;
	}

	if (cfg.transfer && (supportedUsageFlags & ImageUsage::TransferDst) == ImageUsage::None) {
		log::source().error("Vk-Error", "SurfaceInfo: supportedUsageFlags is not supported");
		return false;
	}

	return true;
}

String SurfaceInfo::description() const {
	StringStream stream;
	stream << "\nSurfaceInfo:\n";
	stream << "\tImageCount: " << minImageCount << "-" << maxImageCount << "\n";
	stream << "\tExtent: " << currentExtent.width << "x" << currentExtent.height << " ("
		   << minImageExtent.width << "x" << minImageExtent.height << " - " << maxImageExtent.width
		   << "x" << maxImageExtent.height << ")\n";
	stream << "\tMax Layers: " << maxImageArrayLayers << "\n";

	stream << "\tSupported transforms:" << getSurfaceTransformFlagsDescription(supportedTransforms)
		   << "\n";
	stream << "\tCurrent transforms:" << getSurfaceTransformFlagsDescription(currentTransform)
		   << "\n";
	stream << "\tSupported Alpha:" << getCompositeAlphaFlagsDescription(supportedCompositeAlpha)
		   << "\n";
	stream << "\tSupported Usage:" << getImageUsageDescription(supportedUsageFlags) << "\n";

	stream << "\tSurface format:";
	for (auto it : formats) {
		stream << " (" << getImageFormatName(it.first) << ":" << getColorSpaceName(it.second)
			   << ")";
	}
	stream << "\n";

	stream << "\tPresent modes:";
	for (auto &it : presentModes) { stream << " " << getPresentModeName(it); }
	stream << "\n";
	return stream.str();
}

size_t getFormatBlockSize(ImageFormat format) {
	switch (format) {
	case ImageFormat::Undefined: return 0; break;
	case ImageFormat::R4G4_UNORM_PACK8: return 1; break;
	case ImageFormat::R4G4B4A4_UNORM_PACK16: return 2; break;
	case ImageFormat::B4G4R4A4_UNORM_PACK16: return 2; break;
	case ImageFormat::R5G6B5_UNORM_PACK16: return 2; break;
	case ImageFormat::B5G6R5_UNORM_PACK16: return 2; break;
	case ImageFormat::R5G5B5A1_UNORM_PACK16: return 2; break;
	case ImageFormat::B5G5R5A1_UNORM_PACK16: return 2; break;
	case ImageFormat::A1R5G5B5_UNORM_PACK16: return 2; break;
	case ImageFormat::R8_UNORM: return 1; break;
	case ImageFormat::R8_SNORM: return 1; break;
	case ImageFormat::R8_USCALED: return 1; break;
	case ImageFormat::R8_SSCALED: return 1; break;
	case ImageFormat::R8_UINT: return 1; break;
	case ImageFormat::R8_SINT: return 1; break;
	case ImageFormat::R8_SRGB: return 1; break;
	case ImageFormat::R8G8_UNORM: return 2; break;
	case ImageFormat::R8G8_SNORM: return 2; break;
	case ImageFormat::R8G8_USCALED: return 2; break;
	case ImageFormat::R8G8_SSCALED: return 2; break;
	case ImageFormat::R8G8_UINT: return 2; break;
	case ImageFormat::R8G8_SINT: return 2; break;
	case ImageFormat::R8G8_SRGB: return 2; break;
	case ImageFormat::R8G8B8_UNORM: return 3; break;
	case ImageFormat::R8G8B8_SNORM: return 3; break;
	case ImageFormat::R8G8B8_USCALED: return 3; break;
	case ImageFormat::R8G8B8_SSCALED: return 3; break;
	case ImageFormat::R8G8B8_UINT: return 3; break;
	case ImageFormat::R8G8B8_SINT: return 3; break;
	case ImageFormat::R8G8B8_SRGB: return 3; break;
	case ImageFormat::B8G8R8_UNORM: return 3; break;
	case ImageFormat::B8G8R8_SNORM: return 3; break;
	case ImageFormat::B8G8R8_USCALED: return 3; break;
	case ImageFormat::B8G8R8_SSCALED: return 3; break;
	case ImageFormat::B8G8R8_UINT: return 3; break;
	case ImageFormat::B8G8R8_SINT: return 3; break;
	case ImageFormat::B8G8R8_SRGB: return 3; break;
	case ImageFormat::R8G8B8A8_UNORM: return 4; break;
	case ImageFormat::R8G8B8A8_SNORM: return 4; break;
	case ImageFormat::R8G8B8A8_USCALED: return 4; break;
	case ImageFormat::R8G8B8A8_SSCALED: return 4; break;
	case ImageFormat::R8G8B8A8_UINT: return 4; break;
	case ImageFormat::R8G8B8A8_SINT: return 4; break;
	case ImageFormat::R8G8B8A8_SRGB: return 4; break;
	case ImageFormat::B8G8R8A8_UNORM: return 4; break;
	case ImageFormat::B8G8R8A8_SNORM: return 4; break;
	case ImageFormat::B8G8R8A8_USCALED: return 4; break;
	case ImageFormat::B8G8R8A8_SSCALED: return 4; break;
	case ImageFormat::B8G8R8A8_UINT: return 4; break;
	case ImageFormat::B8G8R8A8_SINT: return 4; break;
	case ImageFormat::B8G8R8A8_SRGB: return 4; break;
	case ImageFormat::A8B8G8R8_UNORM_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SNORM_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_USCALED_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SSCALED_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_UINT_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SINT_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SRGB_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_UNORM_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_SNORM_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_USCALED_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_SSCALED_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_UINT_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_SINT_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_UNORM_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_SNORM_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_USCALED_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_SSCALED_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_UINT_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_SINT_PACK32: return 4; break;
	case ImageFormat::R16_UNORM: return 2; break;
	case ImageFormat::R16_SNORM: return 2; break;
	case ImageFormat::R16_USCALED: return 2; break;
	case ImageFormat::R16_SSCALED: return 2; break;
	case ImageFormat::R16_UINT: return 2; break;
	case ImageFormat::R16_SINT: return 2; break;
	case ImageFormat::R16_SFLOAT: return 2; break;
	case ImageFormat::R16G16_UNORM: return 4; break;
	case ImageFormat::R16G16_SNORM: return 4; break;
	case ImageFormat::R16G16_USCALED: return 4; break;
	case ImageFormat::R16G16_SSCALED: return 4; break;
	case ImageFormat::R16G16_UINT: return 4; break;
	case ImageFormat::R16G16_SINT: return 4; break;
	case ImageFormat::R16G16_SFLOAT: return 4; break;
	case ImageFormat::R16G16B16_UNORM: return 6; break;
	case ImageFormat::R16G16B16_SNORM: return 6; break;
	case ImageFormat::R16G16B16_USCALED: return 6; break;
	case ImageFormat::R16G16B16_SSCALED: return 6; break;
	case ImageFormat::R16G16B16_UINT: return 6; break;
	case ImageFormat::R16G16B16_SINT: return 6; break;
	case ImageFormat::R16G16B16_SFLOAT: return 6; break;
	case ImageFormat::R16G16B16A16_UNORM: return 8; break;
	case ImageFormat::R16G16B16A16_SNORM: return 8; break;
	case ImageFormat::R16G16B16A16_USCALED: return 8; break;
	case ImageFormat::R16G16B16A16_SSCALED: return 8; break;
	case ImageFormat::R16G16B16A16_UINT: return 8; break;
	case ImageFormat::R16G16B16A16_SINT: return 8; break;
	case ImageFormat::R16G16B16A16_SFLOAT: return 8; break;
	case ImageFormat::R32_UINT: return 4; break;
	case ImageFormat::R32_SINT: return 4; break;
	case ImageFormat::R32_SFLOAT: return 4; break;
	case ImageFormat::R32G32_UINT: return 8; break;
	case ImageFormat::R32G32_SINT: return 8; break;
	case ImageFormat::R32G32_SFLOAT: return 8; break;
	case ImageFormat::R32G32B32_UINT: return 12; break;
	case ImageFormat::R32G32B32_SINT: return 12; break;
	case ImageFormat::R32G32B32_SFLOAT: return 12; break;
	case ImageFormat::R32G32B32A32_UINT: return 16; break;
	case ImageFormat::R32G32B32A32_SINT: return 16; break;
	case ImageFormat::R32G32B32A32_SFLOAT: return 16; break;
	case ImageFormat::R64_UINT: return 8; break;
	case ImageFormat::R64_SINT: return 8; break;
	case ImageFormat::R64_SFLOAT: return 8; break;
	case ImageFormat::R64G64_UINT: return 16; break;
	case ImageFormat::R64G64_SINT: return 16; break;
	case ImageFormat::R64G64_SFLOAT: return 16; break;
	case ImageFormat::R64G64B64_UINT: return 24; break;
	case ImageFormat::R64G64B64_SINT: return 24; break;
	case ImageFormat::R64G64B64_SFLOAT: return 24; break;
	case ImageFormat::R64G64B64A64_UINT: return 32; break;
	case ImageFormat::R64G64B64A64_SINT: return 32; break;
	case ImageFormat::R64G64B64A64_SFLOAT: return 32; break;
	case ImageFormat::B10G11R11_UFLOAT_PACK32: return 4; break;
	case ImageFormat::E5B9G9R9_UFLOAT_PACK32: return 4; break;
	case ImageFormat::D16_UNORM: return 2; break;
	case ImageFormat::X8_D24_UNORM_PACK32: return 4; break;
	case ImageFormat::D32_SFLOAT: return 4; break;
	case ImageFormat::S8_UINT: return 1; break;
	case ImageFormat::D16_UNORM_S8_UINT: return 3; break;
	case ImageFormat::D24_UNORM_S8_UINT: return 4; break;
	case ImageFormat::D32_SFLOAT_S8_UINT: return 5; break;
	case ImageFormat::BC1_RGB_UNORM_BLOCK: return 8; break;
	case ImageFormat::BC1_RGB_SRGB_BLOCK: return 8; break;
	case ImageFormat::BC1_RGBA_UNORM_BLOCK: return 8; break;
	case ImageFormat::BC1_RGBA_SRGB_BLOCK: return 8; break;
	case ImageFormat::BC2_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC2_SRGB_BLOCK: return 16; break;
	case ImageFormat::BC3_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC3_SRGB_BLOCK: return 16; break;
	case ImageFormat::BC4_UNORM_BLOCK: return 8; break;
	case ImageFormat::BC4_SNORM_BLOCK: return 8; break;
	case ImageFormat::BC5_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC5_SNORM_BLOCK: return 16; break;
	case ImageFormat::BC6H_UFLOAT_BLOCK: return 16; break;
	case ImageFormat::BC6H_SFLOAT_BLOCK: return 16; break;
	case ImageFormat::BC7_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC7_SRGB_BLOCK: return 16; break;
	case ImageFormat::ETC2_R8G8B8_UNORM_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8_SRGB_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A1_SRGB_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return 8; break;
	case ImageFormat::EAC_R11_UNORM_BLOCK: return 8; break;
	case ImageFormat::EAC_R11_SNORM_BLOCK: return 8; break;
	case ImageFormat::EAC_R11G11_UNORM_BLOCK: return 16; break;
	case ImageFormat::EAC_R11G11_SNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_4x4_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_4x4_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x4_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x4_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x6_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x6_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x6_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x6_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x8_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x8_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x6_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x6_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x8_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x8_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x10_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x10_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x10_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x10_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x12_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x12_SRGB_BLOCK: return 16; break;
	case ImageFormat::G8B8G8R8_422_UNORM: return 4; break;
	case ImageFormat::B8G8R8G8_422_UNORM: return 4; break;
	case ImageFormat::G8_B8_R8_3PLANE_420_UNORM: return 3; break;
	case ImageFormat::G8_B8R8_2PLANE_420_UNORM: return 3; break;
	case ImageFormat::G8_B8_R8_3PLANE_422_UNORM: return 3; break;
	case ImageFormat::G8_B8R8_2PLANE_422_UNORM: return 3; break;
	case ImageFormat::G8_B8_R8_3PLANE_444_UNORM: return 3; break;
	case ImageFormat::R10X6_UNORM_PACK16: return 2; break;
	case ImageFormat::R10X6G10X6_UNORM_2PACK16: return 4; break;
	case ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16: return 8; break;
	case ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return 6; break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return 4; break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return 6; break;
	case ImageFormat::R12X4_UNORM_PACK16: return 2; break;
	case ImageFormat::R12X4G12X4_UNORM_2PACK16: return 4; break;
	case ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16: return 8; break;
	case ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return 6; break;
	case ImageFormat::G16B16G16R16_422_UNORM: return 8; break;
	case ImageFormat::B16G16R16G16_422_UNORM: return 8; break;
	case ImageFormat::G16_B16_R16_3PLANE_420_UNORM: return 6; break;
	case ImageFormat::G16_B16R16_2PLANE_420_UNORM: return 6; break;
	case ImageFormat::G16_B16_R16_3PLANE_422_UNORM: return 6; break;
	case ImageFormat::G16_B16R16_2PLANE_422_UNORM: return 6; break;
	case ImageFormat::G16_B16_R16_3PLANE_444_UNORM: return 6; break;
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_5x4_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_5x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_6x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_6x6_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_8x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_8x6_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_8x8_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x6_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x8_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x10_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_12x10_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::G8_B8R8_2PLANE_444_UNORM_EXT: return 3; break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case ImageFormat::G16_B16R16_2PLANE_444_UNORM_EXT: return 6; break;
	case ImageFormat::A4R4G4B4_UNORM_PACK16_EXT: return 2; break;
	case ImageFormat::A4B4G4R4_UNORM_PACK16_EXT: return 2; break;
	}
	return 0;
}

PixelFormat getImagePixelFormat(ImageFormat format) {
	switch (format) {
	case ImageFormat::Undefined: return PixelFormat::Unknown; break;

	case ImageFormat::R8_UNORM:
	case ImageFormat::R8_SNORM:
	case ImageFormat::R8_USCALED:
	case ImageFormat::R8_SSCALED:
	case ImageFormat::R8_UINT:
	case ImageFormat::R8_SINT:
	case ImageFormat::R8_SRGB:
	case ImageFormat::R16_UNORM:
	case ImageFormat::R16_SNORM:
	case ImageFormat::R16_USCALED:
	case ImageFormat::R16_SSCALED:
	case ImageFormat::R16_UINT:
	case ImageFormat::R16_SINT:
	case ImageFormat::R16_SFLOAT:
	case ImageFormat::R32_UINT:
	case ImageFormat::R32_SINT:
	case ImageFormat::R32_SFLOAT:
	case ImageFormat::R64_UINT:
	case ImageFormat::R64_SINT:
	case ImageFormat::R64_SFLOAT:
	case ImageFormat::EAC_R11_UNORM_BLOCK:
	case ImageFormat::EAC_R11_SNORM_BLOCK:
	case ImageFormat::R10X6_UNORM_PACK16:
	case ImageFormat::R12X4_UNORM_PACK16: return PixelFormat::A; break;

	case ImageFormat::R4G4_UNORM_PACK8:
	case ImageFormat::R8G8_UNORM:
	case ImageFormat::R8G8_SNORM:
	case ImageFormat::R8G8_USCALED:
	case ImageFormat::R8G8_SSCALED:
	case ImageFormat::R8G8_UINT:
	case ImageFormat::R8G8_SINT:
	case ImageFormat::R8G8_SRGB:
	case ImageFormat::R16G16_UNORM:
	case ImageFormat::R16G16_SNORM:
	case ImageFormat::R16G16_USCALED:
	case ImageFormat::R16G16_SSCALED:
	case ImageFormat::R16G16_UINT:
	case ImageFormat::R16G16_SINT:
	case ImageFormat::R16G16_SFLOAT:
	case ImageFormat::R32G32_UINT:
	case ImageFormat::R32G32_SINT:
	case ImageFormat::R32G32_SFLOAT:
	case ImageFormat::R64G64_UINT:
	case ImageFormat::R64G64_SINT:
	case ImageFormat::R64G64_SFLOAT:
	case ImageFormat::EAC_R11G11_UNORM_BLOCK:
	case ImageFormat::EAC_R11G11_SNORM_BLOCK:
	case ImageFormat::R10X6G10X6_UNORM_2PACK16:
	case ImageFormat::R12X4G12X4_UNORM_2PACK16: return PixelFormat::IA; break;

	case ImageFormat::R4G4B4A4_UNORM_PACK16:
	case ImageFormat::B4G4R4A4_UNORM_PACK16:
	case ImageFormat::R5G5B5A1_UNORM_PACK16:
	case ImageFormat::B5G5R5A1_UNORM_PACK16:
	case ImageFormat::A1R5G5B5_UNORM_PACK16:
	case ImageFormat::R8G8B8A8_UNORM:
	case ImageFormat::R8G8B8A8_SNORM:
	case ImageFormat::R8G8B8A8_USCALED:
	case ImageFormat::R8G8B8A8_SSCALED:
	case ImageFormat::R8G8B8A8_UINT:
	case ImageFormat::R8G8B8A8_SINT:
	case ImageFormat::R8G8B8A8_SRGB:
	case ImageFormat::B8G8R8A8_UNORM:
	case ImageFormat::B8G8R8A8_SNORM:
	case ImageFormat::B8G8R8A8_USCALED:
	case ImageFormat::B8G8R8A8_SSCALED:
	case ImageFormat::B8G8R8A8_UINT:
	case ImageFormat::B8G8R8A8_SINT:
	case ImageFormat::B8G8R8A8_SRGB:
	case ImageFormat::A8B8G8R8_UNORM_PACK32:
	case ImageFormat::A8B8G8R8_SNORM_PACK32:
	case ImageFormat::A8B8G8R8_USCALED_PACK32:
	case ImageFormat::A8B8G8R8_SSCALED_PACK32:
	case ImageFormat::A8B8G8R8_UINT_PACK32:
	case ImageFormat::A8B8G8R8_SINT_PACK32:
	case ImageFormat::A8B8G8R8_SRGB_PACK32:
	case ImageFormat::A2R10G10B10_UNORM_PACK32:
	case ImageFormat::A2R10G10B10_SNORM_PACK32:
	case ImageFormat::A2R10G10B10_USCALED_PACK32:
	case ImageFormat::A2R10G10B10_SSCALED_PACK32:
	case ImageFormat::A2R10G10B10_UINT_PACK32:
	case ImageFormat::A2R10G10B10_SINT_PACK32:
	case ImageFormat::A2B10G10R10_UNORM_PACK32:
	case ImageFormat::A2B10G10R10_SNORM_PACK32:
	case ImageFormat::A2B10G10R10_USCALED_PACK32:
	case ImageFormat::A2B10G10R10_SSCALED_PACK32:
	case ImageFormat::A2B10G10R10_UINT_PACK32:
	case ImageFormat::A2B10G10R10_SINT_PACK32:
	case ImageFormat::R16G16B16A16_UNORM:
	case ImageFormat::R16G16B16A16_SNORM:
	case ImageFormat::R16G16B16A16_USCALED:
	case ImageFormat::R16G16B16A16_SSCALED:
	case ImageFormat::R16G16B16A16_UINT:
	case ImageFormat::R16G16B16A16_SINT:
	case ImageFormat::R16G16B16A16_SFLOAT:
	case ImageFormat::R32G32B32A32_UINT:
	case ImageFormat::R32G32B32A32_SINT:
	case ImageFormat::R32G32B32A32_SFLOAT:
	case ImageFormat::R64G64B64A64_UINT:
	case ImageFormat::R64G64B64A64_SINT:
	case ImageFormat::R64G64B64A64_SFLOAT:
	case ImageFormat::BC1_RGBA_UNORM_BLOCK:
	case ImageFormat::BC1_RGBA_SRGB_BLOCK:
	case ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK:
	case ImageFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
	case ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK:
	case ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
	case ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16:
	case ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16:
	case ImageFormat::A4R4G4B4_UNORM_PACK16_EXT:
	case ImageFormat::A4B4G4R4_UNORM_PACK16_EXT: return PixelFormat::RGBA; break;

	case ImageFormat::R5G6B5_UNORM_PACK16:
	case ImageFormat::B5G6R5_UNORM_PACK16:
	case ImageFormat::R8G8B8_UNORM:
	case ImageFormat::R8G8B8_SNORM:
	case ImageFormat::R8G8B8_USCALED:
	case ImageFormat::R8G8B8_SSCALED:
	case ImageFormat::R8G8B8_UINT:
	case ImageFormat::R8G8B8_SINT:
	case ImageFormat::R8G8B8_SRGB:
	case ImageFormat::B8G8R8_UNORM:
	case ImageFormat::B8G8R8_SNORM:
	case ImageFormat::B8G8R8_USCALED:
	case ImageFormat::B8G8R8_SSCALED:
	case ImageFormat::B8G8R8_UINT:
	case ImageFormat::B8G8R8_SINT:
	case ImageFormat::B8G8R8_SRGB:
	case ImageFormat::R16G16B16_UNORM:
	case ImageFormat::R16G16B16_SNORM:
	case ImageFormat::R16G16B16_USCALED:
	case ImageFormat::R16G16B16_SSCALED:
	case ImageFormat::R16G16B16_UINT:
	case ImageFormat::R16G16B16_SINT:
	case ImageFormat::R16G16B16_SFLOAT:
	case ImageFormat::R32G32B32_UINT:
	case ImageFormat::R32G32B32_SINT:
	case ImageFormat::R32G32B32_SFLOAT:
	case ImageFormat::R64G64B64_UINT:
	case ImageFormat::R64G64B64_SINT:
	case ImageFormat::R64G64B64_SFLOAT:
	case ImageFormat::B10G11R11_UFLOAT_PACK32:
	case ImageFormat::G8B8G8R8_422_UNORM:
	case ImageFormat::B8G8R8G8_422_UNORM:
	case ImageFormat::BC1_RGB_UNORM_BLOCK:
	case ImageFormat::BC1_RGB_SRGB_BLOCK:
	case ImageFormat::ETC2_R8G8B8_UNORM_BLOCK:
	case ImageFormat::ETC2_R8G8B8_SRGB_BLOCK:
	case ImageFormat::G8_B8_R8_3PLANE_420_UNORM:
	case ImageFormat::G8_B8R8_2PLANE_420_UNORM:
	case ImageFormat::G8_B8_R8_3PLANE_422_UNORM:
	case ImageFormat::G8_B8R8_2PLANE_422_UNORM:
	case ImageFormat::G8_B8_R8_3PLANE_444_UNORM:
	case ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
	case ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
	case ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
	case ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
	case ImageFormat::G16B16G16R16_422_UNORM:
	case ImageFormat::B16G16R16G16_422_UNORM:
	case ImageFormat::G16_B16_R16_3PLANE_420_UNORM:
	case ImageFormat::G16_B16R16_2PLANE_420_UNORM:
	case ImageFormat::G16_B16_R16_3PLANE_422_UNORM:
	case ImageFormat::G16_B16R16_2PLANE_422_UNORM:
	case ImageFormat::G16_B16_R16_3PLANE_444_UNORM:
	case ImageFormat::G8_B8R8_2PLANE_444_UNORM_EXT:
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
	case ImageFormat::G16_B16R16_2PLANE_444_UNORM_EXT: return PixelFormat::RGB; break;

	case ImageFormat::D16_UNORM:
	case ImageFormat::D32_SFLOAT:
	case ImageFormat::X8_D24_UNORM_PACK32: return PixelFormat::D; break;

	case ImageFormat::S8_UINT: return PixelFormat::S; break;

	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT: return PixelFormat::DS; break;

	case ImageFormat::E5B9G9R9_UFLOAT_PACK32:
	case ImageFormat::BC2_UNORM_BLOCK:
	case ImageFormat::BC2_SRGB_BLOCK:
	case ImageFormat::BC3_UNORM_BLOCK:
	case ImageFormat::BC3_SRGB_BLOCK:
	case ImageFormat::BC4_UNORM_BLOCK:
	case ImageFormat::BC4_SNORM_BLOCK:
	case ImageFormat::BC5_UNORM_BLOCK:
	case ImageFormat::BC5_SNORM_BLOCK:
	case ImageFormat::BC6H_UFLOAT_BLOCK:
	case ImageFormat::BC6H_SFLOAT_BLOCK:
	case ImageFormat::BC7_UNORM_BLOCK:
	case ImageFormat::BC7_SRGB_BLOCK:
	case ImageFormat::ASTC_4x4_UNORM_BLOCK:
	case ImageFormat::ASTC_4x4_SRGB_BLOCK:
	case ImageFormat::ASTC_5x4_UNORM_BLOCK:
	case ImageFormat::ASTC_5x4_SRGB_BLOCK:
	case ImageFormat::ASTC_5x5_UNORM_BLOCK:
	case ImageFormat::ASTC_5x5_SRGB_BLOCK:
	case ImageFormat::ASTC_6x5_UNORM_BLOCK:
	case ImageFormat::ASTC_6x5_SRGB_BLOCK:
	case ImageFormat::ASTC_6x6_UNORM_BLOCK:
	case ImageFormat::ASTC_6x6_SRGB_BLOCK:
	case ImageFormat::ASTC_8x5_UNORM_BLOCK:
	case ImageFormat::ASTC_8x5_SRGB_BLOCK:
	case ImageFormat::ASTC_8x6_UNORM_BLOCK:
	case ImageFormat::ASTC_8x6_SRGB_BLOCK:
	case ImageFormat::ASTC_8x8_UNORM_BLOCK:
	case ImageFormat::ASTC_8x8_SRGB_BLOCK:
	case ImageFormat::ASTC_10x5_UNORM_BLOCK:
	case ImageFormat::ASTC_10x5_SRGB_BLOCK:
	case ImageFormat::ASTC_10x6_UNORM_BLOCK:
	case ImageFormat::ASTC_10x6_SRGB_BLOCK:
	case ImageFormat::ASTC_10x8_UNORM_BLOCK:
	case ImageFormat::ASTC_10x8_SRGB_BLOCK:
	case ImageFormat::ASTC_10x10_UNORM_BLOCK:
	case ImageFormat::ASTC_10x10_SRGB_BLOCK:
	case ImageFormat::ASTC_12x10_UNORM_BLOCK:
	case ImageFormat::ASTC_12x10_SRGB_BLOCK:
	case ImageFormat::ASTC_12x12_UNORM_BLOCK:
	case ImageFormat::ASTC_12x12_SRGB_BLOCK:
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG:
	case ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_5x4_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_5x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_6x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_6x6_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_8x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_8x6_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_8x8_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x6_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x8_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x10_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_12x10_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT: return PixelFormat::Unknown; break;
	}
	return PixelFormat::Unknown;
}

bool isStencilFormat(ImageFormat format) {
	switch (format) {
	case ImageFormat::S8_UINT:
	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT: return true; break;
	default: break;
	}
	return false;
}

bool isDepthFormat(ImageFormat format) {
	switch (format) {
	case ImageFormat::D16_UNORM:
	case ImageFormat::D32_SFLOAT:
	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT:
	case ImageFormat::X8_D24_UNORM_PACK32: return true; break;
	default: break;
	}
	return false;
}

ImageViewType getImageViewType(ImageType imageType, ArrayLayers arrayLayers) {
	switch (imageType) {
	case ImageType::Image1D:
		if (arrayLayers.get() > 1 && arrayLayers != ArrayLayers::max()) {
			return ImageViewType::ImageView1DArray;
		} else {
			return ImageViewType::ImageView1D;
		}
		break;
	case ImageType::Image2D:
		if (arrayLayers.get() > 1 && arrayLayers != ArrayLayers::max()) {
			return ImageViewType::ImageView2DArray;
		} else {
			return ImageViewType::ImageView2D;
		}
		break;
	case ImageType::Image3D: return ImageViewType::ImageView3D; break;
	}
	return ImageViewType::ImageView2D;
}

bool hasReadAccess(AccessType access) {
	if ((access
				& (AccessType::IndirectCommandRead | AccessType::IndexRead
						| AccessType::VertexAttributeRead | AccessType::UniformRead
						| AccessType::InputAttachmantRead | AccessType::ShaderRead
						| AccessType::ColorAttachmentRead | AccessType::DepthStencilAttachmentRead
						| AccessType::TransferRead | AccessType::HostRead | AccessType::MemoryRead
						| AccessType::ColorAttachmentReadNonCoherent
						| AccessType::TransformFeedbackCounterRead
						| AccessType::ConditionalRenderingRead
						| AccessType::AccelerationStructureRead | AccessType::ShadingRateImageRead
						| AccessType::FragmentDensityMapRead | AccessType::CommandPreprocessRead))
			!= AccessType::None) {
		return true;
	}
	return false;
}

bool hasWriteAccess(AccessType access) {
	if ((access
				& (AccessType::ShaderWrite | AccessType::ColorAttachmentWrite
						| AccessType::DepthStencilAttachmentWrite | AccessType::TransferWrite
						| AccessType::HostWrite | AccessType::MemoryWrite
						| AccessType::TransformFeedbackWrite
						| AccessType::TransformFeedbackCounterWrite
						| AccessType::AccelerationStructureWrite
						| AccessType::CommandPreprocessWrite))
			!= AccessType::None) {
		return true;
	}
	return false;
}

String getQueueFlagsDesc(QueueFlags flags) {
	StringStream stream;
	if ((flags & QueueFlags::Graphics) != QueueFlags::None) {
		stream << " Graphics";
	}
	if ((flags & QueueFlags::Compute) != QueueFlags::None) {
		stream << " Compute";
	}
	if ((flags & QueueFlags::Transfer) != QueueFlags::None) {
		stream << " Transfer";
	}
	if ((flags & QueueFlags::SparceBinding) != QueueFlags::None) {
		stream << " SparceBinding";
	}
	if ((flags & QueueFlags::Protected) != QueueFlags::None) {
		stream << " Protected";
	}
	if ((flags & QueueFlags::VideoDecode) != QueueFlags::None) {
		stream << " VideoDecode";
	}
	if ((flags & QueueFlags::VideoEncode) != QueueFlags::None) {
		stream << " VideoEncode";
	}
	if ((flags & QueueFlags::Present) != QueueFlags::None) {
		stream << " Present";
	}
	return stream.str();
}

Bitmap getBitmap(const ImageInfoData &info, BytesView bytes) {
	if (!bytes.empty()) {
		auto fmt = core::getImagePixelFormat(info.format);
		bitmap::PixelFormat pixelFormat = bitmap::PixelFormat::Auto;
		switch (fmt) {
		case core::PixelFormat::A: pixelFormat = bitmap::PixelFormat::A8; break;
		case core::PixelFormat::IA: pixelFormat = bitmap::PixelFormat::IA88; break;
		case core::PixelFormat::RGB: pixelFormat = bitmap::PixelFormat::RGB888; break;
		case core::PixelFormat::RGBA: pixelFormat = bitmap::PixelFormat::RGBA8888; break;
		default: break;
		}

		bitmap::getBytesPerPixel(pixelFormat);

		auto requiredSize = bitmap::getBytesPerPixel(pixelFormat) * info.extent.width
				* info.extent.height * info.extent.depth * info.arrayLayers.get();

		if (pixelFormat != bitmap::PixelFormat::Auto && requiredSize == bytes.size()) {
			return Bitmap(bytes.data(), info.extent.width, info.extent.height, pixelFormat);
		}
	}
	return Bitmap();
}

bool saveImage(const FileInfo &file, const ImageInfoData &info, BytesView bytes) {
	if (auto bmp = getBitmap(info, bytes)) {
		return bmp.save(file);
	}
	return false;
}

std::ostream &operator<<(std::ostream &stream, const ImageInfoData &value) {
	stream << "ImageInfoData: " << value.extent << " Layers:" << value.arrayLayers.get();
	return stream;
}

String PipelineMaterialInfo::data() const {
	BytesView view(reinterpret_cast<const uint8_t *>(this), sizeof(PipelineMaterialInfo));
	return toString(base16::encode<Interface>(view.sub(0, sizeof(BlendInfo))), "'",
			base16::encode<Interface>(view.sub(sizeof(BlendInfo), sizeof(DepthInfo))), "'",
			base16::encode<Interface>(
					view.sub(sizeof(BlendInfo) + sizeof(DepthInfo), sizeof(DepthBounds))),
			"'",
			base16::encode<Interface>(
					view.sub(sizeof(BlendInfo) + sizeof(DepthInfo) + sizeof(DepthBounds),
							sizeof(StencilInfo))),
			"'",
			base16::encode<Interface>(view.sub(sizeof(BlendInfo) + sizeof(DepthInfo)
							+ sizeof(DepthBounds) + sizeof(StencilInfo),
					sizeof(StencilInfo))),
			"'",
			base16::encode<Interface>(view.sub(sizeof(BlendInfo) + sizeof(DepthInfo)
					+ sizeof(DepthBounds) + sizeof(StencilInfo) * 2)));
}

String PipelineMaterialInfo::description() const {
	StringStream stream;
	stream << "{" << blend.enabled << "," << blend.srcColor << "," << blend.dstColor << ","
		   << blend.opColor << "," << blend.srcAlpha << "," << blend.dstAlpha << ","
		   << blend.opAlpha << "," << blend.writeMask << "},{" << depth.writeEnabled << ","
		   << depth.testEnabled << "," << depth.compare << "},{" << bounds.enabled << ","
		   << bounds.min << "," << bounds.max << "},{" << stencil << "}";
	return stream.str();
}

PipelineMaterialInfo::PipelineMaterialInfo() { memset(this, 0, sizeof(PipelineMaterialInfo)); }

void PipelineMaterialInfo::setBlendInfo(const BlendInfo &info) {
	if (info.isEnabled()) {
		blend = info;
	} else {
		blend = BlendInfo();
		blend.writeMask = info.writeMask;
	}
}

void PipelineMaterialInfo::setDepthInfo(const DepthInfo &info) {
	if (info.testEnabled) {
		depth.testEnabled = 1;
		depth.compare = info.compare;
	} else {
		depth.testEnabled = 0;
		depth.compare = 0;
	}
	if (info.writeEnabled) {
		depth.writeEnabled = 1;
	} else {
		depth.writeEnabled = 0;
	}
}

void PipelineMaterialInfo::setDepthBounds(const DepthBounds &b) {
	if (b.enabled) {
		bounds = b;
	} else {
		bounds = DepthBounds();
	}
}

void PipelineMaterialInfo::enableStencil(const StencilInfo &info) {
	stencil = 1;
	front = info;
	back = info;
}

void PipelineMaterialInfo::enableStencil(const StencilInfo &f, const StencilInfo &b) {
	stencil = 1;
	front = f;
	back = b;
}

void PipelineMaterialInfo::disableStancil() {
	stencil = 0;
	memset(&front, 0, sizeof(StencilInfo));
	memset(&back, 0, sizeof(StencilInfo));
}

void PipelineMaterialInfo::setLineWidth(float width) {
	if (width == 0.0f) {
		memset(&lineWidth, 0, sizeof(float));
	} else {
		lineWidth = width;
	}
}

void PipelineMaterialInfo::setImageViewType(ImageViewType type) { imageViewType = type; }

void PipelineMaterialInfo::_setup(const BlendInfo &info) { setBlendInfo(info); }

void PipelineMaterialInfo::_setup(const DepthInfo &info) { setDepthInfo(info); }

void PipelineMaterialInfo::_setup(const DepthBounds &b) { setDepthBounds(b); }

void PipelineMaterialInfo::_setup(const StencilInfo &info) { enableStencil(info); }

void PipelineMaterialInfo::_setup(LineWidth width) { setLineWidth(width.get()); }

void PipelineMaterialInfo::_setup(ImageViewType type) { setImageViewType(type); }

StringView getInputKeyCodeName(InputKeyCode code) {
	switch (code) {
	case InputKeyCode::Unknown: return StringView("Unknown"); break;
	case InputKeyCode::KP_DECIMAL: return StringView("KP_DECIMAL"); break;
	case InputKeyCode::KP_DIVIDE: return StringView("KP_DIVIDE"); break;
	case InputKeyCode::KP_MULTIPLY: return StringView("KP_MULTIPLY"); break;
	case InputKeyCode::KP_SUBTRACT: return StringView("KP_SUBTRACT"); break;
	case InputKeyCode::KP_ADD: return StringView("KP_ADD"); break;
	case InputKeyCode::KP_ENTER: return StringView("KP_ENTER"); break;
	case InputKeyCode::KP_EQUAL: return StringView("KP_EQUAL"); break;

	case InputKeyCode::BACKSPACE: return StringView("BACKSPACE"); break;
	case InputKeyCode::TAB: return StringView("TAB"); break;
	case InputKeyCode::ENTER: return StringView("ENTER"); break;

	case InputKeyCode::RIGHT: return StringView("RIGHT"); break;
	case InputKeyCode::LEFT: return StringView("LEFT"); break;
	case InputKeyCode::DOWN: return StringView("DOWN"); break;
	case InputKeyCode::UP: return StringView("UP"); break;
	case InputKeyCode::PAGE_UP: return StringView("PAGE_UP"); break;
	case InputKeyCode::PAGE_DOWN: return StringView("PAGE_DOWN"); break;
	case InputKeyCode::HOME: return StringView("HOME"); break;
	case InputKeyCode::END: return StringView("END"); break;
	case InputKeyCode::LEFT_SHIFT: return StringView("LEFT_SHIFT"); break;
	case InputKeyCode::LEFT_CONTROL: return StringView("LEFT_CONTROL"); break;
	case InputKeyCode::LEFT_ALT: return StringView("LEFT_ALT"); break;
	case InputKeyCode::LEFT_SUPER: return StringView("LEFT_SUPER"); break;
	case InputKeyCode::RIGHT_SHIFT: return StringView("RIGHT_SHIFT"); break;
	case InputKeyCode::RIGHT_CONTROL: return StringView("RIGHT_CONTROL"); break;
	case InputKeyCode::RIGHT_ALT: return StringView("RIGHT_ALT"); break;
	case InputKeyCode::RIGHT_SUPER: return StringView("RIGHT_SUPER"); break;

	case InputKeyCode::ESCAPE: return StringView("ESCAPE"); break;

	case InputKeyCode::INSERT: return StringView("INSERT"); break;
	case InputKeyCode::CAPS_LOCK: return StringView("CAPS_LOCK"); break;
	case InputKeyCode::SCROLL_LOCK: return StringView("SCROLL_LOCK"); break;
	case InputKeyCode::NUM_LOCK: return StringView("NUM_LOCK"); break;

	case InputKeyCode::SPACE: return StringView("SPACE"); break;

	case InputKeyCode::KP_0: return StringView("KP_0"); break;
	case InputKeyCode::KP_1: return StringView("KP_1"); break;
	case InputKeyCode::KP_2: return StringView("KP_2"); break;
	case InputKeyCode::KP_3: return StringView("KP_3"); break;
	case InputKeyCode::KP_4: return StringView("KP_4"); break;
	case InputKeyCode::KP_5: return StringView("KP_5"); break;
	case InputKeyCode::KP_6: return StringView("KP_6"); break;
	case InputKeyCode::KP_7: return StringView("KP_7"); break;
	case InputKeyCode::KP_8: return StringView("KP_8"); break;
	case InputKeyCode::KP_9: return StringView("KP_9"); break;

	case InputKeyCode::APOSTROPHE: return StringView("APOSTROPHE"); break;
	case InputKeyCode::COMMA: return StringView("COMMA"); break;
	case InputKeyCode::MINUS: return StringView("MINUS"); break;
	case InputKeyCode::PERIOD: return StringView("PERIOD"); break;
	case InputKeyCode::SLASH: return StringView("SLASH"); break;
	case InputKeyCode::_0: return StringView("0"); break;
	case InputKeyCode::_1: return StringView("1"); break;
	case InputKeyCode::_2: return StringView("2"); break;
	case InputKeyCode::_3: return StringView("3"); break;
	case InputKeyCode::_4: return StringView("4"); break;
	case InputKeyCode::_5: return StringView("5"); break;
	case InputKeyCode::_6: return StringView("6"); break;
	case InputKeyCode::_7: return StringView("7"); break;
	case InputKeyCode::_8: return StringView("8"); break;
	case InputKeyCode::_9: return StringView("9"); break;
	case InputKeyCode::SEMICOLON: return StringView("SEMICOLON"); break;
	case InputKeyCode::EQUAL: return StringView("EQUAL"); break;

	case InputKeyCode::WORLD_1: return StringView("WORLD_1"); break;
	case InputKeyCode::WORLD_2: return StringView("WORLD_2"); break;

	case InputKeyCode::A: return StringView("A"); break;
	case InputKeyCode::B: return StringView("B"); break;
	case InputKeyCode::C: return StringView("C"); break;
	case InputKeyCode::D: return StringView("D"); break;
	case InputKeyCode::E: return StringView("E"); break;
	case InputKeyCode::F: return StringView("F"); break;
	case InputKeyCode::G: return StringView("G"); break;
	case InputKeyCode::H: return StringView("H"); break;
	case InputKeyCode::I: return StringView("I"); break;
	case InputKeyCode::J: return StringView("J"); break;
	case InputKeyCode::K: return StringView("K"); break;
	case InputKeyCode::L: return StringView("L"); break;
	case InputKeyCode::M: return StringView("M"); break;
	case InputKeyCode::N: return StringView("N"); break;
	case InputKeyCode::O: return StringView("O"); break;
	case InputKeyCode::P: return StringView("P"); break;
	case InputKeyCode::Q: return StringView("Q"); break;
	case InputKeyCode::R: return StringView("R"); break;
	case InputKeyCode::S: return StringView("S"); break;
	case InputKeyCode::T: return StringView("T"); break;
	case InputKeyCode::U: return StringView("U"); break;
	case InputKeyCode::V: return StringView("V"); break;
	case InputKeyCode::W: return StringView("W"); break;
	case InputKeyCode::X: return StringView("X"); break;
	case InputKeyCode::Y: return StringView("Y"); break;
	case InputKeyCode::Z: return StringView("Z"); break;
	case InputKeyCode::LEFT_BRACKET: return StringView("LEFT_BRACKET"); break;
	case InputKeyCode::BACKSLASH: return StringView("BACKSLASH"); break;
	case InputKeyCode::RIGHT_BRACKET: return StringView("RIGHT_BRACKET"); break;
	case InputKeyCode::GRAVE_ACCENT:
		return StringView("GRAVE_ACCENT");
		break;

		/* Function keys */
	case InputKeyCode::F1: return StringView("F1"); break;
	case InputKeyCode::F2: return StringView("F2"); break;
	case InputKeyCode::F3: return StringView("F3"); break;
	case InputKeyCode::F4: return StringView("F4"); break;
	case InputKeyCode::F5: return StringView("F5"); break;
	case InputKeyCode::F6: return StringView("F6"); break;
	case InputKeyCode::F7: return StringView("F7"); break;
	case InputKeyCode::F8: return StringView("F8"); break;
	case InputKeyCode::F9: return StringView("F9"); break;
	case InputKeyCode::F10: return StringView("F10"); break;
	case InputKeyCode::F11: return StringView("F11"); break;
	case InputKeyCode::F12: return StringView("F12"); break;
	case InputKeyCode::F13: return StringView("F13"); break;
	case InputKeyCode::F14: return StringView("F14"); break;
	case InputKeyCode::F15: return StringView("F15"); break;
	case InputKeyCode::F16: return StringView("F16"); break;
	case InputKeyCode::F17: return StringView("F17"); break;
	case InputKeyCode::F18: return StringView("F18"); break;
	case InputKeyCode::F19: return StringView("F19"); break;
	case InputKeyCode::F20: return StringView("F20"); break;
	case InputKeyCode::F21: return StringView("F21"); break;
	case InputKeyCode::F22: return StringView("F22"); break;
	case InputKeyCode::F23: return StringView("F23"); break;
	case InputKeyCode::F24: return StringView("F24"); break;
	case InputKeyCode::F25: return StringView("F25"); break;

	case InputKeyCode::MENU: return StringView("MENU"); break;
	case InputKeyCode::PRINT_SCREEN: return StringView("PRINT_SCREEN"); break;
	case InputKeyCode::PAUSE: return StringView("PAUSE"); break;
	case InputKeyCode::DELETE: return StringView("DELETE"); break;
	default: break;
	}
	return StringView();
}

StringView getInputKeyCodeKeyName(InputKeyCode code) {
	switch (code) {
	case InputKeyCode::KP_DECIMAL: return StringView("KPDL"); break;
	case InputKeyCode::KP_DIVIDE: return StringView("KPDV"); break;
	case InputKeyCode::KP_MULTIPLY: return StringView("KPMU"); break;
	case InputKeyCode::KP_SUBTRACT: return StringView("KPSU"); break;
	case InputKeyCode::KP_ADD: return StringView("KPAD"); break;
	case InputKeyCode::KP_ENTER: return StringView("KPEN"); break;
	case InputKeyCode::KP_EQUAL: return StringView("KPEQ"); break;

	case InputKeyCode::BACKSPACE: return StringView("BKSP"); break;
	case InputKeyCode::TAB: return StringView("TAB"); break;
	case InputKeyCode::ENTER: return StringView("RTRN"); break;

	case InputKeyCode::RIGHT: return StringView("RGHT"); break;
	case InputKeyCode::LEFT: return StringView("LEFT"); break;
	case InputKeyCode::DOWN: return StringView("DOWN"); break;
	case InputKeyCode::UP: return StringView("UP"); break;
	case InputKeyCode::PAGE_UP: return StringView("PGUP"); break;
	case InputKeyCode::PAGE_DOWN: return StringView("PGDN"); break;
	case InputKeyCode::HOME: return StringView("HOME"); break;
	case InputKeyCode::END: return StringView("END"); break;
	case InputKeyCode::LEFT_SHIFT: return StringView("LFSH"); break;
	case InputKeyCode::LEFT_CONTROL: return StringView("LCTL"); break;
	case InputKeyCode::LEFT_ALT: return StringView("LALT"); break;
	case InputKeyCode::LEFT_SUPER: return StringView("LWIN"); break;
	case InputKeyCode::RIGHT_SHIFT: return StringView("RTSH"); break;
	case InputKeyCode::RIGHT_CONTROL: return StringView("RCTL"); break;
	case InputKeyCode::RIGHT_ALT: return StringView("RALT"); break;
	case InputKeyCode::RIGHT_SUPER: return StringView("RWIN"); break;

	case InputKeyCode::ESCAPE: return StringView("ESC"); break;

	case InputKeyCode::INSERT: return StringView("INS"); break;
	case InputKeyCode::CAPS_LOCK: return StringView("CAPS"); break;
	case InputKeyCode::SCROLL_LOCK: return StringView("SCLK"); break;
	case InputKeyCode::NUM_LOCK: return StringView("NMLK"); break;

	case InputKeyCode::SPACE: return StringView("SPCE"); break;

	case InputKeyCode::KP_0: return StringView("KP0"); break;
	case InputKeyCode::KP_1: return StringView("KP1"); break;
	case InputKeyCode::KP_2: return StringView("KP2"); break;
	case InputKeyCode::KP_3: return StringView("KP3"); break;
	case InputKeyCode::KP_4: return StringView("KP4"); break;
	case InputKeyCode::KP_5: return StringView("KP5"); break;
	case InputKeyCode::KP_6: return StringView("KP6"); break;
	case InputKeyCode::KP_7: return StringView("KP7"); break;
	case InputKeyCode::KP_8: return StringView("KP8"); break;
	case InputKeyCode::KP_9: return StringView("KP9"); break;

	case InputKeyCode::APOSTROPHE: return StringView("AC11"); break;
	case InputKeyCode::COMMA: return StringView("AB08"); break;
	case InputKeyCode::MINUS: return StringView("AE11"); break;
	case InputKeyCode::PERIOD: return StringView("AB09"); break;
	case InputKeyCode::SLASH: return StringView("AB10"); break;
	case InputKeyCode::_0: return StringView("AE10"); break;
	case InputKeyCode::_1: return StringView("AE01"); break;
	case InputKeyCode::_2: return StringView("AE02"); break;
	case InputKeyCode::_3: return StringView("AE03"); break;
	case InputKeyCode::_4: return StringView("AE04"); break;
	case InputKeyCode::_5: return StringView("AE05"); break;
	case InputKeyCode::_6: return StringView("AE06"); break;
	case InputKeyCode::_7: return StringView("AE07"); break;
	case InputKeyCode::_8: return StringView("AE08"); break;
	case InputKeyCode::_9: return StringView("AE09"); break;
	case InputKeyCode::SEMICOLON: return StringView("AC10"); break;
	case InputKeyCode::EQUAL: return StringView("AE12"); break;

	case InputKeyCode::WORLD_1: return StringView("LSGT"); break;

	case InputKeyCode::A: return StringView("AC01"); break;
	case InputKeyCode::B: return StringView("AB05"); break;
	case InputKeyCode::C: return StringView("AB03"); break;
	case InputKeyCode::D: return StringView("AC03"); break;
	case InputKeyCode::E: return StringView("AD03"); break;
	case InputKeyCode::F: return StringView("AC04"); break;
	case InputKeyCode::G: return StringView("AC05"); break;
	case InputKeyCode::H: return StringView("AC06"); break;
	case InputKeyCode::I: return StringView("AD08"); break;
	case InputKeyCode::J: return StringView("AC07"); break;
	case InputKeyCode::K: return StringView("AC08"); break;
	case InputKeyCode::L: return StringView("AC09"); break;
	case InputKeyCode::M: return StringView("AB07"); break;
	case InputKeyCode::N: return StringView("AB06"); break;
	case InputKeyCode::O: return StringView("AD09"); break;
	case InputKeyCode::P: return StringView("AD10"); break;
	case InputKeyCode::Q: return StringView("AD01"); break;
	case InputKeyCode::R: return StringView("AD04"); break;
	case InputKeyCode::S: return StringView("AC02"); break;
	case InputKeyCode::T: return StringView("AD05"); break;
	case InputKeyCode::U: return StringView("AD07"); break;
	case InputKeyCode::V: return StringView("AB04"); break;
	case InputKeyCode::W: return StringView("AD02"); break;
	case InputKeyCode::X: return StringView("AB02"); break;
	case InputKeyCode::Y: return StringView("AD06"); break;
	case InputKeyCode::Z: return StringView("AB01"); break;
	case InputKeyCode::LEFT_BRACKET: return StringView("AD11"); break;
	case InputKeyCode::BACKSLASH: return StringView("BKSL"); break;
	case InputKeyCode::RIGHT_BRACKET: return StringView("AD12"); break;
	case InputKeyCode::GRAVE_ACCENT: return StringView("TLDE"); break;

	// Function keys
	case InputKeyCode::F1: return StringView("FK01"); break;
	case InputKeyCode::F2: return StringView("FK02"); break;
	case InputKeyCode::F3: return StringView("FK03"); break;
	case InputKeyCode::F4: return StringView("FK04"); break;
	case InputKeyCode::F5: return StringView("FK05"); break;
	case InputKeyCode::F6: return StringView("FK06"); break;
	case InputKeyCode::F7: return StringView("FK07"); break;
	case InputKeyCode::F8: return StringView("FK08"); break;
	case InputKeyCode::F9: return StringView("FK09"); break;
	case InputKeyCode::F10: return StringView("FK10"); break;
	case InputKeyCode::F11: return StringView("FK11"); break;
	case InputKeyCode::F12: return StringView("FK12"); break;
	case InputKeyCode::F13: return StringView("FK13"); break;
	case InputKeyCode::F14: return StringView("FK14"); break;
	case InputKeyCode::F15: return StringView("FK15"); break;
	case InputKeyCode::F16: return StringView("FK16"); break;
	case InputKeyCode::F17: return StringView("FK17"); break;
	case InputKeyCode::F18: return StringView("FK18"); break;
	case InputKeyCode::F19: return StringView("FK19"); break;
	case InputKeyCode::F20: return StringView("FK20"); break;
	case InputKeyCode::F21: return StringView("FK21"); break;
	case InputKeyCode::F22: return StringView("FK22"); break;
	case InputKeyCode::F23: return StringView("FK23"); break;
	case InputKeyCode::F24: return StringView("FK24"); break;
	case InputKeyCode::F25: return StringView("FK25"); break;

	case InputKeyCode::MENU: return StringView("MENU"); break;
	case InputKeyCode::PRINT_SCREEN: return StringView("PRSC"); break;
	case InputKeyCode::PAUSE: return StringView("PAUS"); break;
	case InputKeyCode::DELETE: return StringView("DELE"); break;
	default: break;
	}
	return StringView();
}

StringView getInputEventName(InputEventName name) {
	switch (name) {
	case InputEventName::None: return StringView("None"); break;
	case InputEventName::Begin: return StringView("Begin"); break;
	case InputEventName::Move: return StringView("Move"); break;
	case InputEventName::End: return StringView("End"); break;
	case InputEventName::Cancel: return StringView("Cancel"); break;
	case InputEventName::MouseMove: return StringView("MouseMove"); break;
	case InputEventName::Scroll: return StringView("Scroll"); break;

	case InputEventName::KeyPressed: return StringView("KeyPressed"); break;
	case InputEventName::KeyRepeated: return StringView("KeyRepeated"); break;
	case InputEventName::KeyReleased: return StringView("KeyReleased"); break;
	case InputEventName::KeyCanceled: return StringView("KeyCanceled"); break;
	case InputEventName::WindowState: return StringView("WindowState"); break;
	case InputEventName::Max: break;
	}
	return StringView();
}

StringView getInputButtonName(InputMouseButton btn) {
	switch (btn) {
	case InputMouseButton::MouseLeft: return StringView("MouseLeft"); break;
	case InputMouseButton::MouseMiddle: return StringView("MouseMiddle"); break;
	case InputMouseButton::MouseRight: return StringView("MouseRight"); break;
	case InputMouseButton::MouseScrollUp: return StringView("MouseScrollUp"); break;
	case InputMouseButton::MouseScrollDown: return StringView("MouseScrollDown"); break;
	case InputMouseButton::MouseScrollLeft: return StringView("MouseScrollLeft"); break;
	case InputMouseButton::MouseScrollRight: return StringView("MouseScrollRight"); break;
	case InputMouseButton::Mouse8: return StringView("Mouse8"); break;
	case InputMouseButton::Mouse9: return StringView("Mouse9"); break;
	case InputMouseButton::Mouse10: return StringView("Mouse10"); break;
	case InputMouseButton::Mouse11: return StringView("Mouse11"); break;
	case InputMouseButton::Mouse12: return StringView("Mouse12"); break;
	case InputMouseButton::Mouse13: return StringView("Mouse13"); break;
	case InputMouseButton::Mouse14: return StringView("Mouse14"); break;
	case InputMouseButton::Mouse15: return StringView("Mouse15"); break;
	default: break;
	}
	return StringView();
}

String getInputModifiersNames(InputModifier mod) {
	StringStream out;
	if ((mod & InputModifier::Shift) != InputModifier::None) {
		out << " Shift";
	}
	if ((mod & InputModifier::CapsLock) != InputModifier::None) {
		out << " CapsLock";
	}
	if ((mod & InputModifier::Ctrl) != InputModifier::None) {
		out << " Ctrl";
	}
	if ((mod & InputModifier::Alt) != InputModifier::None) {
		out << " Alt";
	}
	if ((mod & InputModifier::NumLock) != InputModifier::None) {
		out << " NumLock";
	}
	if ((mod & InputModifier::Mod3) != InputModifier::None) {
		out << " Mod3";
	}
	if ((mod & InputModifier::Mod4) != InputModifier::None) {
		out << " Mod4";
	}
	if ((mod & InputModifier::Mod5) != InputModifier::None) {
		out << " Mod5";
	}
	if ((mod & InputModifier::LayoutAlternative) != InputModifier::None) {
		out << " LayoutAlternative";
	}
	if ((mod & InputModifier::ShiftL) != InputModifier::None) {
		out << " ShiftL";
	}
	if ((mod & InputModifier::ShiftR) != InputModifier::None) {
		out << " ShiftR";
	}
	if ((mod & InputModifier::CtrlL) != InputModifier::None) {
		out << " CtrlL";
	}
	if ((mod & InputModifier::CtrlR) != InputModifier::None) {
		out << " CtrlR";
	}
	if ((mod & InputModifier::AltL) != InputModifier::None) {
		out << " AltL";
	}
	if ((mod & InputModifier::AltR) != InputModifier::None) {
		out << " AltR";
	}
	if ((mod & InputModifier::Mod3L) != InputModifier::None) {
		out << " Mod3L";
	}
	if ((mod & InputModifier::Mod3L) != InputModifier::None) {
		out << " Mod3L";
	}
	if ((mod & InputModifier::Mod4L) != InputModifier::None) {
		out << " Mod4L";
	}
	if ((mod & InputModifier::Mod4R) != InputModifier::None) {
		out << " Mod4R";
	}
	if ((mod & InputModifier::ScrollLock) != InputModifier::None) {
		out << " ScrollLock";
	}
	return out.str();
}

} // namespace stappler::xenolith::core

namespace std {

std::ostream &operator<<(std::ostream &stream,
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::InputKeyCode code) {
	stream << "InputKeyCode(" << STAPPLER_VERSIONIZED_NAMESPACE::toInt(code) << ", "
		   << getInputKeyCodeName(code) << ", " << getInputKeyCodeKeyName(code) << ")";
	return stream;
}

std::ostream &operator<<(std::ostream &stream,
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::InputEventName name) {
	stream << "InputEventName("
		   << STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::getInputEventName(name) << ")";
	return stream;
}


} // namespace std
