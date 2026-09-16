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

#include "XLMtlInstance.h"
#include "XLMtlDevice.h"
#include "XLMtlLoop.h"

#include <TargetConditionals.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

Instance::Instance(InstanceFlags flags)
: core::Instance(InstanceApi::Metal, flags, sprt::Dso()) {
	enumerateDevices();
}

Instance::~Instance() {
	for (auto &it : _devices) {
		releaseHandle(it.device);
		it.device = nullptr;
	}
	_devices.clear();
}

size_t Instance::getDeviceCount() const { return _devices.size(); }

bool Instance::readDeviceProperties(size_t n, sprt::window::gapi::DeviceProperties &prop) {
	if (n >= _devices.size()) {
		return false;
	}

	auto &it = _devices.at(n);

	prop.deviceName = it.name;
	prop.apiVersion = 0;
	prop.driverVersion = 0;
	prop.presentationSupported = !it.headless;

	return true;
}

Rc<core::Loop> Instance::makeLoop(NotNull<sprt::dispatch::Looper> looper,
		Rc<core::LoopInfo> &&info) const {
	return Rc<Loop>::create(looper, const_cast<Instance *>(this), move(info));
}

const Instance::DeviceData *Instance::selectDevice(const core::LoopInfo &info) const {
	if (info.deviceIdx != core::InstanceDefaultDevice) {
		if (info.deviceIdx < _devices.size()) {
			return &_devices.at(info.deviceIdx);
		}
		return nullptr;
	}

	// prefer a non-headless high-power device; on Apple Silicon there is a
	// single unified device and any branch below selects it
	const DeviceData *result = nullptr;
	for (auto &it : _devices) {
		if (!it.headless && !it.lowPower) {
			return &it;
		}
		if (!result && !it.headless) {
			result = &it;
		}
	}
	if (!result && !_devices.empty()) {
		result = &_devices.front();
	}
	return result;
}

Rc<Device> Instance::makeDevice(const core::LoopInfo &info) const {
	auto data = selectDevice(info);
	if (!data || !data->device) {
		log::source().error("mtl::Instance", "No suitable device for device index: ",
				info.deviceIdx);
		return nullptr;
	}

	return Rc<Device>::create(const_cast<Instance *>(this), *data);
}

void Instance::enumerateDevices() {
#if TARGET_OS_IPHONE
	// iOS exposes a single system device
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device) {
		log::source().warn("mtl::Instance", "No Metal devices found");
		return;
	}

	DeviceData data;
	data.name = String(device.name.UTF8String);
	data.registryID = device.registryID;
	data.unifiedMemory = device.hasUnifiedMemory;
	data.device = retainHandle(device);
	_devices.emplace_back(sp::move(data));
#else
	NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
	if (devices.count == 0) {
		log::source().warn("mtl::Instance", "No Metal devices found");
		return;
	}

	_devices.reserve(devices.count);
	for (id<MTLDevice> device in devices) {
		DeviceData data;
		data.name = String(device.name.UTF8String);
		data.registryID = device.registryID;
		data.lowPower = device.isLowPower;
		data.removable = device.isRemovable;
		data.headless = device.isHeadless;
		data.unifiedMemory = device.hasUnifiedMemory;
		data.device = retainHandle(device);

		_devices.emplace_back(sp::move(data));
	}
#endif
}

} // namespace stappler::xenolith::mtl
