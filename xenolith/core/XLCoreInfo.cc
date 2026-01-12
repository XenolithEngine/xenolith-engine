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

} // namespace stappler::xenolith::core
