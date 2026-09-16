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

#ifndef XENOLITH_BACKEND_GLES_XLGLESINSTANCE_H_
#define XENOLITH_BACKEND_GLES_XLGLESINSTANCE_H_

#include "XLGles.h"
#include "XLGlesTable.h"
#include "XLCoreInstance.h"

#include <sprt/runtime/window/surface_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// What the application layer hands to the GLES instance: a snapshot of the window system the
// controller runs on (the same data the Vulkan branch consumes). EGL needs it twice: to decide
// which platform extensions matter for presentation, and to build window surfaces from the
// native handles.
struct SP_PUBLIC InstanceBackendInfo : public core::InstanceBackendInfo {
	virtual ~InstanceBackendInfo() = default;

	// nothing to encode
	virtual Value encode() const override { return Value(); }

	sprt::window::SurfaceSupportInfo supportInfo;
};

class SP_PUBLIC Instance final : public core::Instance {
public:
	virtual ~Instance();

	Instance(core::InstanceFlags, Rc<InstanceBackendInfo> &&, sprt::Dso &&);

	// Probes the EGL stack once: enumerates EGL devices, reads the GL strings of each through a
	// temporary context and logs the extension recon. Fails (and the instance is dropped) when
	// no usable GLES 3.x context can be created at all.
	bool init();

	virtual size_t getDeviceCount() const override;
	virtual bool readDeviceProperties(size_t, sprt::window::gapi::DeviceProperties &) override;

	virtual Rc<core::Loop> makeLoop(NotNull<sprt::dispatch::Looper>,
			Rc<core::LoopInfo> &&) const override;

	// Selects the device by LoopInfo.deviceIdx (the sentinel means "any", i.e. the first one) and
	// opens its display/context. Fails when no usable context can be created on that display.
	Rc<Device> makeDevice(const core::LoopInfo &) const;

	const EglTable &getTable() const { return _egl; }
	const InstanceBackendInfo *getBackendInfo() const { return _backendInfo; }

	// Open a display for the given platform extension and native handle (the EXT twin is used when
	// the 1.5 core entrypoint is missing). EGL_NO_DISPLAY when neither exists. Shared with Device,
	// which reopens the same way its probe did.
	EGLDisplay getPlatformDisplay(EGLenum platform, void *native) const;

private:
	bool probeGlDevice(EGLDisplay, DeviceInfo &);
	void probeXcbVisual(const char *clientExtensions);
	void logRecon(const char *clientExtensions) const;

	EglTable _egl;
	Rc<InstanceBackendInfo> _backendInfo;
	Vector<DeviceInfo> _devices;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESINSTANCE_H_ */
