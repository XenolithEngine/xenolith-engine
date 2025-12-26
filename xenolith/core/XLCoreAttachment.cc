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

#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

uint32_t DependencyEvent::GetNextId() {
	static std::atomic<uint32_t> s_eventId = 1;
	return s_eventId.fetch_add(1);
}

DependencyEvent::~DependencyEvent() { }

DependencyEvent::DependencyEvent(QueueSet &&q, StringView str) : _queues(sp::move(q)), _tag(str) { }

DependencyEvent::DependencyEvent(InitializerList<Rc<Queue>> &&il, StringView str)
: _queues(sp::move(il)), _tag(str) { }

bool DependencyEvent::signal(Queue *q, bool success) {
	if (!success) {
		_success = success;
	}

	auto it = _queues.find(q);
	if (it != _queues.end()) {
		_queues.erase(it);
	}
	return _queues.empty();
}

bool DependencyEvent::isSignaled() const { return _queues.empty(); }

bool DependencyEvent::isSuccessful() const { return _success; }

void DependencyEvent::addQueue(Rc<Queue> &&q) { _queues.emplace(move(q)); }

bool Attachment::init(AttachmentBuilder &builder) {
	_data = builder.getAttachmentData();
	return true;
}

void Attachment::clear() { }

StringView Attachment::getName() const { return _data->key; }

uint64_t Attachment::getId() const { return _data->id; }

AttachmentUsage Attachment::getUsage() const { return _data->usage; }

bool Attachment::isTransient() const { return _data->transient; }

void Attachment::setFrameHandleCallback(FrameHandleCallback &&cb) {
	_frameHandleCallback = sp::move(cb);
}

void Attachment::acquireInput(FrameQueue &frame, AttachmentHandle &a, Function<void(bool)> &&cb) {
	if (_data->inputAcquisitionCallback) {
		_data->inputAcquisitionCallback(frame, a, sp::move(cb));
	} else {
		// wait for input on handle
		frame.getFrame()->waitForInput(frame, a, sp::move(cb));
	}
}

bool Attachment::validateInput(const AttachmentInputData *data) const {
	if (_data->inputValidationCallback) {
		return _data->inputValidationCallback(data);
	}
	return true;
}

Rc<AttachmentHandle> Attachment::makeFrameHandle(const FrameQueue &queue) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	}
	return nullptr;
}

Vector<const QueuePassData *> Attachment::getRenderPasses() const {
	Vector<const PassData *> ret;
	for (auto &it : _data->passes) { ret.emplace_back(it->pass); }
	return ret;
}

const QueuePassData *Attachment::getFirstRenderPass() const {
	if (_data->passes.empty()) {
		return nullptr;
	}

	return _data->passes.front()->pass;
}

const QueuePassData *Attachment::getLastRenderPass() const {
	if (_data->passes.empty()) {
		return nullptr;
	}

	return _data->passes.back()->pass;
}

const QueuePassData *Attachment::getNextRenderPass(const PassData *pass) const {
	size_t idx = 0;
	for (auto &it : _data->passes) {
		if (it->pass == pass) {
			break;
		}
		++idx;
	}
	if (idx + 1 >= _data->passes.size()) {
		return nullptr;
	}
	return _data->passes.at(idx + 1)->pass;
}

const QueuePassData *Attachment::getPrevRenderPass(const PassData *pass) const {
	size_t idx = 0;
	for (auto &it : _data->passes) {
		if (it->pass == pass) {
			break;
		}
		++idx;
	}
	if (idx == 0) {
		return nullptr;
	}
	return _data->passes.at(idx - 1)->pass;
}

void Attachment::setCompiled(Device &dev) { }

bool BufferAttachment::init(AttachmentBuilder &builder, const BufferInfo &info) {
	_info = info;
	builder.setType(AttachmentType::Buffer);
	if (Attachment::init(builder)) {
		_info.key = _data->key;
		return true;
	}
	return false;
}

bool BufferAttachment::init(AttachmentBuilder &builder, const BufferData *data) {
	_info = *data;
	builder.setType(AttachmentType::Buffer);
	if (Attachment::init(builder)) {
		_staticBuffers.emplace_back(data);
		_info.key = _data->key;
		return true;
	}
	return false;
}

bool BufferAttachment::init(AttachmentBuilder &builder, Vector<const BufferData *> &&buffers) {
	_info = *buffers.front();
	builder.setType(AttachmentType::Buffer);
	if (Attachment::init(builder)) {
		_staticBuffers = sp::move(buffers);
		_info.key = _data->key;
		return true;
	}
	return false;
}

void BufferAttachment::clear() { Attachment::clear(); }

Vector<BufferObject *> BufferAttachment::getStaticBuffers() const {
	Vector<BufferObject *> ret;
	ret.reserve(_staticBuffers.size());
	for (auto &it : _staticBuffers) {
		if (it->buffer) {
			ret.emplace_back(it->buffer);
		}
	}
	return ret;
}

bool ImageAttachment::init(AttachmentBuilder &builder, const ImageInfo &info, AttachmentInfo &&a) {
	builder.setType(AttachmentType::Image);
	if (Attachment::init(builder)) {
		_imageInfo = info;
		_attachmentInfo = move(a);
		_imageInfo.key = _data->key;
		return true;
	}
	return false;
}

bool ImageAttachment::init(AttachmentBuilder &builder, const ImageData *data, AttachmentInfo &&a) {
	builder.setType(AttachmentType::Image);
	if (Attachment::init(builder)) {
		_imageInfo = *data; // copy info
		if ((_imageInfo.hints & ImageHints::Static) != ImageHints::Static) {
			log::source().error("ImageAttachment", "Image ", data->key,
					" is not defined as ImageHint::Static to be used as static image attachment");
		}
		_staticImage = data;
		_attachmentInfo = move(a);
		_imageInfo.key = _data->key;
		return true;
	}
	return false;
}

void ImageAttachment::addImageUsage(ImageUsage usage) { _imageInfo.usage |= usage; }

bool ImageAttachment::isCompatible(const ImageInfo &image) const {
	return _imageInfo.isCompatible(image);
}

ImageViewInfo ImageAttachment::getImageViewInfo(const ImageInfoData &info,
		const AttachmentPassData &passAttachment) const {
	bool allowSwizzle = true;
	AttachmentUsage usage = AttachmentUsage::None;
	for (auto &subpassAttachment : passAttachment.subpasses) { usage |= subpassAttachment->usage; }

	if ((usage & AttachmentUsage::Input) != AttachmentUsage::None
			|| (usage & AttachmentUsage::Output) != AttachmentUsage::None
			|| (usage & AttachmentUsage::Resolve) != AttachmentUsage::None
			|| (usage & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
		allowSwizzle = false;
	}

	ImageViewInfo passInfo(info);
	passInfo.setup(passAttachment.colorMode, allowSwizzle);
	return passInfo;
}

Vector<ImageViewInfo> ImageAttachment::getImageViews(const ImageInfoData &info) const {
	Vector<ImageViewInfo> ret;

	auto addView = [&](const ImageViewInfo &info) {
		auto it = std::find(ret.begin(), ret.end(), info);
		if (it == ret.end()) {
			ret.emplace_back(info);
		}
	};

	for (auto &passAttachment : _data->passes) {
		addView(getImageViewInfo(info, *passAttachment));

		for (auto &desc : passAttachment->descriptors) {
			bool allowSwizzle = (desc->type == DescriptorType::SampledImage);

			ImageViewInfo passInfo(info);
			passInfo.setup(passAttachment->colorMode, allowSwizzle);
			addView(passInfo);
		}
	}

	return ret;
}

void ImageAttachment::setCompiled(Device &dev) {
	Attachment::setCompiled(dev);

	if (!isStatic()) {
		return;
	}

	auto views = getImageViews(getImageInfo());

	_staticImageStorage = Rc<ImageStorage>::create(getStaticImage());

	for (auto &info : views) {
		auto v = _staticImageStorage->getView(info);
		if (!v) {
			auto v = dev.makeImageView(_staticImageStorage->getImage(), info);
			_staticImageStorage->addView(info, move(v));
		}
	}
}

bool GenericAttachment::init(AttachmentBuilder &builder) {
	builder.setType(AttachmentType::Generic);
	return Attachment::init(builder);
}

bool AttachmentHandle::init(const Rc<Attachment> &attachment, const FrameQueue &frame) {
	_attachment = attachment;
	return true;
}

bool AttachmentHandle::init(Attachment &attachment, const FrameQueue &frame) {
	return init(&attachment, frame);
}

void AttachmentHandle::setQueueData(FrameAttachmentData &data) { _queueData = &data; }

// returns true for immediate setup, false if seyup job was scheduled
bool AttachmentHandle::setup(FrameQueue &, Function<void(bool)> &&) { return true; }

void AttachmentHandle::finalize(FrameQueue &, bool successful) { }

void AttachmentHandle::submitInput(FrameQueue &q, Rc<AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
	_input = move(data);

	if (_input->waitDependencies.empty()) {
		auto ret = true;
		if (_attachment->getData()->inputSubmissionCallback) {
			_attachment->getData()->inputSubmissionCallback(q, *this, _input, sp::move(cb));
		} else {
			cb(ret);
		}
	} else {
		q.getFrame()->waitForDependencies(_input->waitDependencies,
				[this, cb = sp::move(cb), q = Rc<FrameQueue>(&q)](FrameHandle &,
						bool success) mutable {
			if (!success) {
				cb(success);
			} else if (_attachment->getData()->inputSubmissionCallback) {
				_attachment->getData()->inputSubmissionCallback(*q, *this, _input, sp::move(cb));
			} else {
				cb(success);
			}
		});
	}
}

bool AttachmentHandle::isInput() const {
	return (_attachment->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None;
}

bool AttachmentHandle::isOutput() const {
	return (_attachment->getUsage() & AttachmentUsage::Output) != AttachmentUsage::None;
}

uint32_t AttachmentHandle::enumerateDirtyDescriptors(const PassHandle &, const PipelineDescriptor &,
		const DescriptorBinding &, const Callback<void(uint32_t)> &) const {
	return 0;
}

void AttachmentHandle::enumerateAttachmentObjects(
		const Callback<void(Object *, const SubresourceRangeInfo &)> &) { }

} // namespace stappler::xenolith::core
