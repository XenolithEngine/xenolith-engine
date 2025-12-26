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

#include "XL2dVkShadow.h"
#include "XLCoreAttachment.h"
#include "XLCoreEnum.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameCache.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

void ShadowLightDataAttachmentHandle::submitInput(FrameQueue &q,
		Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<FrameContextHandle2d>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, d = sp::move(d), cb = sp::move(cb)](FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		auto devFrame = static_cast<DeviceFrameHandle *>(&handle);

		_input = move(d);
		_data = devFrame->getMemPool(devFrame)->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::ShaderDeviceAddress),
						sizeof(ShadowData)));
		cb(true);
	});
}

void ShadowLightDataAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, uint32_t gridSize,
		float maxValue) {
	_data->map([&, this](uint8_t *buf, VkDeviceSize size) {
		ShadowData *data = reinterpret_cast<ShadowData *>(buf);

		if (isnan(_input->lights.luminosity)) {
			float l = _input->lights.globalColor.a;
			for (uint32_t i = 0; i < _input->lights.ambientLightCount; ++i) {
				l += _input->lights.ambientLights[i].color.a;
			}
			for (uint32_t i = 0; i < _input->lights.directLightCount; ++i) {
				l += _input->lights.directLights[i].color.a;
			}
			_shadowData.luminosity = data->luminosity = 1.0f / l;
		} else {
			_shadowData.luminosity = data->luminosity = 1.0f / _input->lights.luminosity;
		}

		float fullDensity = _input->lights.sceneDensity;
		auto screenSize = devFrame->getFrameConstraints().getScreenSize();

		Extent2 scaledExtent(ceilf(screenSize.width / fullDensity),
				ceilf(screenSize.height / fullDensity));

		_shadowData.globalColor = data->globalColor = _input->lights.globalColor * data->luminosity;

		// pre-calculated color with no shadows
		Color4F discardColor = _shadowData.globalColor;
		for (uint32_t i = 0; i < _input->lights.ambientLightCount; ++i) {
			auto a = _input->lights.ambientLights[i].color.a * data->luminosity;
			auto ncolor = (_input->lights.ambientLights[i].color
								  * _input->lights.ambientLights[i].color.a)
					* data->luminosity;
			ncolor.a = a;
			discardColor = discardColor + ncolor;
		}
		discardColor.a = 1.0f;
		_shadowData.discardColor = data->discardColor = discardColor;

		_shadowData.maxValue = data->maxValue = maxValue;

		_shadowData.ambientLightCount = data->ambientLightCount = _input->lights.ambientLightCount;
		_shadowData.directLightCount = data->directLightCount = _input->lights.directLightCount;

		_shadowData.bbOffset = data->bbOffset = getBoxOffset(_shadowData.maxValue);
		_shadowData.density = data->density = _input->lights.sceneDensity;
		_shadowData.shadowSdfDensity = data->shadowSdfDensity = 1.0f / _input->lights.shadowDensity;
		_shadowData.shadowDensity = data->shadowDensity = 1.0f / _input->lights.sceneDensity;
		_shadowData.pix = data->pix =
				Vec2(1.0f / float(screenSize.width), 1.0f / float(screenSize.height));

		memcpy(data->ambientLights, _input->lights.ambientLights,
				sizeof(AmbientLightData) * config::MaxAmbientLights);
		memcpy(data->directLights, _input->lights.directLights,
				sizeof(DirectLightData) * config::MaxDirectLights);
		memcpy(_shadowData.ambientLights, _input->lights.ambientLights,
				sizeof(AmbientLightData) * config::MaxAmbientLights);
		memcpy(_shadowData.directLights, _input->lights.directLights,
				sizeof(DirectLightData) * config::MaxDirectLights);
	});
}

float ShadowLightDataAttachmentHandle::getBoxOffset(float value) const {
	value = std::max(value, 2.0f);
	float bbox = 0.0f;
	for (size_t i = 0; i < _input->lights.ambientLightCount; ++i) {
		auto &l = _input->lights.ambientLights[i];
		float n_2 = l.normal.x * l.normal.x + l.normal.y * l.normal.y;
		float m = std::sqrt(n_2) / std::sqrt(1 - n_2);

		bbox = std::max((m * value * 2.0f) + (std::ceil(l.normal.w * value)), bbox);
	}
	return bbox;
}

uint32_t ShadowLightDataAttachmentHandle::getLightsCount() const {
	return _input->lights.ambientLightCount + _input->lights.directLightCount;
}

bool ShadowLightDataAttachment::init(AttachmentBuilder &builder) {
	if (!core::GenericAttachment::init(builder)) {
		return false;
	}

	builder.setInputValidationCallback([](const core::AttachmentInputData *input) {
		if (dynamic_cast<const FrameContextHandle2d *>(input)) {
			return true;
		}
		return false;
	});

	return true;
}

auto ShadowLightDataAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowLightDataAttachmentHandle>::create(this, handle);
}

} // namespace stappler::xenolith::basic2d::vk
