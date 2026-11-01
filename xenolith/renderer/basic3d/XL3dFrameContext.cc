/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dFrameContext.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "XLCoreFrameRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

FrameContextHandle3d::~FrameContextHandle3d() {
	if (app && !views.empty() && !app->isOnThisThread()) {
		app->performOnAppThread([views = sp::move(views)]() { }, nullptr, false,
				"basic3d::FrameContextHandle3d::~FrameContextHandle3d");
	}
}

void FrameContext3d::onEnter(xenolith::Scene *scene) {
	FrameContext2d::onEnter(scene);

	_inputAttachmentData = nullptr;
	if (_queue) {
		for (auto &it : _queue->getInputAttachments()) {
			if (it->key == InputAttachmentName) {
				_inputAttachmentData = it;
			}
		}
	}
	if (!_inputAttachmentData) {
		log::source().error("FrameContext3d", "the queue has no ", InputAttachmentName,
				" attachment: the scene was not built by basic3d::Scene3d");
	}
}

Rc<FrameContextHandle> FrameContext3d::makeHandle(FrameInfo &frame) {
	auto h = Rc<FrameContextHandle3d>::alloc();
	h->clock = frame.director->getUpdateTime().app;
	h->client = frame.director;
	// Locally built frame: the client is this window's Director, so the id is the local-request 0.
	h->windowId = 0;
	h->context = this;
	h->app = frame.director->getApplication();
	h->commands = Rc<basic2d::CommandList>::create(frame.pool);
	return h;
}

void FrameContext3d::submitHandle(FrameInfo &frame, FrameContextHandle *handle) {
	if (_inputAttachmentData) {
		frame.resolvedInputs.emplace(_inputAttachmentData);
	}

	FrameContext2d::submitHandle(frame, handle);

	if (_inputAttachmentData) {
		frame.director->performOnRenderThread(
				[req = frame.request, att = _inputAttachmentData,
						h = Rc<FrameContextHandle3d>(static_cast<FrameContextHandle3d *>(handle))]() mutable {
			req->addInput(att, sp::move(h));
		}, this);
	}
}

} // namespace stappler::xenolith::basic3d
