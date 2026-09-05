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

#include "XLGlesDevice.h"
#include "XLGlesObject.h"
#include "XLGlesTextureSet.h"

// Older eglext.h revisions lack these tokens; the same guards Instance.cc carries.
#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool Device::init(const Instance *instance, const DeviceInfo &info) {
	auto &table = instance->getTable();

	if (!core::Device::init(instance)) {
		return false;
	}

	_deviceInfo = info;

	// The render context and the window surface it presents must live on the SAME EGLDisplay. A
	// headless device (no session window handle) reopens whatever display the probe used - a GPU
	// platform device or the surfaceless platform - exactly as before. A windowed device instead
	// opens the session's own wayland/xcb platform display, because only that display can both
	// render into textures and create the EGLWindowSurface the compositor presents. The support
	// snapshot travels with the instance; an empty backendMask is what a headless controller
	// reports, so it is the clean discriminator.
	auto &support = instance->getBackendInfo()->supportInfo;
	EGLDisplay dpy = EGL_NO_DISPLAY;
	if (support.backendMask.test(toInt(sprt::window::SurfaceBackend::Wayland))
			&& support.wayland.display) {
		dpy = instance->getPlatformDisplay(EGL_PLATFORM_WAYLAND_EXT, support.wayland.display);
		log::source().info("gles::Device", "WSI: opening wayland platform display ",
				support.wayland.display ? "ok" : "null-handle");
	} else if (support.backendMask.test(toInt(sprt::window::SurfaceBackend::Xcb))
			&& support.xcb.connection) {
		dpy = instance->getPlatformDisplay(EGL_PLATFORM_XCB_EXT, support.xcb.connection);
		log::source().info("gles::Device", "WSI: opening xcb platform display");
	} else {
		log::source().info("gles::Device", "WSI: no session window handle (headless path), "
				"backendMask wayland=",
				support.backendMask.test(toInt(sprt::window::SurfaceBackend::Wayland)),
				" wl.display=", support.wayland.display ? "set" : "null");
	}

	// Windowed when a session platform display was opened; the config below must then also carry
	// EGL_WINDOW_BIT so the same config can back an EGLWindowSurface. Such a display belongs to the
	// window system's connection, not to us - end() records that by leaving it alone.
	bool windowed = dpy != EGL_NO_DISPLAY;
	_ownsDisplay = !windowed;
	if (!windowed) {
		if (info.eglDevice != nullptr) {
			dpy = instance->getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, info.eglDevice);
		} else if (info.surfaceless) {
			dpy = instance->getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, nullptr);
		}
		if (dpy == EGL_NO_DISPLAY) {
			dpy = table.eglGetDisplay(EGL_DEFAULT_DISPLAY);
		}
	}
	if (dpy == EGL_NO_DISPLAY) {
		log::source().error("gles::Device", "Fail to reopen the EGL display");
		return false;
	}

	EGLint major = 0;
	EGLint minor = 0;
	if (!table.eglInitialize(dpy, &major, &minor)) {
		log::source().error("gles::Device", "Fail to initialize the EGL display, error ",
				EGLint(table.eglGetError()));
		return false;
	}

	auto teardown = [&]() {
		if (_context != EGL_NO_CONTEXT) {
			table.eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			table.eglDestroyContext(_display, _context);
			_context = EGL_NO_CONTEXT;
		}
		if (_surface != EGL_NO_SURFACE) {
			table.eglDestroySurface(_display, _surface);
			_surface = EGL_NO_SURFACE;
		}
		if (_display != EGL_NO_DISPLAY) {
			// Same rule as end(): a display belonging to the session's connection is not ours to
			// terminate, not even when init fails on it.
			if (_ownsDisplay) {
				table.eglTerminate(_display);
			}
			_display = EGL_NO_DISPLAY;
		}
	};

	// Config selection. A windowed device needs a config that can back an EGLWindowSurface, so
	// WINDOW_BIT is mandatory there; PBUFFER is a bonus (a render pbuffer) but not required - the
	// surfaceless-context extension covers rendering without one. Headless devices only ever need
	// pbuffer/surfaceless. The old "any" fallback that dropped the surface-type constraint is what
	// handed back a non-window config on a windowed display and produced EGL_BAD_CONFIG at surface
	// creation, so it is gone: a windowed device fails cleanly when no window-capable config exists.
	EGLConfig config = nullptr;
	EGLint numConfigs = 0;

	// X first, and by the window's visual rather than by channel depth. An xcb window already has
	// a visual by the time a surface is created, and eglCreatePlatformWindowSurfaceEXT rejects
	// (EGL_BAD_MATCH) every config whose EGL_NATIVE_VISUAL_ID is not exactly that visual - so
	// asking for RGBA8 and hoping is how a window renders every frame and shows none. Alpha is
	// deliberately not constrained here: a plain depth-24 TrueColor window, which is what a
	// toolkit-less window creation gets, maps to a config with no alpha bits at all.
	if (windowed && support.backendMask.test(toInt(sprt::window::SurfaceBackend::Xcb))
			&& support.xcb.visual_id != 0) {
		const EGLint visualAttribs[] = {
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_NONE,
		};

		EGLConfig configs[64] = {nullptr};
		EGLint count = 0;
		if (table.eglChooseConfig(dpy, visualAttribs, configs, 64, &count) && count > 0) {
			for (EGLint i = 0; i < count && !config; ++i) {
				EGLint visualId = 0;
				if (table.eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &visualId)
						&& uint32_t(visualId) == support.xcb.visual_id) {
					config = configs[i];
					numConfigs = 1;
				}
			}
		}

		if (config) {
			log::source().info("gles::Device", "WSI: config picked by the xcb window visual ",
					support.xcb.visual_id);
		} else {
			log::source().warn("gles::Device", "No EGL window config maps to the xcb window visual ",
					support.xcb.visual_id, "; falling back to a plain RGBA8 config - the window "
					"surface will be refused unless the window system can be told which visual to "
					"use");
		}
	}

	if (!config && windowed) {
		const EGLint windowAttribs[] = {
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 8,
			EGL_NONE,
		};
		if (!table.eglChooseConfig(dpy, windowAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
			const EGLint windowOnlyAttribs[] = {
				EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_ALPHA_SIZE, 8,
				EGL_NONE,
			};
			if (!table.eglChooseConfig(dpy, windowOnlyAttribs, &config, 1, &numConfigs)
					|| numConfigs == 0) {
				log::source().error("gles::Device", "No RGBA8 GLES3 EGL config with EGL_WINDOW_BIT");
				teardown();
				return false;
			}
		}
	} else if (!config) {
		const EGLint configAttribs[] = {
			EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 8,
			EGL_NONE,
		};
		if (!table.eglChooseConfig(dpy, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
			const EGLint anyAttribs[] = {
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_ALPHA_SIZE, 8,
				EGL_NONE,
			};
			if (!table.eglChooseConfig(dpy, anyAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
				log::source().error("gles::Device", "No RGBA8 GLES3-capable EGL config");
				teardown();
				return false;
			}
		}
	}
	_config = config;

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, EGLint(RequiredVersionMajor),
		EGL_CONTEXT_MINOR_VERSION, EGLint(RequiredVersionMinor),
		EGL_NONE,
	};
	auto ctx = table.eglCreateContext(dpy, config, EGL_NO_CONTEXT, contextAttribs);
	if (ctx == EGL_NO_CONTEXT) {
		log::source().error("gles::Device", "Fail to create an ES ", RequiredVersionMajor, ".",
				RequiredVersionMinor, " context, error ", EGLint(table.eglGetError()));
		teardown();
		return false;
	}
	_context = ctx;

	auto displayExtensions = table.eglQueryString(dpy, EGL_EXTENSIONS);
	EGLSurface surface = EGL_NO_SURFACE;
	bool surfacelessOk = hasExtension(displayExtensions, "EGL_KHR_surfaceless_context");

	// A display extension, so it is a property of THIS display and has to be asked here rather than
	// of the client string, which does not list it.
	_swapWithDamage = table.eglSwapBuffersWithDamageKHR != nullptr
			&& hasExtension(displayExtensions, "EGL_KHR_swap_buffers_with_damage");

	// A render pbuffer is only creatable when the config advertises PBUFFER_BIT (a window-only
	// config does not). When neither that nor a surfaceless context is available there is nothing
	// to make current - which is fine on a display that supports surfaceless contexts and fatal
	// otherwise.
	EGLint surfaceType = 0;
	if (table.eglGetConfigAttrib && table.eglGetConfigAttrib(dpy, config, EGL_SURFACE_TYPE, &surfaceType)) {
		bool hasPbuffer = (surfaceType & EGL_PBUFFER_BIT) != 0;
		if (!surfacelessOk && hasPbuffer) {
			const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
			surface = table.eglCreatePbufferSurface(dpy, config, pbufferAttribs);
			if (surface == EGL_NO_SURFACE) {
				log::source().error("gles::Device",
						"Fail to create the render pbuffer surface, error ",
						EGLint(table.eglGetError()));
				teardown();
				return false;
			}
		}
	}

	if (surface == EGL_NO_SURFACE && !surfacelessOk) {
		log::source().error("gles::Device",
				"Neither a surfaceless context nor a pbuffer is available");
		teardown();
		return false;
	}
	_surface = surface;

	if (!table.eglMakeCurrent(dpy, surface, surface, ctx)) {
		log::source().error("gles::Device", "Fail to make the context current on this thread, error ",
				EGLint(table.eglGetError()));
		teardown();
		return false;
	}

	// The instance already resolved the GL entrypoints while its probe context was current; core
	// GLES addresses are stable per display, so this context reuses them as-is.
	if (!table.hasGlDevice()) {
		log::source().error("gles::Device", "The driver is missing GLES entrypoints the backend needs");
		teardown();
		return false;
	}

	_display = dpy;
	_alive.store(true);
	log::source().info("gles::Device", "Context ready on ", info.deviceName, " (", info.version,
			") windowed=", windowed ? "yes" : "no",
			// Whether a present can carry a damage region is not visible from anywhere else, and a
			// missing extension looks exactly like a working one: the picture is the same, the
			// compositor just repaints more of the screen.
			windowed ? (_swapWithDamage ? " swap-with-damage=yes" : " swap-with-damage=no") : "");

	// The same two formats the probe's config guarantees: everything else the backend could accept
	// (R8G8B8A8_SRGB) is a texture-level feature, not an allocation one.
	_colorFormats.emplace_back(core::ImageFormat::R8G8B8A8_UNORM);
	_colorFormats.emplace_back(core::ImageFormat::R8_UNORM);

	// One "queue family": submission is synchronous on the loop thread, so a single
	// graphics/transfer queue models it exactly. Compute is not offered - there is no compute path.
	auto &family = _families.emplace_back(core::DeviceQueueFamily());
	family.index = 0;
	family.count = 1;
	family.preferred = core::QueueFlags::Graphics;
	family.flags = core::QueueFlags::Graphics | core::QueueFlags::Transfer;
	family.queues.emplace_back(Rc<core::DeviceQueue>::create(*this, 0, family.flags));

	return true;
}

void Device::end() {
	// The compiled programs live in this device's own shader cache (Loop::compileQueue hands every
	// Shader to addProgram), and nothing else drops that cache: left alone it holds them until
	// ~Device, where core::Device::invalidateObjects reports each one as "not destroyed before
	// device destruction". They are ours to release, and here is where the context is still alive
	// to release them - vk::Device does the same before its own invalidateObjects.
	clearShaders();

	// Close the door to deferred deletions and drop what is queued: eglDestroyContext below
	// reclaims every name that was ever allocated, so there is nothing left to delete.
	{
		sprt::lock_guard<sprt::mutex> lk(_releaseMutex);
		_alive.store(false);
		_pendingReleases.clear();
	}

	auto &t = getTable();
	if (_context != EGL_NO_CONTEXT) {
		t.eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		t.eglDestroyContext(_display, _context);
		_context = EGL_NO_CONTEXT;
	}
	if (_surface != EGL_NO_SURFACE) {
		t.eglDestroySurface(_display, _surface);
		_surface = EGL_NO_SURFACE;
	}
	if (_display != EGL_NO_DISPLAY) {
		// Only a display this backend opened for itself (the surfaceless/device probe path) gets
		// terminated. A windowed display is EGL's shared handle for the session's wayland or xcb
		// connection, which is already gone by the time this runs (Context::handleWillDestroy
		// drops the window-system controller before it stops the loop), so eglTerminate would
		// marshal requests through freed proxies. Everything this device created - context,
		// render surface, GL objects - is released above.
		if (_ownsDisplay) {
			t.eglTerminate(_display);
		}
		_display = EGL_NO_DISPLAY;
	}

	core::Device::end();
}

void Device::waitIdle() const {
	// Submission is synchronous on the loop thread, so by the time control returns here there is no
	// GPU work outstanding to wait for. Deferred deletes are drained at the next tick or dropped in
	// end(), where context teardown reclaims their names.
	core::Device::waitIdle();
}

void Device::scheduleRelease(Function<void()> &&fn) {
	Function<void()> keep = sp::move(fn);
	sprt::lock_guard<sprt::mutex> lk(_releaseMutex);
	if (_alive.load()) {
		_pendingReleases.emplace_back(sp::move(keep));
	}
	// else: the device is ending - its teardown reclaims this name, so drop the call.
}

void Device::drainPendingReleases() {
	for (;;) {
		Vector<Function<void()>> pending;
		{
			sprt::lock_guard<sprt::mutex> lk(_releaseMutex);
			if (!_alive.load() || _pendingReleases.empty()) {
				return;
			}
			pending = sp::move(_pendingReleases);
			_pendingReleases.clear();
		}

		for (auto &it : pending) {
			it();
		}

		// A release callback can drop the last reference to yet another object and queue more work;
		// go around again until the queue is quiet.
	}
}

bool Device::createWindowSurface(sprt::window::SurfaceBackend backend, void *nativeWindow,
		Extent2 extent, EGLSurface &out, void *&outNativeWindow) {
	auto &t = getTable();
	if (!t.eglCreatePlatformWindowSurfaceEXT || _display == EGL_NO_DISPLAY) {
		log::source().error("gles::Device", "No eglCreatePlatformWindowSurfaceEXT or no display");
		return false;
	}

	// The device's display was opened on the matching platform (wayland or xcb), but neither takes
	// the handle the window system reports: each platform extension names its own native window
	// type, and getting it wrong is an EGL_BAD_NATIVE_WINDOW at surface creation - a window that
	// renders every frame and shows none.
	EGLSurface surface = EGL_NO_SURFACE;
	void *created = nullptr;
	EGLint attribs[] = {EGL_NONE};

	switch (backend) {
	case sprt::window::SurfaceBackend::Wayland: {
		// EGL_EXT_platform_wayland: a `struct wl_egl_window *`, not the wl_surface. It is a
		// client-side object (libwayland-egl allocates the buffer queue), so it can be made before
		// the compositor has mapped anything.
		if (!t.hasWaylandEgl()) {
			log::source().error("gles::Device",
					"libwayland-egl.so.1 is not available: no windowed presentation on wayland");
			return false;
		}
		created = t.wl_egl_window_create(nativeWindow, int(extent.width), int(extent.height));
		if (!created) {
			log::source().error("gles::Device", "Fail to create the wl_egl_window for ",
					extent.width, "x", extent.height);
			return false;
		}
		surface = t.eglCreatePlatformWindowSurfaceEXT(_display, _config, created, attribs);
		break;
	}
	case sprt::window::SurfaceBackend::Xcb: {
		// EGL_EXT_platform_xcb: a POINTER to the xcb_window_t, where the pre-platform
		// eglCreateWindowSurface took the id by value. The pointer is read during the call only,
		// so a local holds it.
		auto windowId = uint32_t(reinterpret_cast<uintptr_t>(nativeWindow));
		surface = t.eglCreatePlatformWindowSurfaceEXT(_display, _config, &windowId, attribs);
		break;
	}
	default:
		log::source().error("gles::Device", "Unsupported window backend for WSI: ", toInt(backend));
		return false;
	}

	if (surface == EGL_NO_SURFACE) {
		auto err = EGLint(t.eglGetError()); // capture before any other EGL call consumes it
		EGLint visualId = 0;
		t.eglGetConfigAttrib(_display, _config, EGL_NATIVE_VISUAL_ID, &visualId);
		log::source().error("gles::Device", "Fail to create the EGL window surface, error ",
				err, " backend=", toInt(backend), " nativeWindow=",
				nativeWindow ? "set" : "null", " config visual=", int(visualId));
		if (created && t.wl_egl_window_destroy) {
			t.wl_egl_window_destroy(created);
		}
		return false;
	}

	out = surface;
	outNativeWindow = created;
	return true;
}

void Device::destroyWindowSurface(EGLSurface &surface, void *&nativeWindow) {
	auto &t = getTable();
	if (surface != EGL_NO_SURFACE) {
		if (_display != EGL_NO_DISPLAY && t.eglDestroySurface) {
			t.eglDestroySurface(_display, surface);
		}
		surface = EGL_NO_SURFACE;
	}

	// Second, and only after the EGLSurface built on it is gone: the driver holds the buffer queue
	// for as long as the surface exists.
	if (nativeWindow) {
		if (t.wl_egl_window_destroy) {
			t.wl_egl_window_destroy(nativeWindow);
		}
		nativeWindow = nullptr;
	}
}

Rc<core::Sampler> Device::getSampler(const core::SamplerInfo &info) {
	sprt::unique_lock<sprt::mutex> lock(_samplerMutex);
	for (auto &it : _samplers) {
		if (it->getInfo() == info) {
			return it;
		}
	}

	if (auto sampler = Rc<Sampler>::create(*this, info)) {
		return _samplers.emplace_back(move(sampler));
	}
	return nullptr;
}

Rc<core::Framebuffer> Device::makeFramebuffer(const core::QueuePassData *pass,
		SpanView<Rc<core::ImageView>> views) {
	return Rc<Framebuffer>::create(*this, pass, views);
}

Rc<core::ImageStorage> Device::makeImage(StringView name, const core::ImageInfoData &info) {
	if (auto image = Rc<Image>::create(*this, name, info)) {
		return Rc<core::ImageStorage>::create(move(image));
	}
	return nullptr;
}

Rc<core::Semaphore> Device::makeSemaphore() { return Rc<Semaphore>::create(*this); }

Rc<core::ImageView> Device::makeImageView(const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	return Rc<ImageView>::create(*this, image, info);
}

Rc<core::TextureSet> Device::makeTextureSet(const core::TextureSetLayout &layout) {
	return Rc<TextureSet>::create(*this, static_cast<const TextureSetLayout &>(layout));
}

} // namespace stappler::xenolith::gles
