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

#ifndef XENOLITH_CORE_XLCOREINSTANCE_H_
#define XENOLITH_CORE_XLCOREINSTANCE_H_

#include "XLCore.h" // IWYU pragma: keep
#include "SPDso.h"

#include <sprt/runtime/window/gapi.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Loop;
class Queue;
class Device;

static constexpr uint16_t InstanceDefaultDevice = maxOf<uint16_t>();

using sprt::window::gapi::InstanceApi;
using sprt::window::gapi::InstanceFlags;


struct SP_PUBLIC InstanceBackendInfo : public sprt::window::gapi::InstanceBackendInfo {
	virtual ~InstanceBackendInfo() = default;

	virtual Value encode() const = 0;
};

struct SP_PUBLIC InstancePlatformInfo : public sprt::window::gapi::InstancePlatformInfo {
	virtual ~InstancePlatformInfo() = default;

	virtual Value encode() const = 0;
};

using sprt::window::gapi::InstanceInfo;
using sprt::window::gapi::LoopInfo;

SP_PUBLIC Value encodeInstanceInfo(const InstanceInfo &info);
SP_PUBLIC Value encodeLoopInfo(const LoopInfo &info);

struct SP_PUBLIC LoopBackendInfo : public sprt::window::gapi::LoopBackendInfo {
	virtual ~LoopBackendInfo() = default;

	virtual Value encode() const = 0;
};

struct SP_PUBLIC DeviceProperties {
	String deviceName;
	uint32_t apiVersion = 0;
	uint32_t driverVersion = 0;
	bool supportsPresentation = false;
};

class SP_PUBLIC Instance : public sprt::window::gapi::Instance {
public:
	static Rc<Instance> create(Rc<InstanceInfo> &&);

	virtual ~Instance();

	Instance(InstanceApi b, InstanceFlags flags, Dso &&);

	const Vector<DeviceProperties> &getAvailableDevices() const { return _availableDevices; }

	virtual Rc<Loop> makeLoop(NotNull<event::Looper>, Rc<core::LoopInfo> &&) const;

	virtual bool isPresentationSupported() const;

	InstanceApi getApi() const { return _api; }
	InstanceFlags getFlags() const { return _flags; }

protected:
	InstanceApi _api = InstanceApi::None;
	InstanceFlags _flags = InstanceFlags::None;
	Dso _dsoModule;
	Vector<DeviceProperties> _availableDevices;
};

SP_PUBLIC StringView getInstanceApiName(InstanceApi);

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREINSTANCE_H_ */
