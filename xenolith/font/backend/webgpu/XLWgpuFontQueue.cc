/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLWgpuFontQueue.h"

#if MODULE_XENOLITH_BACKEND_WEBGPU

#include "XLWgpuDevice.h"
#include "XLWgpuLoop.h"
#include "XLWgpuObject.h"
#include "XLFontDeferredRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreDynamicImage.h"
#include "SPFontEmplace.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

class FontAttachment : public core::GenericAttachment {
public:
	virtual ~FontAttachment() = default;

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;
};

class FontAttachmentHandle : public core::AttachmentHandle {
public:
	static constexpr uint64_t CopyBlockSize = 32_MiB;

	// glyph placement data: bitmap location in _glyphData + packed atlas position
	struct GlyphRegion {
		uint64_t bufferOffset = 0;
		uint32_t texOffset = 0;
		uint32_t objectId = 0;
		uint16_t x = 0;
		uint16_t y = 0;
		uint16_t width = 0;
		uint16_t height = 0;
	};

	struct GlyphTextureData {
		int16_t x = 0;
		int16_t y = 0;
		uint16_t width = 0;
		uint16_t height = 0;
	};

	virtual ~FontAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	Extent2 getImageExtent() const { return _imageExtent; }
	const Rc<font::RenderFontInput> &getInput() const { return _input; }
	const Rc<core::DataAtlas> &getAtlas() const { return _atlas; }
	BytesView getComposedImage() const { return _composedImage; }

protected:
	void doSubmitInput(core::FrameHandle &, Function<void(bool)> &&, Rc<font::RenderFontInput> &&);
	void pushCopyTexture(uint32_t reqIdx, const font::CharTexture &);
	void writeAtlasData(core::FrameHandle &);
	void pushAtlasTexture(core::DataAtlas *, const GlyphRegion &);

	uint64_t reserveBlock(uint64_t size);

	Rc<font::RenderFontInput> _input;
	Function<void(bool)> _onInput;

	Bytes _glyphData;
	sprt::atomic<uint64_t> _dataOffset = 0;
	sprt::atomic<uint32_t> _regionOffset = 0;
	Vector<GlyphRegion> _regions;
	Vector<GlyphTextureData> _textureTarget;

	Bytes _composedImage;
	Extent2 _imageExtent;
	Rc<core::DataAtlas> _atlas;
};

class FontRenderPass : public QueuePass {
public:
	virtual ~FontRenderPass() = default;

	virtual bool init(QueuePassBuilder &, const core::AttachmentData *);

	virtual Rc<core::QueuePassHandle> makeFrameHandle(const core::FrameQueue &) override;

	const core::AttachmentData *getFontAttachment() const { return _fontAttachment; }

protected:
	using QueuePass::init;

	const core::AttachmentData *_fontAttachment = nullptr;
};

class FontRenderPassHandle : public QueuePassHandle {
public:
	virtual ~FontRenderPassHandle() = default;

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(core::FrameQueue &, Rc<core::FrameSync> &&, Function<void(bool)> &&,
			Function<void(bool)> &&) override;

protected:
	FontAttachmentHandle *_fontAttachment = nullptr;
};

FontQueue::~FontQueue() { }

bool FontQueue::init(StringView name) {
	using namespace core;
	Queue::Builder builder(name);

	auto attachment = builder.addAttachemnt("FontQueueAttachment",
			[](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		attachmentBuilder.defineAsOutput();
		return Rc<FontAttachment>::create(attachmentBuilder);
	});

	builder.addPass("FontQueuePass", PassType::Transfer, RenderOrdering(0),
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<FontRenderPass>::create(passBuilder, attachment);
	});

	if (Queue::init(move(builder))) {
		_attachment = attachment;
		return true;
	}
	return false;
}

Rc<core::AttachmentHandle> FontAttachment::makeFrameHandle(const core::FrameQueue &handle) {
	return Rc<FontAttachmentHandle>::create(this, handle);
}

void FontAttachmentHandle::submitInput(core::FrameQueue &q,
		Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<font::RenderFontInput>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, d = sp::move(d), cb = sp::move(cb)](core::FrameHandle &handle,
					bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		doSubmitInput(handle, sp::move(cb), sp::move(d));
	});
}

uint64_t FontAttachmentHandle::reserveBlock(uint64_t size) {
	auto aligned = math::align(size, uint64_t(4));
	auto ret = _dataOffset.fetch_add(aligned);
	if (ret + size > _glyphData.size()) {
		return maxOf<uint64_t>();
	}
	return ret;
}

void FontAttachmentHandle::doSubmitInput(core::FrameHandle &handle, Function<void(bool)> &&cb,
		Rc<font::RenderFontInput> &&d) {
	uint32_t totalCount = 0;
	for (auto &it : d->requests) { totalCount += uint32_t(it.chars.size()); }

	_input = sp::move(d);
	_onInput = sp::move(cb);

	// +1: white underline pixel
	_regions.resize(totalCount + 1);
	_textureTarget.resize(totalCount + 1);
	_glyphData.resize(CopyBlockSize);

	if (totalCount == 0) {
		writeAtlasData(handle);
		return;
	}

	font::DeferredRequest::runFontRenderer(_input->queue, _input->ext, _input->requests,
			[this](uint32_t reqIdx, const font::CharTexture &texData) {
		pushCopyTexture(reqIdx, texData);
	}, [this, handle = Rc<core::FrameHandle>(&handle)] { writeAtlasData(*handle); });
}

void FontAttachmentHandle::pushCopyTexture(uint32_t reqIdx, const font::CharTexture &texData) {
	if (texData.width != texData.bitmapWidth || texData.height != texData.bitmapRows) {
		log::source().error("webgpu::FontQueue", "Invalid glyph size: ", texData.width, ";",
				texData.height, " vs. ", texData.bitmapWidth, ";", texData.bitmapRows);
	}

	// store rows tightly repacked to bitmapWidth: the composer reads the
	// block with bitmapWidth stride, source pitch may be padded or negative
	const uint32_t rowWidth = texData.bitmapWidth;
	const uint64_t size = uint64_t(texData.bitmapRows) * uint64_t(rowWidth);

	auto offset = reserveBlock(size);
	if (offset == maxOf<uint64_t>()) {
		log::source().error("webgpu::FontQueue", "Not enough space in glyph buffer");
		return;
	}

	auto ptr = texData.bitmap;
	for (size_t i = 0; i < texData.bitmapRows; ++i) {
		sprt::memcpy(_glyphData.data() + offset + i * rowWidth, ptr, rowWidth);
		ptr += texData.pitch;
	}

	auto objectId =
			font::CharId::getCharId(texData.fontID, texData.charID, font::CharAnchor::BottomLeft);
	auto texOffset = _regionOffset.fetch_add(1);

	_regions[texOffset] = GlyphRegion{offset, texOffset, objectId, 0, 0,
		uint16_t(texData.bitmapWidth), uint16_t(texData.bitmapRows)};
	_textureTarget[texOffset] =
			GlyphTextureData{texData.x, texData.y, texData.width, texData.height};
}

void FontAttachmentHandle::writeAtlasData(core::FrameHandle &handle) {
	// white pixel for underlines
	{
		auto offset = reserveBlock(1);
		if (offset != maxOf<uint64_t>()) {
			_glyphData[offset] = 255;

			auto objectId = font::CharId::getCharId(font::CharId::SourceMax, 0,
					font::CharAnchor::BottomLeft);
			auto texOffset = _regionOffset.fetch_add(1);
			_regions[texOffset] = GlyphRegion{offset, texOffset, objectId, 0, 0, 1, 1};
			_textureTarget[texOffset] = GlyphTextureData{0, 0, 1, 1};
		}
	}

	_regions.resize(_regionOffset.load());
	_textureTarget.resize(_regions.size());

	memory::perform_temporary([&] {
		mem_pool::Vector<GlyphRegion *> layoutData;
		layoutData.reserve(_regions.size());

		float totalSquare = 0.0f;

		for (auto &d : _regions) {
			auto it = sprt::lower_bound(layoutData.begin(), layoutData.end(), &d,
					[](const GlyphRegion *l, const GlyphRegion *r) -> bool {
				if (l->height == r->height && l->width == r->width) {
					return l->objectId < r->objectId;
				} else if (l->height == r->height) {
					return l->width > r->width;
				} else {
					return l->height > r->height;
				}
			});
			layoutData.emplace(it, &d);
			totalSquare += d.width * d.height;
		}

		font::EmplaceCharInterface iface({
			[](void *ptr) -> uint16_t { return reinterpret_cast<GlyphRegion *>(ptr)->x; },
			[](void *ptr) -> uint16_t { return reinterpret_cast<GlyphRegion *>(ptr)->y; },
			[](void *ptr) -> uint16_t { return reinterpret_cast<GlyphRegion *>(ptr)->width; },
			[](void *ptr) -> uint16_t { return reinterpret_cast<GlyphRegion *>(ptr)->height; },
			[](void *ptr, uint16_t value) { reinterpret_cast<GlyphRegion *>(ptr)->x = value; },
			[](void *ptr, uint16_t value) { reinterpret_cast<GlyphRegion *>(ptr)->y = value; },
			[](void *ptr, uint16_t value) { },
		});

		auto span = makeSpanView(reinterpret_cast<void **>(layoutData.data()), layoutData.size());

		_imageExtent = font::emplaceChars(iface, span, totalSquare);

		auto atlas = Rc<core::DataAtlas>::create(core::DataAtlas::ImageAtlas,
				uint32_t(_regions.size() * 4), uint32_t(sizeof(font::FontAtlasValue)),
				_imageExtent);

		for (auto &it : _regions) { pushAtlasTexture(atlas, it); }

		atlas->compile();
		_atlas = move(atlas);
	});

	// compose the final R8 raster on CPU: used for both the GPU upload
	// and the optional CPU output callback
	_composedImage.clear();
	_composedImage.resize(size_t(_imageExtent.width) * size_t(_imageExtent.height), 0);
	for (auto &it : _regions) {
		for (uint32_t row = 0; row < it.height; ++row) {
			sprt::memcpy(_composedImage.data()
							+ size_t(it.y + row) * _imageExtent.width + it.x,
					_glyphData.data() + it.bufferOffset + size_t(row) * it.width, it.width);
		}
	}

	if (auto path = ::getenv("XL_DUMP_FONT_ATLAS")) {
		if (auto f = ::fopen(path, "wb")) {
			::fprintf(f, "P5\n%u %u\n255\n", _imageExtent.width, _imageExtent.height);
			::fwrite(_composedImage.data(), 1, _composedImage.size(), f);
			::fclose(f);
		}
	}

	handle.performOnGlThread([this](core::FrameHandle &) {
		_onInput(true);
		_onInput = nullptr;
	}, this, false, "FontAttachmentHandle::writeAtlasData");
}

void FontAttachmentHandle::pushAtlasTexture(core::DataAtlas *atlas, const GlyphRegion &region) {
	if (::getenv("XL_DUMP_FONT_ATLAS")) {
		log::source().debug("webgpu::FontQueue", "atlas obj=", region.objectId, " ch=",
				uint32_t(region.objectId) & 0xFFFF, " src=", uint32_t(region.objectId) >> 18,
				" cell=", region.x, ",", region.y, " ", region.width, "x", region.height);
	}

	font::FontAtlasValue data[4];

	auto &tex = _textureTarget[region.texOffset];

	const float x = float(region.x);
	const float y = float(region.y);
	const float w = float(region.width);
	const float h = float(region.height);

	data[0].pos = Vec2(tex.x, -tex.y);
	data[0].tex = Vec2(x / _imageExtent.width, y / _imageExtent.height);

	data[1].pos = Vec2(tex.x, -tex.y - tex.height);
	data[1].tex = Vec2(x / _imageExtent.width, (y + h) / _imageExtent.height);

	data[2].pos = Vec2(tex.x + tex.width, -tex.y - tex.height);
	data[2].tex = Vec2((x + w) / _imageExtent.width, (y + h) / _imageExtent.height);

	data[3].pos = Vec2(tex.x + tex.width, -tex.y);
	data[3].tex = Vec2((x + w) / _imageExtent.width, y / _imageExtent.height);

	atlas->addObject(font::CharId::rebindCharId(region.objectId, font::CharAnchor::BottomLeft),
			&data[0]);
	atlas->addObject(font::CharId::rebindCharId(region.objectId, font::CharAnchor::TopLeft),
			&data[1]);
	atlas->addObject(font::CharId::rebindCharId(region.objectId, font::CharAnchor::TopRight),
			&data[2]);
	atlas->addObject(font::CharId::rebindCharId(region.objectId, font::CharAnchor::BottomRight),
			&data[3]);
}

bool FontRenderPass::init(QueuePassBuilder &passBuilder, const core::AttachmentData *attachment) {
	passBuilder.addAttachment(attachment);

	if (!QueuePass::init(passBuilder)) {
		return false;
	}
	_fontAttachment = attachment;
	return true;
}

Rc<core::QueuePassHandle> FontRenderPass::makeFrameHandle(const core::FrameQueue &handle) {
	return Rc<FontRenderPassHandle>::create(*this, handle);
}

bool FontRenderPassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	if (auto a = q.getAttachment(
				static_cast<FontRenderPass *>(_queuePass.get())->getFontAttachment())) {
		_fontAttachment = static_cast<FontAttachmentHandle *>(a->handle.get());
	}

	return QueuePassHandle::prepare(q, sp::move(cb));
}

void FontRenderPassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	if (!_fontAttachment || !_fontAttachment->getInput()) {
		log::source().error("webgpu::FontQueue", "No font input to submit");
		if (onSubmited) {
			onSubmited(false);
		}
		if (onComplete) {
			onComplete(false);
		}
		return;
	}

	auto &input = _fontAttachment->getInput();
	auto instance = input->image->getInstance();
	if (!instance) {
		log::source().error("webgpu::FontQueue", "Target image has no initial instance");
		if (onSubmited) {
			onSubmited(false);
		}
		if (onComplete) {
			onComplete(false);
		}
		return;
	}

	auto extent = _fontAttachment->getImageExtent();

	core::ImageInfo info = input->image->getInfo();
	info.format = core::ImageFormat::R8_UNORM;
	info.extent = Extent3(extent.width, extent.height, 1);
	info.usage = core::ImageUsage::Sampled | core::ImageUsage::TransferDst;

	auto targetImage = Rc<Image>::create(*_device, info.key, info);
	if (!targetImage) {
		log::source().error("webgpu::FontQueue", "Fail to allocate font atlas image");
		if (onSubmited) {
			onSubmited(false);
		}
		if (onComplete) {
			onComplete(false);
		}
		return;
	}

	// direct upload, ordered before the (empty) queue submit below
	auto composed = _fontAttachment->getComposedImage();

	WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	dst.texture = targetImage->getTexture();

	WGPUTexelCopyBufferLayout layout;
	layout.offset = 0;
	layout.bytesPerRow = extent.width;
	layout.rowsPerImage = extent.height;

	WGPUExtent3D writeExtent{extent.width, extent.height, 1};

	wgpuQueueWriteTexture(_device->getQueue(), &dst, composed.data(), composed.size(), &layout,
			&writeExtent);

	// publish the new instance to the dynamic image
	core::ImageViewInfo viewInfo;
	viewInfo.setup(targetImage->getInfo());
	viewInfo.setup(core::ColorMode::SolidColor, true);

	auto view = Rc<ImageView>::create(*_device, targetImage.get(), viewInfo);

	input->image->updateInstance(*q.getLoop(), targetImage,
			Rc<core::DataAtlas>(_fontAttachment->getAtlas()), nullptr,
			q.getFrame()->getSignalDependencies(), sp::move(view));

	if (input->output) {
		input->output(targetImage->getInfo(), composed);
	}

	QueuePassHandle::submit(q, sp::move(sync), sp::move(onSubmited), sp::move(onComplete));

	q.getFrame()->signalDependencies(true);
}

} // namespace stappler::xenolith::webgpu

#endif /* MODULE_XENOLITH_BACKEND_WEBGPU */
