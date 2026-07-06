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

#include "XLWgpuInstance.h"
#include "XLWgpuDevice.h"
#include "XLWgpuLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

Instance::Instance(WGPUInstance instance, InstanceFlags flags, sprt::Dso &&dso)
: core::Instance(InstanceApi::WebGPU, flags, sp::move(dso)), _instance(instance) {
	enumerateAdapters();
}

Instance::~Instance() {
	for (auto &it : _adapters) {
		if (it.adapter) {
			wgpuAdapterRelease(it.adapter);
			it.adapter = nullptr;
		}
	}
	_adapters.clear();

	if (_instance) {
		wgpuInstanceRelease(_instance);
		_instance = nullptr;
	}
}

size_t Instance::getDeviceCount() const { return _adapters.size(); }

bool Instance::readDeviceProperties(size_t n, sprt::window::gapi::DeviceProperties &prop) {
	if (n >= _adapters.size()) {
		return false;
	}

	auto &it = _adapters.at(n);

	prop.deviceName = it.device;
	prop.apiVersion = 0;
	prop.driverVersion = 0;

	// Actual presentation support can only be verified against a live surface;
	// exclude only adapters that can not output to a screen at all
	prop.presentationSupported = it.adapterType != WGPUAdapterType_CPU
			&& it.backendType != WGPUBackendType_Null;

	return true;
}

Rc<core::Loop> Instance::makeLoop(NotNull<sprt::dispatch::Looper> looper,
		Rc<core::LoopInfo> &&info) const {
	return Rc<Loop>::create(looper, const_cast<Instance *>(this), move(info));
}

const Instance::AdapterData *Instance::selectAdapter(const core::LoopInfo &info) const {
	if (info.deviceIdx != core::InstanceDefaultDevice) {
		if (info.deviceIdx < _adapters.size()) {
			return &_adapters.at(info.deviceIdx);
		}
		return nullptr;
	}

	// prefer discrete, then integrated, then any non-CPU adapter
	const AdapterData *result = nullptr;
	for (auto &it : _adapters) {
		if (it.adapterType == WGPUAdapterType_DiscreteGPU) {
			return &it;
		}
		if (!result && it.adapterType == WGPUAdapterType_IntegratedGPU) {
			result = &it;
		}
	}
	if (!result) {
		for (auto &it : _adapters) {
			if (it.adapterType != WGPUAdapterType_CPU) {
				result = &it;
				break;
			}
		}
	}
	if (!result && !_adapters.empty()) {
		result = &_adapters.front();
	}
	return result;
}

Rc<Device> Instance::makeDevice(const core::LoopInfo &info) const {
	auto data = selectAdapter(info);
	if (!data || !data->adapter) {
		log::source().error("webgpu::Instance", "No suitable adapter for device index: ",
				info.deviceIdx);
		return nullptr;
	}

	struct RequestResult {
		WGPUDevice device = nullptr;
		bool complete = false;
	} result;

	// request everything the adapter can offer: all supported features
	// and the adapter's actual limits instead of the WebGPU defaults
	WGPUSupportedFeatures supportedFeatures;
	wgpuAdapterGetFeatures(data->adapter, &supportedFeatures);

	Vector<WGPUFeatureName> requestedFeatures;
	requestedFeatures.reserve(supportedFeatures.featureCount);
	for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
		auto feature = supportedFeatures.features[i];
		// experimental features require an opt-in that is not exposed
		// via the C API, requesting them fails device creation
		if (feature == WGPUFeatureName(WGPUNativeFeature_RayQuery)
				|| feature == WGPUFeatureName(WGPUNativeFeature_CooperativeMatrix)) {
			continue;
		}
		requestedFeatures.emplace_back(feature);
	}
	wgpuSupportedFeaturesFreeMembers(supportedFeatures);

	// query and request native limits too (binding array sizes live there)
	WGPUNativeLimits nativeLimits{};
	nativeLimits.chain.sType = (WGPUSType)WGPUSType_NativeLimits;

	WGPULimits adapterLimits = WGPU_LIMITS_INIT;
	adapterLimits.nextInChain = &nativeLimits.chain;
	auto limitsStatus = wgpuAdapterGetLimits(data->adapter, &adapterLimits);

	WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
	deviceDesc.label = WGPUStringView{"xenolith", WGPU_STRLEN};
	deviceDesc.requiredFeatureCount = requestedFeatures.size();
	deviceDesc.requiredFeatures = requestedFeatures.data();
	if (limitsStatus == WGPUStatus_Success) {
		deviceDesc.requiredLimits = &adapterLimits;
	}

	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const *, WGPUDeviceLostReason reason,
			WGPUStringView message, void *, void *) {
		log::source().error("webgpu::Device", "Device lost (", toInt(reason), "): ",
				toStringView(message));
	};

	deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const *, WGPUErrorType type,
			WGPUStringView message, void *, void *) {
		log::source().error("webgpu::Device", "Uncaptured error (", toInt(type), "): ",
				toStringView(message));
	};

	WGPURequestDeviceCallbackInfo cbInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
	cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	cbInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
			void *userdata1, void *) {
		auto result = reinterpret_cast<RequestResult *>(userdata1);
		if (status == WGPURequestDeviceStatus_Success) {
			result->device = device;
		} else {
			log::source().error("webgpu::Instance", "Fail to request device: ",
					toStringView(message));
		}
		result->complete = true;
	};
	cbInfo.userdata1 = &result;

	wgpuAdapterRequestDevice(data->adapter, &deviceDesc, cbInfo);

	// wgpu-native resolves device requests synchronously via ProcessEvents;
	// bounded loop protects against a hang if that ever changes
	uint32_t attempts = 1'000;
	while (!result.complete && attempts > 0) {
		wgpuInstanceProcessEvents(_instance);
		--attempts;
	}

	if (!result.complete || !result.device) {
		log::source().error("webgpu::Instance", "Fail to create device on adapter: ",
				data->device);
		return nullptr;
	}

	return Rc<Device>::create(const_cast<Instance *>(this), *data, result.device);
}

void Instance::enumerateAdapters() {
	auto count = wgpuInstanceEnumerateAdapters(_instance, nullptr, nullptr);
	if (count == 0) {
		log::source().warn("webgpu::Instance", "No adapters found");
		return;
	}

	Vector<WGPUAdapter> adapters;
	adapters.resize(count);
	count = wgpuInstanceEnumerateAdapters(_instance, nullptr, adapters.data());

	_adapters.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
		wgpuAdapterGetInfo(adapters[i], &info);

		AdapterData data;
		data.vendor = toStringView(info.vendor).str<Interface>();
		data.architecture = toStringView(info.architecture).str<Interface>();
		data.device = toStringView(info.device).str<Interface>();
		data.description = toStringView(info.description).str<Interface>();
		data.vendorID = info.vendorID;
		data.deviceID = info.deviceID;
		data.backendType = info.backendType;
		data.adapterType = info.adapterType;
		data.adapter = adapters[i];

		wgpuAdapterInfoFreeMembers(info);

		_adapters.emplace_back(sp::move(data));
	}
}

StringView getBackendTypeName(WGPUBackendType type) {
	switch (type) {
	case WGPUBackendType_Undefined: return "Undefined"; break;
	case WGPUBackendType_Null: return "Null"; break;
	case WGPUBackendType_WebGPU: return "WebGPU"; break;
	case WGPUBackendType_D3D11: return "D3D11"; break;
	case WGPUBackendType_D3D12: return "D3D12"; break;
	case WGPUBackendType_Metal: return "Metal"; break;
	case WGPUBackendType_Vulkan: return "Vulkan"; break;
	case WGPUBackendType_OpenGL: return "OpenGL"; break;
	case WGPUBackendType_OpenGLES: return "OpenGLES"; break;
	case WGPUBackendType_Force32: break;
	}
	return StringView();
}

StringView getAdapterTypeName(WGPUAdapterType type) {
	switch (type) {
	case WGPUAdapterType_DiscreteGPU: return "DiscreteGPU"; break;
	case WGPUAdapterType_IntegratedGPU: return "IntegratedGPU"; break;
	case WGPUAdapterType_CPU: return "CPU"; break;
	case WGPUAdapterType_Unknown: return "Unknown"; break;
	case WGPUAdapterType_Force32: break;
	}
	return StringView();
}

} // namespace stappler::xenolith::webgpu
