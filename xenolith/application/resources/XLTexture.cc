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

#include "XLTexture.h"
#include "XLTemporaryResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Texture::~Texture() { }

bool Texture::init(const core::ImageData *data) {
	if (!ResourceObject::init(ResourceType::Texture)) {
		return false;
	}

	_data = data;
	return true;
}

bool Texture::init(const core::ImageData *data, const Rc<core::Resource> &res) {
	if (!ResourceObject::init(ResourceType::Texture, res)) {
		return false;
	}

	_data = data;
	return true;
}

bool Texture::init(const Rc<core::DynamicImage> &image) {
	if (!ResourceObject::init(ResourceType::Texture)) {
		return false;
	}

	_dynamic = image;
	return true;
}

bool Texture::init(const core::ImageData *data, const Rc<TemporaryResource> &tmp) {
	if (!ResourceObject::init(ResourceType::Texture, tmp)) {
		return false;
	}

	_data = data;
	return true;
}

StringView Texture::getName() const {
	if (_dynamic) {
		return _dynamic->getInfo().key;
	} else if (_data) {
		return _data->key;
	}
	return StringView();
}

uint64_t Texture::getIndex() const {
	if (_dynamic) {
		return _dynamic->getInstance()->data.image->getIndex();
	} else if (_data->image) {
		return _data->image->getIndex();
	}
	return 0;
}

/* Block formats that may carry alpha. `getImagePixelFormat` answers Unknown for most of them, which
is right for what it is asked elsewhere (a view, an attachment) and wrong here: a sprite whose
texture is BC7 or ASTC was drawn opaque, its transparent texels as the colour stored under them. An
image known to be opaque says so with ImageHints::Opaque. */
static bool Texture_isAlphaBlockFormat(core::ImageFormat format) {
	using core::ImageFormat;
	switch (format) {
	case ImageFormat::BC2_UNORM_BLOCK:
	case ImageFormat::BC2_SRGB_BLOCK:
	case ImageFormat::BC3_UNORM_BLOCK:
	case ImageFormat::BC3_SRGB_BLOCK:
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
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT:
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG: return true;
	default: break;
	}
	return false;
}

static bool Texture_hasAlpha(core::ImageFormat format, core::ImageHints hints) {
	if ((hints & core::ImageHints::Opaque) != core::ImageHints::None) {
		return false;
	}
	switch (core::getImagePixelFormat(format)) {
	case core::PixelFormat::A:
	case core::PixelFormat::IA:
	case core::PixelFormat::RGBA: return true;
	default: break;
	}
	return Texture_isAlphaBlockFormat(format);
}

bool Texture::hasAlpha() const {
	if (_dynamic) {
		auto info = _dynamic->getInfo();
		return Texture_hasAlpha(info.format, info.hints);
	}
	return Texture_hasAlpha(_data->format, _data->hints);
}

Extent3 Texture::getExtent() const {
	if (_dynamic) {
		return _dynamic->getExtent();
	} else if (_data) {
		return _data->extent;
	}
	return Extent3();
}

bool Texture::isLoaded() const {
	return _dynamic || (_temporary && _temporary->isLoaded() && _data->image) || _data->image;
}

core::MaterialImage Texture::getMaterialImage() const {
	core::MaterialImage ret;
	if (_dynamic) {
		ret.dynamic = _dynamic->getInstance();
		ret.image = &ret.dynamic->data;
	} else {
		ret.image = _data;
	}
	ret.info.setup(*ret.image);
	return ret;
}

core::ImageInfoData Texture::getImageInfo() const {
	if (_dynamic) {
		return _dynamic->getInfo();
	} else {
		return *_data;
	}
}

} // namespace stappler::xenolith
