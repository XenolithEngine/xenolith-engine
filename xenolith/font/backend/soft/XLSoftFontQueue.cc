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

#include "XLSoftFontQueue.h"

#if MODULE_XENOLITH_BACKEND_SOFT

#include "XLSoftDevice.h"
#include "XLSoftObject.h"
#include "XLSoftGlyphStore.h"
#include "XLFontDeferredRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreDynamicImage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class FontAttachment : public core::GenericAttachment {
public:
	virtual ~FontAttachment() = default;

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;
};

class FontAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~FontAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	const Rc<font::RenderFontInput> &getInput() const { return _input; }
	const Rc<core::DataAtlas> &getAtlas() const { return _atlas; }
	const Rc<GlyphStore> &getStore() const { return _store; }

protected:
	void doSubmitInput(core::FrameHandle &, Function<void(bool)> &&, Rc<font::RenderFontInput> &&);
	void buildAtlas(core::FrameHandle &);

	Rc<font::RenderFontInput> _input;
	Function<void(bool)> _onInput;
	Rc<GlyphStore> _store;
	Rc<core::DataAtlas> _atlas;
};

class FontRenderPass : public core::QueuePass {
public:
	virtual ~FontRenderPass() = default;

	virtual bool init(core::QueuePassBuilder &, const core::AttachmentData *);

	virtual Rc<core::QueuePassHandle> makeFrameHandle(const core::FrameQueue &) override;

	const core::AttachmentData *getFontAttachment() const { return _fontAttachment; }

protected:
	using core::QueuePass::init;

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

void FontAttachmentHandle::submitInput(core::FrameQueue &q, Rc<core::AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
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

void FontAttachmentHandle::doSubmitInput(core::FrameHandle &handle, Function<void(bool)> &&cb,
		Rc<font::RenderFontInput> &&d) {
	_input = sp::move(d);
	_onInput = sp::move(cb);

	// The store outlives the frame: it is carried on the dynamic image's instance, so a glyph
	// rasterized for one frame is still there for the next. This is what makes the whole update
	// incremental - only characters never seen before cost anything.
	if (auto instance = _input->image->getInstance()) {
		_store = instance->userdata.cast<GlyphStore>();
	}
	if (!_store) {
		_store = Rc<GlyphStore>::create();
	}

	_store->emplaceWhitePixel();

	// The controller resends every character a face has ever needed, not just the new ones
	// (FontFaceObject::_required only grows), so most of a request is usually already in the store.
	// Dropping those here is what the Vulkan backend does with its persistent-glyph buffers, minus
	// the copying.
	uint32_t pending = 0;
	for (auto &it : _input->requests) {
		for (auto &c : it.chars) {
			auto glyphId =
					font::CharId::getCharId(it.object->getId(), c, font::CharAnchor::BottomLeft);
			if (_store->hasGlyph(glyphId)) {
				c = 0; // the worker skips zeroed entries
			} else {
				++pending;
			}
		}
	}

	if (pending == 0) {
		buildAtlas(handle);
		return;
	}

	log::source().info("soft::FontQueue", "rasterizing ", pending, " glyph(s), workers=",
			_input->queue->getWorkersCount());

	font::DeferredRequest::runFontRendererDirect(_input->queue, _input->ext, _input->requests,
			[this](uint32_t reqIdx, const font::CharTexture &texData) -> font::GlyphTarget {
		auto glyphId = font::CharId::getCharId(texData.fontID, texData.charID,
				font::CharAnchor::BottomLeft);
		return _store->emplaceGlyph(glyphId, texData);
	}, [this, handle = Rc<core::FrameHandle>(&handle)] { buildAtlas(*handle); });
}

void FontAttachmentHandle::buildAtlas(core::FrameHandle &handle) {
	auto count = _store->getGlyphCount();

	// The atlas keeps its job of mapping a glyph id to the four corner offsets of its quad - that
	// part is backend-neutral and the shared vertex plan reads it directly. Only the texture
	// coordinates change meaning: with no atlas image to address, each glyph is its own texture and
	// the corners are the unit square.
	//
	// The extent must not be left at zero: the plan divides by it when a glyph is missing.
	auto atlas = Rc<core::DataAtlas>::create(core::DataAtlas::ImageAtlas, count * 4,
			uint32_t(sizeof(font::FontAtlasValue)), Extent2(1, 1));

	_store->foreachGlyph([&](uint32_t glyphId, const GlyphStore::Glyph &glyph) {
		font::FontAtlasValue data[4];

		// Identical to what every other backend writes: the quad arrives degenerate (all four
		// corners at the pen), and these offsets give it its shape.
		data[0].pos = Vec2(glyph.x, -glyph.y);
		data[0].tex = Vec2(0.0f, 0.0f);

		data[1].pos = Vec2(glyph.x, -glyph.y - glyph.metricHeight);
		data[1].tex = Vec2(0.0f, 1.0f);

		data[2].pos = Vec2(glyph.x + glyph.metricWidth, -glyph.y - glyph.metricHeight);
		data[2].tex = Vec2(1.0f, 1.0f);

		data[3].pos = Vec2(glyph.x + glyph.metricWidth, -glyph.y);
		data[3].tex = Vec2(1.0f, 0.0f);

		atlas->addObject(font::CharId::rebindCharId(glyphId, font::CharAnchor::BottomLeft),
				&data[0]);
		atlas->addObject(font::CharId::rebindCharId(glyphId, font::CharAnchor::TopLeft), &data[1]);
		atlas->addObject(font::CharId::rebindCharId(glyphId, font::CharAnchor::TopRight), &data[2]);
		atlas->addObject(font::CharId::rebindCharId(glyphId, font::CharAnchor::BottomRight),
				&data[3]);
	});

	atlas->compile();
	_atlas = move(atlas);

	handle.performOnGlThread([this](core::FrameHandle &) {
		_onInput(true);
		_onInput = nullptr;
	}, this, false, "soft::FontAttachmentHandle::buildAtlas");
}

bool FontRenderPass::init(core::QueuePassBuilder &passBuilder,
		const core::AttachmentData *attachment) {
	passBuilder.addAttachment(attachment);

	if (!core::QueuePass::init(passBuilder)) {
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
		log::source().error("soft::FontQueue", "No font input to submit");
		fail();
		return;
	}

	auto &input = _fontAttachment->getInput();
	if (!input->image->getInstance()) {
		log::source().error("soft::FontQueue", "Target image has no initial instance");
		fail();
		return;
	}

	// A material needs an image, and nothing samples this one: the renderer resolves glyphs from
	// the store instead. One texel keeps it honest - if something ever does sample it, the result
	// is a visible flat colour rather than a plausible-looking wrong glyph.
	core::ImageInfo info = input->image->getInfo();
	info.format = core::ImageFormat::R8_UNORM;
	info.extent = Extent3(1, 1, 1);

	auto placeholder = Rc<Image>::create(*_device, info.key, core::ImageInfoData(info));
	if (!placeholder) {
		log::source().error("soft::FontQueue", "Fail to allocate glyph placeholder image");
		fail();
		return;
	}
	placeholder->getData()[0] = 255;

	core::ImageViewInfo viewInfo;
	viewInfo.setup(placeholder->getInfo());
	viewInfo.setup(core::ColorMode::SolidColor, true);

	auto view = Rc<ImageView>::create(*_device, Rc<core::ImageObject>(placeholder.get()), viewInfo);

	auto frame = Rc<core::FrameHandle>(q.getFrame());

	input->image->updateInstance(*q.getLoop(), placeholder,
			Rc<core::DataAtlas>(_fontAttachment->getAtlas()),
			Rc<Ref>(_fontAttachment->getStore().get()), frame->getSignalDependencies(),
			sp::move(view));

	// Signal before submitting, and through a retained handle: this backend runs its passes
	// synchronously, so by the time the base submit returns the frame can already be finished and
	// released - reaching for q.getFrame() afterwards reads freed memory.
	frame->signalDependencies(true);

	QueuePassHandle::submit(q, sp::move(sync), sp::move(onSubmited), sp::move(onComplete));
}

} // namespace stappler::xenolith::soft

#endif
