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

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Loop;
class Queue;
class Device;

static constexpr uint16_t InstanceDefaultDevice = maxOf<uint16_t>();

enum class InstanceApi {
	None = 0,
	Vulkan = 1,
};

enum class InstanceFlags : uint32_t {
	None = 0,
	RenderDoc = 1 << 0,
	Validation = 1 << 1, // try to enable validation layers
	ForcedValidation = 1 << 2, // do not start if we failed to enable validation
	ValidateSynchromization = 1 << 3, // validate data synchronization
};

SP_DEFINE_ENUM_AS_MASK(InstanceFlags)

struct SP_PUBLIC InstanceBackendInfo : public Ref {
	virtual ~InstanceBackendInfo() = default;

	virtual Value encode() const = 0;
};

struct SP_PUBLIC InstancePlatformInfo : public Ref {
	virtual ~InstancePlatformInfo() = default;

	virtual Value encode() const = 0;
};

struct SP_PUBLIC InstanceInfo final : public Ref {
	virtual ~InstanceInfo() = default;

	InstanceApi api = InstanceApi::None;
	InstanceFlags flags = InstanceFlags::None;
	Rc<InstanceBackendInfo> backend;

	Value encode() const;
};

struct SP_PUBLIC LoopBackendInfo : public Ref {
	virtual ~LoopBackendInfo() = default;

	virtual Value encode() const = 0;
};

struct SP_PUBLIC LoopInfo final : public Ref {
	virtual ~LoopInfo() = default;

	uint16_t deviceIdx = InstanceDefaultDevice;
	ImageFormat defaultFormat = ImageFormat::R8G8B8A8_UNORM;
	Rc<LoopBackendInfo> backend; // backend-specific data

	Value encode() const;
};

struct SP_PUBLIC DeviceProperties {
	String deviceName;
	uint32_t apiVersion = 0;
	uint32_t driverVersion = 0;
	bool supportsPresentation = false;
};

class SP_PUBLIC Instance : public Ref {
public:
	static Rc<Instance> create(Rc<InstanceInfo> &&);

	virtual ~Instance();

	Instance(InstanceApi b, InstanceFlags flags, Dso &&);

	const Vector<DeviceProperties> &getAvailableDevices() const { return _availableDevices; }

	virtual Rc<Loop> makeLoop(NotNull<event::Looper>, Rc<core::LoopInfo> &&) const;

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
