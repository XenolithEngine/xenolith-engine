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

#include "XLGlesInstance.h"
#include "XLGlesDevice.h"
#include "XLGlesLoop.h"
#include "XLCoreLoop.h"

// Older eglext.h revisions lack the surfaceless platform token; the extension itself is probed
// by name from the client extension string at runtime.
#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

#ifndef EGL_PLATFORM_XCB_EXT
#define EGL_PLATFORM_XCB_EXT 0x31DC
#endif

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

Instance::Instance(
		core::InstanceFlags flags, Rc<InstanceBackendInfo> &&backend, sprt::Dso &&dso)
: core::Instance(core::InstanceApi::GLES, flags, sp::move(dso))
, _backendInfo(sp::move(backend)) { }

Instance::~Instance() {
	if (_egl.eglReleaseThread) {
		_egl.eglReleaseThread();
	}

	log::source().debug("GLES", "~Instance");
}

bool Instance::init() {
	_egl.loadEgl(_dsoModule);
	if (!_egl) {
		log::source().error("GLES", "Fail to resolve the EGL entrypoints");
		return false;
	}

	// Client extensions are queryable without a display since EGL 1.4; a null here means the
	// loader predates that and the platform display path is simply unavailable.
	auto clientExtensions = _egl.eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	logRecon(clientExtensions);

	auto hasDevicePlatform = hasExtension(clientExtensions, "EGL_EXT_platform_device");
	auto hasSurfaceless = hasExtension(clientExtensions, "EGL_MESA_platform_surfaceless");
	auto hasDeviceEnumeration = _egl.eglQueryDevicesEXT != nullptr
			&& hasExtension(clientExtensions, "EGL_EXT_device_enumeration");

	if (hasDeviceEnumeration && hasDevicePlatform) {
		EGLint numDevices = 0;
		if (_egl.eglQueryDevicesEXT(0, nullptr, &numDevices) && numDevices > 0) {
			Vector<EGLDeviceEXT> eglDevices(static_cast<size_t>(numDevices));
			EGLint queried = 0;
			if (_egl.eglQueryDevicesEXT(numDevices, eglDevices.data(), &queried)) {
				for (EGLint i = 0; i < queried; ++i) {
					auto dpy = getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, eglDevices[i]);
					if (dpy == EGL_NO_DISPLAY) {
						continue;
					}

					EGLint major = 0;
					EGLint minor = 0;
					if (!_egl.eglInitialize(dpy, &major, &minor)) {
						continue;
					}

					DeviceInfo device;
					device.eglDevice = eglDevices[i]; // Device reopens this exact platform device
					auto ok = probeGlDevice(dpy, device);
					_egl.eglTerminate(dpy);
					if (ok) {
						_devices.emplace_back(sp::move(device));
					}
				}
			}
		}
	}

	if (_devices.empty()) {
		// No device enumeration (or none usable): probe one pseudo-device, surfaceless first so
		// the backend works with no window system at all, default display as the fallback.
		EGLDisplay dpy = EGL_NO_DISPLAY;
		const char *probeName = "default";
		if (hasSurfaceless) {
			dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, nullptr);
			probeName = "surfaceless";
		}
		if (dpy == EGL_NO_DISPLAY) {
			dpy = _egl.eglGetDisplay(EGL_DEFAULT_DISPLAY);
			probeName = "default";
		}

		if (dpy == EGL_NO_DISPLAY) {
			log::source().error("GLES", "Fail to acquire any EGL display");
			return false;
		}

		EGLint major = 0;
		EGLint minor = 0;
		if (!_egl.eglInitialize(dpy, &major, &minor)) {
			log::source().error("GLES", "Fail to initialize the EGL display (", probeName,
					"), error ", EGLint(_egl.eglGetError()));
			return false;
		}

		DeviceInfo device;
		device.surfaceless = (StringView(probeName) == "surfaceless"); // Device reopens this way
		auto ok = probeGlDevice(dpy, device);
		_egl.eglTerminate(dpy);
		if (!ok) {
			log::source().error("GLES", "Fail to probe a GLES device");
			return false;
		}
		_devices.emplace_back(sp::move(device));
	}

	// §3.3 of the backend plan: a device reports presentation support when the session's window
	// backend has a matching EGL platform extension. Which EGLDeviceEXT actually backs the
	// session display is resolved at surface creation (M2), not here.
	auto &support = _backendInfo->supportInfo;
	auto presentable = false;
	if (support.backendMask.test(toInt(sprt::window::SurfaceBackend::Wayland))) {
		presentable = presentable
				|| hasExtension(clientExtensions, "EGL_EXT_platform_wayland")
				|| hasExtension(clientExtensions, "EGL_KHR_platform_wayland");
	}
	if (support.backendMask.test(toInt(sprt::window::SurfaceBackend::Xcb))) {
		presentable = presentable || hasExtension(clientExtensions, "EGL_EXT_platform_xcb")
				|| hasExtension(clientExtensions, "EGL_KHR_platform_xcb");
	}
	for (auto &dev : _devices) {
		dev.presentationSupported = presentable;
	}

	probeXcbVisual(clientExtensions);

	size_t idx = 0;
	for (auto &dev : _devices) {
		log::source().info("GLES", "Device ", idx, ": ", dev.deviceName, " [", dev.version, ", ",
				dev.vendor, ", presentation ", dev.presentationSupported ? "yes" : "no", "]");
		++idx;
	}

	return true;
}

size_t Instance::getDeviceCount() const { return _devices.size(); }

bool Instance::readDeviceProperties(size_t n, sprt::window::gapi::DeviceProperties &prop) {
	if (n >= _devices.size()) {
		return false;
	}

	auto &dev = _devices.at(n);
	prop.deviceName = dev.deviceName;
	prop.apiVersion = makeApiVersion(dev.majorVersion, dev.minorVersion);
	prop.driverVersion = 0;
	prop.presentationSupported = dev.presentationSupported;
	return true;
}

Rc<core::Loop> Instance::makeLoop(NotNull<sprt::dispatch::Looper> looper,
		Rc<core::LoopInfo> &&info) const {
	return Rc<Loop>::create(looper, const_cast<Instance *>(this), sp::move(info));
}

Rc<Device> Instance::makeDevice(const core::LoopInfo &info) const {
	if (_devices.empty()) {
		log::source().error("GLES", "No GLES device to create");
		return nullptr;
	}

	size_t idx = 0;
	if (info.deviceIdx != core::InstanceDefaultDevice && info.deviceIdx >= _devices.size()) {
		// Enumerated EGL devices are usually the same driver behind different platforms: an
		// out-of-range pick is a misconfiguration, not a reason to fail the launch.
		log::source().warn("GLES", "No device ", info.deviceIdx, "; using the first one");
	} else if (info.deviceIdx != core::InstanceDefaultDevice) {
		idx = size_t(info.deviceIdx);
	}

	return Rc<Device>::create(this, _devices[idx]);
}

EGLDisplay Instance::getPlatformDisplay(EGLenum platform, void *native) const {
	if (_egl.eglGetPlatformDisplay) {
		return _egl.eglGetPlatformDisplay(platform, native, nullptr);
	}
	if (_egl.eglGetPlatformDisplayEXT) {
		return _egl.eglGetPlatformDisplayEXT(platform, native, nullptr);
	}
	return EGL_NO_DISPLAY;
}

bool Instance::probeGlDevice(EGLDisplay dpy, DeviceInfo &device) {
	if (!_egl.eglBindAPI(EGL_OPENGL_ES_API)) {
		log::source().error("GLES", "Fail to bind EGL_OPENGL_ES_API, error ",
				EGLint(_egl.eglGetError()));
		return false;
	}

	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE,
	};

	EGLConfig config = nullptr;
	EGLint numConfigs = 0;
	if (!_egl.eglChooseConfig(dpy, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
		// Surfaceless platforms may not advertise pbuffer configs: accept any surface type.
		const EGLint anyAttribs[] = {
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 8,
			EGL_NONE,
		};
		if (!_egl.eglChooseConfig(dpy, anyAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
			log::source().error("GLES", "No RGBA8 GLES3-capable EGL config");
			return false;
		}
	}

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, EGLint(RequiredVersionMajor),
		EGL_CONTEXT_MINOR_VERSION, EGLint(RequiredVersionMinor),
		EGL_NONE,
	};
	auto ctx = _egl.eglCreateContext(dpy, config, EGL_NO_CONTEXT, contextAttribs);
	if (ctx == EGL_NO_CONTEXT) {
		log::source().error("GLES", "Fail to create an ES ", RequiredVersionMajor, ".",
				RequiredVersionMinor, " context, error ", EGLint(_egl.eglGetError()));
		return false;
	}

	auto displayExtensions = _egl.eglQueryString(dpy, EGL_EXTENSIONS);

	EGLSurface surface = EGL_NO_SURFACE;
	if (!hasExtension(displayExtensions, "EGL_KHR_surfaceless_context")) {
		const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
		surface = _egl.eglCreatePbufferSurface(dpy, config, pbufferAttribs);
		if (surface == EGL_NO_SURFACE) {
			log::source().error("GLES",
					"Neither a surfaceless context nor a pbuffer is available, error ",
					EGLint(_egl.eglGetError()));
			_egl.eglDestroyContext(dpy, ctx);
			return false;
		}
	}

	if (!_egl.eglMakeCurrent(dpy, surface, surface, ctx)) {
		log::source().error("GLES", "Fail to make the probe context current, error ",
				EGLint(_egl.eglGetError()));
		if (surface != EGL_NO_SURFACE) {
			_egl.eglDestroySurface(dpy, surface);
		}
		_egl.eglDestroyContext(dpy, ctx);
		return false;
	}

	_egl.loadGl();

	auto glString = [this](GLenum name) {
		if (!_egl.glGetString) {
			return String();
		}
		auto str = _egl.glGetString(name);
		return str ? String(reinterpret_cast<const char *>(str)) : String();
	};

	device.version = glString(GL_VERSION);
	device.deviceName = glString(GL_RENDERER);
	device.vendor = glString(GL_VENDOR);
	device.glslVersion = glString(GL_SHADING_LANGUAGE_VERSION);

	if (_egl.glGetIntegerv) {
		GLint value = 0;
		_egl.glGetIntegerv(GL_MAJOR_VERSION, &value);
		device.majorVersion = uint32_t(value);
		_egl.glGetIntegerv(GL_MINOR_VERSION, &value);
		device.minorVersion = uint32_t(value);

		GLint numExtensions = 0;
		_egl.glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
		if (_egl.glGetStringi && numExtensions > 0) {
			device.extensions.reserve(size_t(numExtensions));
			for (GLint i = 0; i < numExtensions; ++i) {
				auto str = _egl.glGetStringi(GL_EXTENSIONS, uint32_t(i));
				if (str) {
					device.extensions.emplace_back(reinterpret_cast<const char *>(str));
				}
			}
		}
	}

	log::source().verbose("GLES",
			"Display extensions: buffer_age=",
			hasExtension(displayExtensions, "EGL_EXT_buffer_age") ? "yes" : "no",
			", swap_buffers_with_damage=",
			hasExtension(displayExtensions, "EGL_KHR_swap_buffers_with_damage") ? "yes" : "no",
			", surfaceless_context=",
			hasExtension(displayExtensions, "EGL_KHR_surfaceless_context") ? "yes" : "no");

	_egl.eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (surface != EGL_NO_SURFACE) {
		_egl.eglDestroySurface(dpy, surface);
	}
	_egl.eglDestroyContext(dpy, ctx);

	return !device.version.empty();
}

void Instance::probeXcbVisual(const char *clientExtensions) {
	auto &support = _backendInfo->supportInfo;
	auto hasXcbPlatform = hasExtension(clientExtensions, "EGL_EXT_platform_xcb")
			|| hasExtension(clientExtensions, "EGL_KHR_platform_xcb");
	if (!support.xcb.connection || !hasXcbPlatform) {
		log::source().verbose("GLES", "xcb visual probe skipped: ",
				support.xcb.connection ? "no EGL xcb platform extension"
									   : "no xcb connection in this session");
		return;
	}

	auto dpy = getPlatformDisplay(EGL_PLATFORM_XCB_EXT, support.xcb.connection);
	if (dpy == EGL_NO_DISPLAY) {
		log::source().warn("GLES", "Fail to open an EGL display on the xcb connection");
		return;
	}

	EGLint major = 0;
	EGLint minor = 0;
	if (!_egl.eglInitialize(dpy, &major, &minor)) {
		log::source().warn("GLES", "Fail to initialize EGL on the xcb connection");
		return;
	}

	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE,
	};

	EGLConfig configs[32] = {nullptr};
	EGLint numConfigs = 0;
	if (!_egl.eglChooseConfig(dpy, attribs, configs, 32, &numConfigs) || numConfigs == 0) {
		log::source().warn("GLES", "xcb visual probe: no RGBA8 window configs on the xcb display");
		_egl.eglTerminate(dpy);
		return;
	}

	// The xcb window already exists with its own visual by the time a surface is created, so the
	// backend is usable only if one of the RGBA8 configs maps to that same visual.
	auto compatible = false;
	EGLint matchedVisual = 0;
	for (EGLint i = 0; i < numConfigs; ++i) {
		EGLint visualId = 0;
		if (_egl.eglGetConfigAttrib
				&& _egl.eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &visualId)
				&& visualId != 0 && uint32_t(visualId) == support.xcb.visual_id) {
			compatible = true;
			matchedVisual = visualId;
			break;
		}
	}

	if (compatible) {
		log::source().info("GLES", "xcb visual probe: window visual ", support.xcb.visual_id,
				" matches an EGLConfig (visual ", matchedVisual, ")");
	} else {
		log::source().warn("GLES",
				"xcb visual probe: no RGBA8 EGLConfig matches the window visual ",
				support.xcb.visual_id, " - the xcb window creation has to let the gAPI pick one");
	}

	_egl.eglTerminate(dpy);
}

void Instance::logRecon(const char *clientExtensions) const {
	auto eglVersion = _egl.eglQueryString(EGL_NO_DISPLAY, EGL_VERSION);
	auto eglVendor = _egl.eglQueryString(EGL_NO_DISPLAY, EGL_VENDOR);

	log::source().info("GLES", "EGL client: ", eglVersion ? eglVersion : "(unknown)", ", vendor: ",
			eglVendor ? eglVendor : "(unknown)");

	auto has = [&](const char *name) { return hasExtension(clientExtensions, name); };

	StringStream out;
	out << "EGL client extensions recon:"
		<< "\n\tEGL_EXT_platform_xcb: " << (has("EGL_EXT_platform_xcb") ? "yes" : "no")
		<< "\n\tEGL_EXT_platform_wayland: " << (has("EGL_EXT_platform_wayland") ? "yes" : "no")
		<< "\n\tEGL_MESA_platform_surfaceless: " << (has("EGL_MESA_platform_surfaceless") ? "yes"
																						   : "no")
		<< "\n\tEGL_EXT_platform_device: " << (has("EGL_EXT_platform_device") ? "yes" : "no")
		<< "\n\tEGL_EXT_device_enumeration: "
		<< (_egl.eglQueryDevicesEXT && has("EGL_EXT_device_enumeration") ? "yes" : "no")
		<< "\n\tEGL_EXT_buffer_age: " << (has("EGL_EXT_buffer_age") ? "yes" : "no")
		<< "\n\tEGL_KHR_swap_buffers_with_damage: "
		<< (has("EGL_KHR_swap_buffers_with_damage") ? "yes" : "no");
	log::source().info("GLES", out.str());
}

} // namespace stappler::xenolith::gles
