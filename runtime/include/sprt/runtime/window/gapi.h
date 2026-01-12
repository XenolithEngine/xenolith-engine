/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_GAPI_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_GAPI_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/mode.h>
#include <sprt/runtime/window/surface_info.h>

namespace sprt::window::gapi {

static constexpr uint16_t InstanceDefaultDevice = Max<uint16_t>;

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

SPRT_DEFINE_ENUM_AS_MASK(InstanceFlags)

struct SPRT_API InstanceBackendInfo : public Ref {
	virtual ~InstanceBackendInfo() = default;
};

struct SPRT_API InstancePlatformInfo : public Ref {
	virtual ~InstancePlatformInfo() = default;
};

struct SPRT_API InstanceInfo : public Ref {
	virtual ~InstanceInfo() = default;

	InstanceApi api = InstanceApi::None;
	InstanceFlags flags = InstanceFlags::None;
	Rc<InstanceBackendInfo> backend;
};

struct SPRT_API LoopBackendInfo : public Ref {
	virtual ~LoopBackendInfo() = default;
};

struct SPRT_API LoopInfo : public Ref {
	virtual ~LoopInfo() = default;

	uint16_t deviceIdx = InstanceDefaultDevice;
	ImageFormat defaultFormat = ImageFormat::R8G8B8A8_UNORM;
	Rc<LoopBackendInfo> backend; // backend-specific data
};

class SPRT_API Instance : public Ref {
public:
	virtual ~Instance() = default;

	virtual bool isPresentationSupported() const = 0;
	virtual InstanceApi getApi() const = 0;
};

class SPRT_API Loop : public Ref {
public:
	virtual ~Loop() = default;
};

} // namespace sprt::window::gapi

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_GAPI_INFO_H_
