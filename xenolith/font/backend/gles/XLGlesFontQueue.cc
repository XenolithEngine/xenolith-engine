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

#include "XLGlesFontQueue.h"

#if MODULE_XENOLITH_BACKEND_GLES

#include "XLGlesDevice.h"
#include "XLGlesLoop.h"
#include "XLGlesObject.h"
#include "XLFontDeferredRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreDynamicImage.h"
#include "SPFontEmplace.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// The queue-pass machinery is the core one (the gles backend has no pass base of its own, unlike
// soft): pull the names in so the font pass can be written without qualifying every type.
using core::QueuePass;
using core::QueuePassBuilder;
using core::RenderOrdering;

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
		log::source().error("gles::FontQueue", "Invalid glyph size: ", texData.width, ";",
				texData.height, " vs. ", texData.bitmapWidth, ";", texData.bitmapRows);
	}

	// store rows tightly repacked to bitmapWidth: the composer reads the
	// block with bitmapWidth stride, source pitch may be padded or negative
	const uint32_t rowWidth = texData.bitmapWidth;
	const uint64_t size = uint64_t(texData.bitmapRows) * uint64_t(rowWidth);

	auto offset = reserveBlock(size);
	if (offset == maxOf<uint64_t>()) {
		log::source().error("gles::FontQueue", "Not enough space in glyph buffer");
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
			[](void *ptr, uint16_t) { },
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

	// compose the final R8 raster on CPU: this is what gets uploaded as the atlas texture
	_composedImage.clear();
	_composedImage.resize(size_t(_imageExtent.width) * size_t(_imageExtent.height), 0);
	for (auto &it : _regions) {
		for (uint32_t row = 0; row < it.height; ++row) {
			sprt::memcpy(_composedImage.data()
							+ size_t(it.y + row) * _imageExtent.width + it.x,
					_glyphData.data() + it.bufferOffset + size_t(row) * it.width, it.width);
		}
	}

	handle.performOnGlThread([this](core::FrameHandle &) {
		_onInput(true);
		_onInput = nullptr;
	}, this, false, "gles::FontAttachmentHandle::writeAtlasData");
}

void FontAttachmentHandle::pushAtlasTexture(core::DataAtlas *atlas, const GlyphRegion &region) {
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
	auto fail = [&] {
		if (onSubmited) {
			onSubmited(false);
		}
		if (onComplete) {
			onComplete(false);
		}
	};

	if (!_fontAttachment || !_fontAttachment->getInput()) {
		log::source().error("gles::FontQueue", "No font input to submit");
		fail();
		return;
	}

	auto &input = _fontAttachment->getInput();
	auto instance = input->image->getInstance();
	if (!instance) {
		log::source().error("gles::FontQueue", "Target image has no initial instance");
		fail();
		return;
	}

	auto extent = _fontAttachment->getImageExtent();
	if (extent.width == 0 || extent.height == 0) {
		// No glyphs yet: keep the existing instance, signal the frame's dependencies so the
		// waiting vertex stage can proceed with an empty atlas.
		q.getFrame()->signalDependencies(true);
		if (onSubmited) { onSubmited(true); }
		if (onComplete) { onComplete(true); }
		return;
	}

	core::ImageInfo info = input->image->getInfo();
	info.format = core::ImageFormat::R8_UNORM;
	info.extent = Extent3(extent.width, extent.height, 1);
	info.usage = core::ImageUsage::Sampled | core::ImageUsage::TransferDst;

	// Upload the composed R8 atlas as a real texture in one go: Image::init(initialData) copies
	// the bytes through glTexSubImage2D during setup. This runs on the loop thread where the EGL
	// context is current, so the call is safe here (unlike an object clear callback).
	auto composed = _fontAttachment->getComposedImage();
	auto targetImage = Rc<Image>::create(*_device, info.key, core::ImageInfoData(info),
			BytesView(composed.data(), composed.size()));
	if (!targetImage) {
		log::source().error("gles::FontQueue", "Fail to allocate font atlas image");
		fail();
		return;
	}

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

	// Signal the frame's dependencies (this is what releases the vertex stage that was waiting on
	// the FontController event). The real work - the texture upload - already happened above, so
	// there is no GL render to fence: acquire a host-only fence for the frame graph's bookkeeping,
	// exactly the way the software backend does for its font pass (which also has no GPU submit).
	q.getFrame()->signalDependencies(true);

	auto success = true;
	_fence = _loop->acquireFence(core::FenceType::Default);
	if (!_fence) {
		onSubmited(false);
		return;
	}

	_fence->setTag(getName());
	_fence->addRelease([this, guard = Rc<core::FrameQueue>(&q),
			onComplete = sp::move(onComplete)](bool fenceSuccess) {
		for (auto &it : _data->completeCallbacks) { it(*guard, *_data, fenceSuccess); }
		onComplete(fenceSuccess);
	}, this, "gles::FontRenderPassHandle::submit");

	// Nothing armed this fence on a device queue - there is no queue - so arm it by hand. Without
	// this core::Fence::check short-circuits on a non-Armed state and the release callbacks never
	// run (the soft backend needs the same call for the same reason).
	_fence->setArmed();

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, success); }

	onSubmited(success);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

} // namespace stappler::xenolith::gles

#endif /* MODULE_XENOLITH_BACKEND_GLES */
