/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include <sprt/runtime/enum.h>
#include <sprt/runtime/stream.h>
#include <sprt/runtime/window/mode.h>
#include <sprt/runtime/window/window_info.h>
#include <sprt/runtime/window/interface.h>
#include <sprt/runtime/log.h>

#include "window/SPRuntimeWindowPnpId.cc"

namespace sprt::window {

static memory::dynstring getManufacturerName(const unsigned char *x) {
	memory::dynstring name;
	name.resize(3);

	name[0] = ((x[0] & 0x7c) >> 2) + '@';
	name[1] = ((x[0] & 0x03) << 3) + ((x[1] & 0xe0) >> 5) + '@';
	name[2] = (x[1] & 0x1f) + '@';

	return name;
}

static memory::dynstring extractString(const unsigned char *x, unsigned len) {
	memory::dynstring ret;
	bool seen_newline = false;
	unsigned i;

	for (i = 0; i < len; i++) {
		// The encoding is cp437, so any character is allowed,
		// but in practice it is unwise to use a non-ASCII character.
		bool non_ascii = (x[i] >= 1 && x[i] < 0x20 && x[i] != 0x0a) || x[i] >= 0x7f;

		if (seen_newline) {
			if (x[i] != 0x20) {
				return ret;
			}
		} else if (x[i] == 0x0a) {
			seen_newline = true;
		} else if (!x[i]) {
			return ret;
		} else if (x[i] == 0xff) {
			return ret;
		} else if (!non_ascii) {
			ret.push_back(x[i]);
		} else {
			ret.push_back('.');
		}
	}
	return ret;
}

static void parseDetailedBlock(const unsigned char *x, EdidInfo &info) {
	static const unsigned char zero_descr[18] = {0};

	if (!__sprt_memcmp(x, zero_descr, sizeof(zero_descr))) {
		return;
	}

	switch (x[3]) {
	case 0xfc: info.model = extractString(x + 5, 13); return;
	case 0xff: info.serial = extractString(x + 5, 13); return;
	default: return;
	}
}

EdidInfo EdidInfo::parse(BytesView data) {
	// std::cout << base16::encode<Interface>(data) << "\n";

	EdidInfo ret;
	auto x = data.data();

	ret.vendorId = getManufacturerName(x + 0x08);
	if (ret.vendorId == "CID") {
		ret.vendorId.clear();
	} else {
		ret.vendor = StringView(pnp_name(ret.vendorId.data()));
		if (ret.vendor.empty()) {
			ret.vendor = ret.vendorId;
		}
	}

	parseDetailedBlock(x + 0x36, ret);
	parseDetailedBlock(x + 0x48, ret);
	parseDetailedBlock(x + 0x5a, ret);
	parseDetailedBlock(x + 0x6c, ret);

	return ret;
}

StringView EdidInfo::getVendorName(StringView data) { return pnp_name(data.data()); }

const ModeInfo ModeInfo::Preferred{Max<uint16_t>, Max<uint16_t>, 0};
const ModeInfo ModeInfo::Current{Max<uint16_t>, Max<uint16_t>, Max<uint16_t>};
const MonitorId MonitorId::Primary{"__primary__"};
const MonitorId MonitorId::None;

const FullscreenInfo FullscreenInfo::None{MonitorId::None, ModeInfo::Current,
	FullscreenFlags::None};

const FullscreenInfo FullscreenInfo::Current{MonitorId::None, ModeInfo::Current,
	FullscreenFlags::Current};

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

void getCompositeAlphaFlagsDescription(const callback<void(StringView)> &stream,
		CompositeAlphaFlags fmt) {
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
}

void getSurfaceTransformFlagsDescription(const callback<void(StringView)> &stream,
		SurfaceTransformFlags fmt) {
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
}

void getImageUsageDescription(const callback<void(StringView)> &stream, ImageUsage fmt) {
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

void SwapchainConfig::description(const callback<void(StringView)> &stream) const {
	stream << "\nSurfaceInfo:\n";
	stream << "\tPresentMode: " << getPresentModeName(presentMode);
	if (presentModeFast != PresentMode::Unsupported) {
		stream << " (" << getPresentModeName(presentModeFast) << ")";
	}
	stream << "\n";
	stream << "\tSurface format: (" << getImageFormatName(imageFormat) << ":"
		   << getColorSpaceName(colorSpace) << ")\n";
	stream << "\tTransform:" << transform << "\n";
	stream << "\tAlpha:" << alpha << "\n";
	stream << "\tImage count: " << imageCount << "\n";
	stream << "\tExtent: " << extent.width << "x" << extent.height << "\n";
}

bool SurfaceInfo::isSupported(const SwapchainConfig &cfg) const {
	if (sprt::find(presentModes.begin(), presentModes.end(), cfg.presentMode)
			== presentModes.end()) {
		log::vperror(__SPRT_LOCATION, "Vk-Error", "SurfaceInfo: presentMode is not supported");
		return false;
	}

	if (cfg.presentModeFast != PresentMode::Unsupported
			&& sprt::find(presentModes.begin(), presentModes.end(), cfg.presentModeFast)
					== presentModes.end()) {
		log::vperror(__SPRT_LOCATION, "Vk-Error", "SurfaceInfo: presentModeFast is not supported");
		return false;
	}

	if (sprt::find(formats.begin(), formats.end(), sprt::pair{cfg.imageFormat, cfg.colorSpace})
			== formats.end()) {
		log::vperror(__SPRT_LOCATION, "Vk-Error",
				"SurfaceInfo: imageFormat or colorSpace is not supported");
		return false;
	}

	if ((supportedCompositeAlpha & cfg.alpha) == CompositeAlphaFlags::None) {
		log::vperror(__SPRT_LOCATION, "Vk-Error", "SurfaceInfo: alpha is not supported");
		return false;
	}

	if ((supportedTransforms & cfg.transform) == SurfaceTransformFlags::None) {
		log::vperror(__SPRT_LOCATION, "Vk-Error", "SurfaceInfo: transform is not supported");
		return false;
	}

	if (cfg.imageCount < minImageCount || (maxImageCount != 0 && cfg.imageCount > maxImageCount)) {
		log::vperror(__SPRT_LOCATION, "Vk-Error", "SurfaceInfo: imageCount is not supported");
		return false;
	}

	if (cfg.extent.width < minImageExtent.width || cfg.extent.width > maxImageExtent.width
			|| cfg.extent.height < minImageExtent.height
			|| cfg.extent.height > maxImageExtent.height) {
		log::vperror(__SPRT_LOCATION, "Vk-Error", "SurfaceInfo: extent is not supported");
		return false;
	}

	if (cfg.transfer && (supportedUsageFlags & ImageUsage::TransferDst) == ImageUsage::None) {
		log::vperror(__SPRT_LOCATION, "Vk-Error",
				"SurfaceInfo: supportedUsageFlags is not supported");
		return false;
	}

	return true;
}

void SurfaceInfo::description(const callback<void(StringView)> &stream) const {
	stream << "\nSurfaceInfo:\n";
	stream << "\tImageCount: " << minImageCount << "-" << maxImageCount << "\n";
	stream << "\tExtent: " << currentExtent.width << "x" << currentExtent.height << " ("
		   << minImageExtent.width << "x" << minImageExtent.height << " - " << maxImageExtent.width
		   << "x" << maxImageExtent.height << ")\n";
	stream << "\tMax Layers: " << maxImageArrayLayers << "\n";

	stream << "\tSupported transforms:" << supportedTransforms << "\n";
	stream << "\tCurrent transforms:" << currentTransform << "\n";
	stream << "\tSupported Alpha:" << supportedCompositeAlpha << "\n";
	stream << "\tSupported Usage:" << supportedUsageFlags << "\n";

	stream << "\tSurface format:";
	for (auto it : formats) {
		stream << " (" << getImageFormatName(it.first) << ":" << getColorSpaceName(it.second)
			   << ")";
	}
	stream << "\n";

	stream << "\tPresent modes:";
	for (auto &it : presentModes) { stream << " " << getPresentModeName(it); }
	stream << "\n";
}

void getWindowStateDescription(const callback<void(StringView)> &out, WindowState f) {
	for (auto value : flags(f)) {
		switch (value) {
		case WindowState::None: break;
		case WindowState::Modal: out << " Modal"; break;
		case WindowState::Sticky: out << " Sticky"; break;
		case WindowState::MaximizedVert: out << " MaximizedVert"; break;
		case WindowState::MaximizedHorz: out << " MaximizedHorz"; break;
		case WindowState::Maximized: break;
		case WindowState::Shaded: out << " Shaded"; break;
		case WindowState::SkipTaskbar: out << " SkipTaskbar"; break;
		case WindowState::SkipPager: out << " SkipPager"; break;
		case WindowState::Minimized: out << " Minimized"; break;
		case WindowState::Fullscreen: out << " Fullscreen"; break;
		case WindowState::Above: out << " Above"; break;
		case WindowState::Below: out << " Below"; break;
		case WindowState::DemandsAttention: out << " DemandsAttention"; break;
		case WindowState::Focused: out << " Focused"; break;
		case WindowState::Resizing: out << " Resizing"; break;
		case WindowState::Pointer: out << " Pointer"; break;
		case WindowState::CloseGuard: out << " CloseGuard"; break;
		case WindowState::CloseRequest: out << " CloseRequest"; break;
		case WindowState::Enabled: out << " Enabled"; break;
		case WindowState::DecorationStatusBarVisible: out << " DecorationStatusBarVisible"; break;
		case WindowState::DecorationNavigationVisible: out << " DecorationNavigationVisible"; break;
		case WindowState::DecorationStatusBarLight: out << " DecorationStatusBarLight"; break;
		case WindowState::DecorationNavigationLight: out << " DecorationNavigationLight"; break;
		case WindowState::DecorationShowBySwipe: out << " DecorationShowBySwipe"; break;
		case WindowState::DecorationState: break;
		case WindowState::AllowedWindowMenu: out << " AllowedWindowMenu"; break;
		case WindowState::AllowedMove: out << " MoveAllowed"; break;
		case WindowState::AllowedResize: out << " ResizeAllowed"; break;
		case WindowState::AllowedMinimize: out << " MinimizeAllowed"; break;
		case WindowState::AllowedShade: out << " ShadeAllowed"; break;
		case WindowState::AllowedStick: out << " StickAllowed"; break;
		case WindowState::AllowedMaximizeVert: out << " MaximizeVertAllowed"; break;
		case WindowState::AllowedMaximizeHorz: out << " MaximizeHorzAllowed"; break;
		case WindowState::AllowedClose: out << " CloseAllowed"; break;
		case WindowState::AllowedFullscreen: out << " FullscreenAllowed"; break;
		case WindowState::AllowedActionsMask: break;
		case WindowState::TiledLeft: out << " TiledLeft"; break;
		case WindowState::TiledRight: out << " TiledRight"; break;
		case WindowState::TiledTop: out << " TiledTop"; break;
		case WindowState::TiledBottom: out << " TiledBottom"; break;
		case WindowState::TiledTopLeft: break;
		case WindowState::TiledTopRight: break;
		case WindowState::TiledBottomLeft: break;
		case WindowState::TiledBottomRight: break;
		case WindowState::ConstrainedLeft: out << " ConstrainedLeft"; break;
		case WindowState::ConstrainedRight: out << " ConstrainedRight"; break;
		case WindowState::ConstrainedTop: out << " ConstrainedTop"; break;
		case WindowState::ConstrainedBottom: out << " ConstrainedBottom"; break;
		case WindowState::TilingMask: break;
		case WindowState::All: break;
		}
	}
}

thread_local LooperAdapter *tl_threadLooper;

LooperAdapter *LooperAdapter::getForThread() { return tl_threadLooper; }

void LooperAdapter::setForThread(LooperAdapter *) { tl_threadLooper = nullptr; }

} // namespace sprt::window
