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

#include "XL2dFrameContext.h"
#include "XLFrameContext.h"
#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLDirector.h"
#include "SPBitmap.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool ShadowLightInput::addAmbientLight(const Vec4 &pos, const Color4F &color, bool softShadow) {
	if (ambientLightCount >= config::MaxAmbientLights) {
		return false;
	}

	ambientLights[ambientLightCount++] = AmbientLightData{pos, color, uint32_t(softShadow ? 1 : 0)};
	return true;
}

bool ShadowLightInput::addDirectLight(const Vec4 &pos, const Color4F &color, const Vec4 &data) {
	if (directLightCount >= config::MaxDirectLights) {
		return false;
	}

	directLights[directLightCount++] = DirectLightData{pos, color, data};
	return true;
}

Extent2 ShadowLightInput::getShadowExtent(Size2 frameSize) const {
	return Extent2(std::ceil((frameSize.width / sceneDensity) * shadowDensity),
			std::ceil((frameSize.height / sceneDensity) * shadowDensity));
}

Size2 ShadowLightInput::getShadowSize(Size2 frameSize) const {
	return Size2((frameSize.width / sceneDensity) * shadowDensity,
			(frameSize.height / sceneDensity) * shadowDensity);
}

void FrameContext2d::onEnter(Scene *scene) {
	FrameContext::onEnter(scene);
	if (!_init || _queue) {
		if (!initWithQueue(_queue)) {
			log::source().error("FrameContext2d",
					"Fail to initialize with queue: ", _queue->getName());
		}
	}
}

void FrameContext2d::onExit() { FrameContext::onExit(); }

Rc<FrameContextHandle> FrameContext2d::makeHandle(FrameInfo &frame) {
	auto h = Rc<FrameContextHandle2d>::alloc();
	h->clock = frame.director->getUpdateTime().app;
	h->director = frame.director;
	h->context = this;
	h->commands = Rc<CommandList>::create(frame.pool);
	return h;
}

void FrameContext2d::submitHandle(FrameInfo &frame, FrameContextHandle *handle) {
	auto h = static_cast<FrameContextHandle2d *>(handle);

	frame.resolvedInputs.emplace(_vertexAttachmentData);
	frame.resolvedInputs.emplace(_lightAttachmentData);
	frame.resolvedInputs.emplace(_particleEmitterAttachmentData);

	if (_materialDependency) {
		handle->waitDependencies.emplace_back(_materialDependency);
	}

	frame.director->getGlLoop()->performOnThread(
			[this, req = frame.request, q = _queue, dir = frame.director,
					h = Rc<FrameContextHandle2d>(h)]() mutable {
		req->addInput(_vertexAttachmentData, Rc<FrameContextHandle2d>(h));
		req->addInput(_lightAttachmentData, Rc<FrameContextHandle2d>(h));
		req->addInput(_particleEmitterAttachmentData, Rc<FrameContextHandle2d>(h));
	},
			this);

	FrameContext::submitHandle(frame, handle);
}

bool FrameContext2d::initWithQueue(core::Queue *queue) {
	for (auto &it : queue->getAttachments()) {
		if (it->key == MaterialAttachmentName) {
			if (auto m = dynamic_cast<core::MaterialAttachment *>(it->attachment.get())) {
				_materialAttachmentData = it;
				readMaterials(m);
			}
		}
	}

	for (auto &it : queue->getInputAttachments()) {
		if (it->key == VertexAttachmentName) {
			_vertexAttachmentData = it;
		} else if (it->key == LightDataAttachmentName) {
			_lightAttachmentData = it;
		} else if (it->key == ParticleEmittersAttachment) {
			_particleEmitterAttachmentData = it;
		}
	}

	return _materialAttachmentData && _vertexAttachmentData && _lightAttachmentData
			&& _particleEmitterAttachmentData;
}

bool StateData::init() { return true; }

bool StateData::init(StateData *data) {
	if (data) {
		transform = data->transform;
		gradient = data->gradient;
	}
	return true;
}

} // namespace stappler::xenolith::basic2d
