#include "XLCommon.h"
#include "render/ResourceUploadLayout.h"
#include "XLDirector.h"
#include "XLCoreDynamicImage.h"
#include "XLRemoteObject.h"
#include "XL2dCommandList.h"
#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool ResourceUploadLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}
	_sprite = addChild(Rc<basic2d::Sprite>::create());
	_sprite->setAnchorPoint(Anchor::Middle);
	_sprite->setContentSize(Size2(128, 128));
	return true;
}

void ResourceUploadLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);
	unsigned variant = ::getenv("XL_UPLOAD_VARIANT") ? 1 : 0;
	_texture = _director->getResourceCache()->addExternalImage("upload-quadrants",
			core::ImageInfo(Extent2(128, 128), core::ImageFormat::R8G8B8A8_UNORM,
					core::ImageUsage::Sampled),
			[variant](uint8_t *ptr, uint64_t, const core::ImageData::DataCallback &cb) {
		Bytes pixels(128 * 128 * 4);
		constexpr uint8_t colors[4][4] = {
			{230, 20, 40, 255},
			{20, 210, 60, 255},
			{30, 70, 230, 255},
			{240, 190, 20, 255},
		};
		for (unsigned y = 0; y < 128; ++y) {
			for (unsigned x = 0; x < 128; ++x) {
				::memcpy(pixels.data() + (y * 128 + x) * 4,
						colors[((y >= 64) * 2 + (x >= 64) + variant) % 4], 4);
			}
		}
		if (ptr) {
			::memcpy(ptr, pixels.data(), pixels.size());
		} else {
			cb(pixels);
		}
	}, TimeInterval(), TemporaryResourceFlags::CompileWhenAdded);
	_sprite->setTexture(Rc<Texture>(_texture));
}

void ResourceUploadLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_sprite->setPosition(Vec2(getWorkSize().width / 2, getWorkSize().height / 2));
}

void ResourceUploadLayout::registerCommands() {
	addCommand("state", "Whether the uploaded image is compiled", [this](Value &&) {
		Value value;
		value.setBool(_texture && _texture->isLoaded(), "loaded");
		static constexpr uint8_t pixel[4] = {255, 255, 255, 255};
		auto dynamic = Rc<core::DynamicImage>::create([](core::DynamicImage::Builder &builder) {
			builder.setImageByRef("pending-atlas",
					core::ImageInfo(Extent2(1, 1), core::ImageFormat::R8G8B8A8_UNORM,
							core::ImageUsage::Sampled),
					BytesView(pixel, 4));
			return true;
		});
		auto texture = Rc<Texture>::create(dynamic);
		bool pending = !texture->isLoaded() && texture->getIndex() == 0;
		auto image = Rc<remote::Image>::create(987, dynamic->getInfo());
		dynamic->setImage(image);
		bool ready = texture->isLoaded() && texture->getIndex() == image->getIndex();
		dynamic->finalize();
		value.setBool(pending && ready && !texture->isLoaded() && texture->getIndex() == 0,
				"dynamicLifetime");
		auto empty = Rc<basic2d::FrameContextHandle2d>::alloc();
		auto decoded = Rc<basic2d::FrameContextHandle2d>::alloc();
		bool roundtrip = false;
		empty->serialize([&](BytesView bytes) {
			roundtrip =
					decoded->deserialize(bytes) && decoded->commands && decoded->commands->empty();
		});
		value.setBool(roundtrip, "emptyFrame");
		return value;
	}, true);
}

} // namespace stappler::xenolith::app
