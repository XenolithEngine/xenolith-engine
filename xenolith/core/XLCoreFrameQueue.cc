/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreLoop.h"
#include "XLCoreDevice.h"
#include "XLCoreQueuePass.h"

#ifndef XL_FRAME_QUEUE_LOG
#define XL_FRAME_QUEUE_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

FrameQueue::~FrameQueue() {
	_frame = nullptr;
	XL_FRAME_QUEUE_LOG("Ended");
}

bool FrameQueue::init(const Rc<PoolRef> &p, const Rc<Queue> &q, FrameHandle &f) {
	_pool = p;
	_queue = q;
	_frame = &f;
	_loop = _frame->getLoop();
	_order = f.getOrder();
	XL_FRAME_QUEUE_LOG("Started");
	return true;
}

bool FrameQueue::setup() {
	bool valid = true;

	_renderPasses.reserve(_queue->getPasses().size());
	_renderPassesInitial.reserve(_queue->getPasses().size());

	for (auto &it : _queue->getPasses()) {
		auto pass = it->pass->makeFrameHandle(*this);
		auto v =
				_renderPasses
						.emplace(it,
								FramePassData{FrameRenderPassState::Initial, pass, pass->getData()})
						.first;
		pass->setQueueData(v->second);
		_renderPassesInitial.emplace(&v->second);
	}

	_attachments.reserve(_queue->getAttachments().size());
	_attachmentsInitial.reserve(_queue->getAttachments().size());

	for (auto &it : _queue->getAttachments()) {
		auto h = it->attachment->makeFrameHandle(*this);
		if (h->isAvailable(*this)) {
			auto v = _attachments
							 .emplace(it, FrameAttachmentData({FrameAttachmentState::Initial, h}))
							 .first;
			if (it->type == AttachmentType::Image) {
				auto img = static_cast<ImageAttachment *>(it->attachment.get());
				v->second.info = img->getImageInfo();
				if ((v->second.info.hints & ImageHints::FixedSize) == ImageHints::None) {
					v->second.info.extent = _frame->getFrameConstraints().extent;
				}
			} else {
				v->second.info.extent = _frame->getFrameConstraints().extent;
			}

			h->setQueueData(v->second);
			_attachmentsInitial.emplace(&v->second);
		}
	}

	for (auto &it : _attachments) {
		auto passes = it.second.handle->getAttachment()->getRenderPasses();
		it.second.passes.reserve(passes.size());
		for (auto &pass : passes) {
			auto passIt = _renderPasses.find(pass);
			if (passIt != _renderPasses.end()) {
				it.second.passes.emplace_back(&passIt->second);
			} else {
				XL_FRAME_QUEUE_LOG("RenderPass '", pass->key, "' is not available on frame");
				valid = false;
			}
		}

		if (!it.first->passes.empty()) {
			auto &last = it.first->passes.back();
			it.second.final = last->dependency.requiredRenderPassState;
		} else {
			log::source().error("FrameQueue", "Attachment ", it.first->key,
					" not attached to any pass");
		}
	}

	for (auto &passIt : _renderPasses) {
		for (auto &a : passIt.first->attachments) {
			auto aIt = _attachments.find(a->attachment);
			if (aIt != _attachments.end()) {
				passIt.second.attachments.emplace_back(a, &aIt->second);
			} else {
				XL_FRAME_QUEUE_LOG("Attachment '", a->key, "' is not available on frame");
				valid = false;
			}
		}

		for (auto &a : passIt.first->attachments) {
			auto aIt = _attachments.find(a->attachment);
			if (aIt != _attachments.end()) {
				if (a->index == maxOf<uint32_t>()) {
					passIt.second.attachments.emplace_back(a, &aIt->second);
				}
			} else {
				XL_FRAME_QUEUE_LOG("Attachment '", a->key, "' is not available on frame");
				valid = false;
			}
		}

		for (auto &a : passIt.second.attachments) {
			passIt.second.attachmentMap.emplace(a.first->attachment, a.second);
		}
	}

	for (auto &passIt : _renderPasses) {
		FramePassData &passData = passIt.second;
		for (auto &it : passData.data->required) {
			if (auto targetPassData = const_cast<FramePassData *>(getRenderPass(it.data))) {
				auto wIt = targetPassData->waiters.find(it.requiredState);
				if (wIt == targetPassData->waiters.end()) {
					wIt = targetPassData->waiters
								  .emplace(it.requiredState, Vector<FramePassData *>())
								  .first;
				}
				wIt->second.emplace_back(&passIt.second);
			}
		}
	}

	XL_FRAME_QUEUE_LOG("Setup: ", valid);
	return valid;
}

void FrameQueue::update() {
	if (!_attachmentsInitial.empty()) {
		for (auto &it : _attachmentsInitial) {
			if (it->handle->setup(*this,
						[this, guard = Rc<FrameQueue>(this), attachment = it](bool success) {
				_loop->performOnThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentSetupComplete(*attachment);
						_loop->performOnThread([this] {
							if (_frame) {
								_frame->update();
							}
						}, this);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			})) {
				onAttachmentSetupComplete(*it);
			} else {
				it->waitForResult = true;
				XL_FRAME_QUEUE_LOG("[Attachment:", it->handle->getName(), "] State: Setup");
				it->state = FrameAttachmentState::Setup;
			}
		}
		_attachmentsInitial.clear();
	}

	do {
		auto it = _renderPassesInitial.begin();
		while (it != _renderPassesInitial.end()) {
			if ((*it)->state == FrameRenderPassState::Initial) {
				if (isRenderPassReady(**it)) {
					auto v = *it;
					it = _renderPassesInitial.erase(it);
					updateRenderPassState(*v, FrameRenderPassState::Ready);
				} else {
					++it;
				}
			} else {
				it = _renderPassesInitial.erase(it);
			}
		}
	} while (0);

	do {
		auto awaitPasses = sp::move(_awaitPasses);
		_awaitPasses.clear();
		_awaitPasses.reserve(awaitPasses.size());

		auto it = awaitPasses.begin();
		while (it != awaitPasses.end()) {
			if (isRenderPassReadyForState(*it->first,
						FrameRenderPassState(toInt(it->second) + 1))) {
				updateRenderPassState(*it->first, it->second);
			} else {
				_awaitPasses.emplace_back(*it);
			}
			++it;
		}
	} while (0);

	do {
		auto it = _renderPassesPrepared.begin();
		while (it != _renderPassesPrepared.end()) {
			if ((*it)->state == FrameRenderPassState::Prepared) {
				auto v = *it;
				onRenderPassPrepared(*v);
				if (v->state != FrameRenderPassState::Prepared) {
					it = _renderPassesPrepared.erase(it);
				} else {
					++it;
				}
			} else {
				it = _renderPassesPrepared.erase(it);
			}
		}
	} while (0);
}

void FrameQueue::invalidate() {
	if (!_finalized) {
		XL_FRAME_QUEUE_LOG("invalidate");
		_success = false;
		_invalidated = true;
		auto f = _frame;
		onFinalized();
		if (f) {
			f->onQueueInvalidated(*this);
			tryReleaseFrame();
		}
	}
}

Loop *FrameQueue::getLoop() const { return _loop; }

const FrameAttachmentData *FrameQueue::getAttachment(const AttachmentData *a) const {
	auto it = _attachments.find(a);
	if (it != _attachments.end()) {
		return &it->second;
	}
	return nullptr;
}

const FramePassData *FrameQueue::getRenderPass(const QueuePassData *p) const {
	auto it = _renderPasses.find(p);
	if (it != _renderPasses.end()) {
		return &it->second;
	}
	return nullptr;
}

bool FrameQueue::isResourcePending(const FrameAttachmentData &image) {
	if (image.image) {
		if (!image.image->isReady()) {
			return true;
		}
	}
	return false;
}

void FrameQueue::waitForResource(const FrameAttachmentData &image, Function<void(bool)> &&cb) {
	if (image.image) {
		image.image->waitReady(sp::move(cb));
	} else {
		cb(false);
	}
}

bool FrameQueue::isResourcePending(const FramePassData &) { return false; }

void FrameQueue::waitForResource(const FramePassData &, Function<void()> &&) {
	// TODO
}

void FrameQueue::onAttachmentSetupComplete(FrameAttachmentData &attachment) {
	if (attachment.handle->isOutput()) {
		// do nothing for now
	}
	if (attachment.handle->isInput()) {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: InputRequired");
		attachment.state = FrameAttachmentState::InputRequired;
		if (auto data = _frame->getInputData(attachment.handle->getAttachment()->getData())) {
			attachment.waitForResult = true;
			attachment.handle->submitInput(*this, move(data),
					[this, guard = Rc<FrameQueue>(this), attachment = &attachment](bool success) {
				_loop->performOnThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentInput(*attachment);
						_loop->performOnThread([frame = _frame.get()] {
							if (frame) {
								frame->update();
							}
						}, _frame);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			});
		} else {
			attachment.waitForResult = true;
			attachment.handle->getAttachment()->acquireInput(*this, *attachment.handle,
					[this, guard = Rc<FrameQueue>(this), attachment = &attachment](bool success) {
				if (!success) {
					log::source().warn("FrameQueue", "Fail to acquire input for attachment: ",
							attachment->handle->getName());
				}
				_loop->performOnThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentInput(*attachment);
						_loop->performOnThread([frame = _frame] {
							if (frame) {
								frame->update();
							}
						}, _frame);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			});
		}
	} else {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: Ready");
		attachment.state = FrameAttachmentState::Ready;
	}
}

void FrameQueue::onAttachmentInput(FrameAttachmentData &attachment) {
	XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: Ready");
	attachment.state = FrameAttachmentState::Ready;
}

void FrameQueue::onAttachmentAcquire(FrameAttachmentData &attachment) {
	if (_finalized) {
		if (attachment.state != FrameAttachmentState::Finalized) {
			finalizeAttachment(attachment);
		}
		return;
	}

	XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: ResourcesPending");
	attachment.state = FrameAttachmentState::ResourcesPending;
	if (attachment.handle->getAttachment()->getData()->type == AttachmentType::Image) {
		auto img = static_cast<ImageAttachment *>(attachment.handle->getAttachment().get());

		if (img->isStatic()) {
			attachment.image = img->getStaticImageStorage();
		} else {
			attachment.image = _frame->getRenderTarget(attachment.handle->getAttachment());
		}

		if (!attachment.image && attachment.handle->isAvailable(*this)) {
			if (auto spec = _frame->getImageSpecialization(img)) {
				attachment.info = *spec;
			}
			attachment.image = _loop->acquireImage(img, attachment.handle.get(), attachment.info);
			if (!attachment.image) {
				log::source().warn("FrameQueue", "Fail to acquire image for attachment ",
						attachment.handle->getName());
				invalidate(attachment);
				return;
			}

			attachment.image->setFrameIndex(_frame->getOrder());
		}

		if (attachment.image) {
			attachment.info = attachment.image->getInfo();

			_autorelease.emplace_front(attachment.image);
			if (attachment.image->getSignalSem()) {
				_autorelease.emplace_front(attachment.image->getSignalSem());
			}
			if (attachment.image->getWaitSem()) {
				_autorelease.emplace_front(attachment.image->getWaitSem());
			}
		}

		if (isResourcePending(attachment)) {
			waitForResource(attachment, [this, attachment = &attachment](bool success) {
				if (!success) {
					log::source().warn("FrameQueue",
							"Waiting on attachment failed: ", attachment->handle->getName());
					invalidate();
					return;
				}
				XL_FRAME_QUEUE_LOG("[Attachment:", attachment->handle->getName(),
						"] State: ResourcesAcquired");
				attachment->state = FrameAttachmentState::ResourcesAcquired;
			});
		} else {
			XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(),
					"] State: ResourcesAcquired");
			attachment.state = FrameAttachmentState::ResourcesAcquired;
		}
	} else {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(),
				"] State: ResourcesAcquired");
		attachment.state = FrameAttachmentState::ResourcesAcquired;
	}
}

void FrameQueue::onAttachmentRelease(FrameAttachmentData &attachment, FrameAttachmentState state) {
	if (attachment.image
			&& attachment.handle->getAttachment()->getData()->type == AttachmentType::Image) {
		_loop->releaseImage(move(attachment.image));
		attachment.image = nullptr;
	}

	if (_finalized) {
		if (attachment.state != FrameAttachmentState::Finalized) {
			finalizeAttachment(attachment);
		}
	} else {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(),
				"] State: ResourcesReleased");
		attachment.state = state;
	}
}

bool FrameQueue::isRenderPassReady(const FramePassData &data) const {
	return isRenderPassReadyForState(data, FrameRenderPassState::Initial);
}

bool FrameQueue::isRenderPassReadyForState(const FramePassData &data,
		FrameRenderPassState state) const {
	for (auto &it : data.data->required) {
		if (auto d = getRenderPass(it.data)) {
			if (toInt(d->state) < toInt(it.requiredState) && state >= it.lockedState) {
				return false;
			}
		}
	}

	for (auto &it : data.attachments) {
		if (toInt(it.second->state) < toInt(FrameAttachmentState::Ready)) {
			return false;
		}
	}

	return true;
}

void FrameQueue::updateRenderPassState(FramePassData &data, FrameRenderPassState state) {
	if (_finalized && state != FrameRenderPassState::Finalized) {
		return;
	}

	if (toInt(data.state) >= toInt(state)) {
		return;
	}

	if (!isRenderPassReadyForState(data, FrameRenderPassState(toInt(state) + 1)) && !_invalidated) {
		_awaitPasses.emplace_back(&data, state);
		return;
	}

	if (!_loop->isRunning()) {
		invalidate();
		return;
	}

	data.state = state;

	switch (state) {
	case FrameRenderPassState::Initial:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Initial");
		break;
	case FrameRenderPassState::Ready:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Ready");
		onRenderPassReady(data);
		break;
	case FrameRenderPassState::ResourcesAcquired:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: ResourcesAcquired");
		onRenderPassResourcesAcquired(data);
		break;
	case FrameRenderPassState::Prepared:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Prepared");
		onRenderPassPrepared(data);
		break;
	case FrameRenderPassState::Submission:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Submission");
		onRenderPassSubmission(data);
		break;
	case FrameRenderPassState::Submitted:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Submitted");
		onRenderPassSubmitted(data);
		break;
	case FrameRenderPassState::Complete:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Complete");
		onRenderPassComplete(data);
		break;
	case FrameRenderPassState::Finalized:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Finalized");
		data.handle->finalize(*this, _success);
		break;
	}

	auto it = data.waiters.find(state);
	if (it != data.waiters.end()) {
		for (auto &v : it->second) {
			if (v->state == FrameRenderPassState::Initial) {
				if (isRenderPassReady(*v)) {
					updateRenderPassState(*v, FrameRenderPassState::Ready);
				}
			}
		}
	}

	for (auto &it : data.attachments) {
		if (!it.second->passes.empty() && it.second->passes.back() == &data
				&& it.second->state != FrameAttachmentState::ResourcesReleased) {
			if (it.second->final == FrameRenderPassState::Initial) {
				if (toInt(state) >= toInt(FrameRenderPassState::Submitted)) {
					onAttachmentRelease(*it.second);
				}
			} else if ((toInt(state) >= toInt(it.second->final))) {
				onAttachmentRelease(*it.second);
			}
		}
	}

	if (state >= FrameRenderPassState::Finalized) {
		++_finalizedObjects;
		tryReleaseFrame();
	}
}

void FrameQueue::onRenderPassReady(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	if (data.framebuffer) {
		return;
	}

	// fill required imageViews for framebuffer
	// only images, that attached to subpasses, merged into framebuffer
	// all framebuffer images must have same extent
	Vector<Rc<ImageView>> imageViews;
	bool attachmentsAcquired = true;
	bool _invalidate = false;

	auto acquireView = [&](const AttachmentPassData *imgDesc, const Rc<ImageStorage> &image) {
		auto imgAttachment = (const ImageAttachment *)imgDesc->attachment->attachment.get();
		ImageViewInfo info = imgAttachment->getImageViewInfo(image->getInfo(), *imgDesc);

		auto view = image->getView(info);
		if (!view) {
			view = image->makeView(info);
		}

		if (view) {
			imageViews.emplace_back(move(view));
		} else {
			XL_FRAME_QUEUE_LOG("Fail to acquire ImageView for framebuffer");
			_invalidate = true;
			attachmentsAcquired = false;
		}
	};

	for (auto &it : data.attachments) {
		if (it.second->handle->isOutput()) {
			auto out = _frame->getOutputBinding(it.second->handle->getAttachment());
			if (out) {
				_autorelease.emplace_front((FrameOutputBinding *)out);
			}
		}
		if (it.second->state == FrameAttachmentState::Ready) {
			onAttachmentAcquire(*it.second);
			if (it.second->state != FrameAttachmentState::ResourcesAcquired) {
				attachmentsAcquired = false;
				XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(),
						"] waitForResource: ", it.second->handle->getName());
				auto refId = retain();
				waitForResource(*it.second, [this, data = &data, refId](bool success) {
					if (!success) {
						invalidate();
						release(refId);
						return;
					}
					onRenderPassReady(*data);
					release(refId);
				});
			} else {
				if (it.second->image && !it.first->subpasses.empty()) {
					acquireView(it.first, it.second->image);
				}
			}
		} else if (it.second->state == FrameAttachmentState::ResourcesAcquired) {
			if (it.second->image && !it.first->subpasses.empty()) {
				acquireView(it.first, it.second->image);
			}
		}
	}

	if (_invalidate) {
		invalidate();
		return;
	}

	if (attachmentsAcquired) {
		if (!imageViews.empty()) {
			Extent3 extent = imageViews.front()->getFramebufferExtent();
			auto it = imageViews.begin();
			while (it != imageViews.end()) {
				if ((*it)->getFramebufferExtent() != extent) {
					log::source().warn("FrameQueue", "Invalid extent for framebuffer image: ",
							(*it)->getFramebufferExtent());
					it = imageViews.erase(it);
				} else {
					++it;
				}
			}

			if (data.handle->isFramebufferRequired()) {
				data.framebuffer = _loop->acquireFramebuffer(data.handle->getData(), imageViews);
				if (!data.framebuffer) {
					log::source().warn("FrameQueue", "Fail to acquire framebuffer");
					invalidate();
				}
				_autorelease.emplace_front(data.framebuffer);
			}
			if (isResourcePending(data)) {
				XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(),
						"] waitForResource (pending): ", data.handle->getName());
				data.waitForResult = true;
				waitForResource(data, [this, data = &data] {
					data->waitForResult = false;
					updateRenderPassState(*data, FrameRenderPassState::ResourcesAcquired);
				});
			} else {
				data.waitForResult = false;
				updateRenderPassState(data, FrameRenderPassState::ResourcesAcquired);
			}
		} else {
			updateRenderPassState(data, FrameRenderPassState::ResourcesAcquired);
		}
	}
}

void FrameQueue::onRenderPassResourcesAcquired(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	if (!data.handle->isAvailable(*this)) {
		updateRenderPassState(data, FrameRenderPassState::Complete);
		return;
	}

	for (auto &it : data.attachments) {
		if (it.second->image) {
			if (auto img = it.second->image->getImage()) {
				data.handle->autorelease(img);
			}
		}
	}

	if (data.framebuffer) {
		data.handle->autorelease(data.framebuffer);
	}

	for (auto &it : data.handle->getData()->subpasses) {
		for (auto &p : it->graphicPipelines) {
			if (p->pipeline) {
				data.handle->autorelease(p->pipeline);
			}
		}
		for (auto &p : it->computePipelines) {
			if (p->pipeline) {
				data.handle->autorelease(p->pipeline);
			}
		}
	}

	if (data.handle->prepare(*this,
				[this, guard = Rc<FrameQueue>(this), data = &data](bool success) mutable {
		_loop->performOnThread([this, data, success] {
			data->waitForResult = false;
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Prepared);
			} else {
				log::source().warn("FrameQueue",
						"Fail to prepare render pass: ", data->handle->getName());
				invalidate(*data);
			}
		}, guard, true);
	})) {
		updateRenderPassState(data, FrameRenderPassState::Prepared);
	} else {
		data.waitForResult = true;
	}
}

void FrameQueue::onRenderPassPrepared(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	updateRenderPassState(data, FrameRenderPassState::Submission);
}

void FrameQueue::onRenderPassSubmission(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	auto sync = makeRenderPassSync(data);

	data.waitForResult = true;
	data.handle->submit(*this, move(sync),
			[this, guard = Rc<FrameQueue>(this), data = &data](bool success) {
		_loop->performOnThread([this, data, success] {
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Submitted);
			} else {
				data->waitForResult = false;
				log::source().warn("FrameQueue",
						"Fail to submit render pass: ", data->handle->getName());
				invalidate(*data);
			}
		}, guard, true);
	}, [this, guard = Rc<FrameQueue>(this), data = &data](bool success) {
		_loop->performOnThread([this, data, success] {
			data->waitForResult = false;
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Complete);
			} else {
				log::source().warn("FrameQueue", "Render pass operation completed unsuccessfully: ",
						data->handle->getName());
				invalidate(*data);
			}
		}, guard, true);
	});
}

void FrameQueue::onRenderPassSubmitted(FramePassData &data) {
	// no need to check finalization

	++_renderPassSubmitted;
	if (data.framebuffer) {
		_loop->releaseFramebuffer(move(data.framebuffer));
		data.framebuffer = nullptr;
	}

	if (_renderPassSubmitted == _renderPasses.size()) {
		_frame->onQueueSubmitted(*this);
	}

	for (auto &it : data.attachments) {
		if (it.second->handle->isOutput()
				&& it.second->handle->getAttachment()->getData()->outputState
						== FrameRenderPassState::Submitted
				&& it.first->attachment->attachment->getLastRenderPass()
						== data.handle->getData()) {
			if (_frame->onOutputAttachment(*it.second)) {
				it.second->image = nullptr;
				onAttachmentRelease(*it.second, FrameAttachmentState::Detached);
			}
		}
	}

	if (!data.submitTime) {
		data.submitTime = sp::platform::clock(ClockType::Monotonic);
	}
}

void FrameQueue::onRenderPassComplete(FramePassData &data) {
	auto t = sp::platform::clock(ClockType::Monotonic) - data.submitTime;

	_submissionTime += t;
	_deviceTime += data.deviceTime;
	if (_finalized) {
		invalidate(data);
		return;
	}

	for (auto &it : data.attachments) {
		if (it.second->handle->isOutput()
				&& it.second->handle->getAttachment()->getData()->outputState
						== FrameRenderPassState::Complete
				&& it.first->attachment->attachment->getLastRenderPass()
						== data.handle->getData()) {
			_frame->onOutputAttachment(*it.second);
		}
	}

	++_renderPassCompleted;
	if (_renderPassCompleted == _renderPasses.size()) {
		onComplete();
	}
}

Rc<FrameSync> FrameQueue::makeRenderPassSync(FramePassData &data) const {
	auto ret = Rc<FrameSync>::alloc();

	for (auto &it : data.data->sourceQueueDependencies) {
		//auto handle = getAttachment(it->attachments.front());
		auto target = getRenderPass(it->target);

		auto sem = _loop->makeSemaphore();

		ret->signalAttachments.emplace_back(FrameSyncAttachment{nullptr, sem, nullptr});

		target->waitSync.emplace_back(FrameSyncAttachment{nullptr, sem, nullptr, it->stageFlags});
	}

	for (auto &it : data.waitSync) { ret->waitAttachments.emplace_back(move(it)); }

	for (auto &it : data.attachments) {
		// insert wait sem when image first-time used
		if (it.first->attachment->attachment->getFirstRenderPass() == data.handle->getData()) {
			if (it.second->image && it.second->image->getWaitSem()) {
				ret->waitAttachments.emplace_back(FrameSyncAttachment{it.second->handle,
					it.second->image->getWaitSem(), it.second->image.get(),
					getWaitStageForAttachment(data, it.second->handle)});
			}
		}

		// insert signal sem when image last-time used
		if (it.second->handle->getAttachment()->getLastRenderPass() == data.handle->getData()) {
			if (it.second->image && it.second->image->getSignalSem()) {
				ret->signalAttachments.emplace_back(FrameSyncAttachment{it.second->handle,
					it.second->image->getSignalSem(), it.second->image.get()});
			}
		}

		if (it.second->image) {
			auto layout = it.first->finalLayout;
			if (layout == AttachmentLayout::PresentSrc && !it.second->image->isSwapchainImage()) {
				layout = AttachmentLayout::TransferSrcOptimal;
			}
			ret->images.emplace_back(FrameSyncImage{it.second->handle, it.second->image, layout});
		}
	}

	return ret;
}

PipelineStage FrameQueue::getWaitStageForAttachment(FramePassData &data,
		const AttachmentHandle *handle) const {
	for (auto &it : data.handle->getData()->attachments) {
		if (it->attachment == handle->getAttachment()->getData()) {
			if (it->dependency.initialUsageStage == PipelineStage::None) {
				return PipelineStage::BottomOfPipe;
			} else {
				return it->dependency.initialUsageStage;
			}
		}
	}
	return PipelineStage::None;
}

void FrameQueue::onComplete() {
	if (!_finalized) {
		XL_FRAME_QUEUE_LOG("onComplete");
		_success = true;
		_frame->onQueueComplete(*this);
		onFinalized();
	}
}

void FrameQueue::onFinalized() {
	if (_finalized) {
		return;
	}

	XL_FRAME_QUEUE_LOG("onFinalized");

	_finalized = true;
	for (auto &pass : _renderPasses) { invalidate(pass.second); }

	for (auto &pass : _attachments) { invalidate(pass.second); }
}

void FrameQueue::invalidate(FrameAttachmentData &data) {
	if (!_finalized) {
		invalidate();
		return;
	}

	if (data.state == FrameAttachmentState::Finalized) {
		return;
	}

	if (!data.waitForResult) {
		finalizeAttachment(data);
	}
}

void FrameQueue::invalidate(FramePassData &data) {
	if (!_finalized) {
		XL_FRAME_QUEUE_LOG("[Queue:", _queue->getName(), "] Invalidated");
		invalidate();
		return;
	}

	if (data.state == FrameRenderPassState::Finalized) {
		return;
	}

	if (data.state == FrameRenderPassState::Ready
			|| (!data.waitForResult && toInt(data.state) > toInt(FrameRenderPassState::Ready))) {
		data.waitForResult = false;
	}

	if (!data.waitForResult && data.framebuffer) {
		_loop->releaseFramebuffer(move(data.framebuffer));
		data.framebuffer = nullptr;
	}

	if (!data.waitForResult) {
		updateRenderPassState(data, FrameRenderPassState::Finalized);
	}
}

void FrameQueue::tryReleaseFrame() {
	if (_finalizedObjects == _renderPasses.size() + _attachments.size()) {
		if (_frame) {
			_frame = nullptr;
		}
	}
}

void FrameQueue::finalizeAttachment(FrameAttachmentData &attachment) {
	attachment.handle->finalize(*this, _success);
	XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: Finalized [",
			_success, "]");
	attachment.state = FrameAttachmentState::Finalized;
	if (!_success && _frame && attachment.handle->isOutput()) {
		_frame->onOutputAttachmentInvalidated(attachment);
	}
	++_finalizedObjects;
	tryReleaseFrame();
}

} // namespace stappler::xenolith::core
