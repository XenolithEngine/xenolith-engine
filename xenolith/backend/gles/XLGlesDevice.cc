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

	// Reopen the display the probe used: a platform device when one was enumerated, the
	// surfaceless platform when the session has no window system, plain default otherwise.
	EGLDisplay dpy = EGL_NO_DISPLAY;
	if (info.eglDevice != nullptr) {
		dpy = instance->getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, info.eglDevice);
	} else if (info.surfaceless) {
		dpy = instance->getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, nullptr);
	}
	if (dpy == EGL_NO_DISPLAY) {
		dpy = table.eglGetDisplay(EGL_DEFAULT_DISPLAY);
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
			table.eglTerminate(_display);
			_display = EGL_NO_DISPLAY;
		}
	};

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
	if (!hasExtension(displayExtensions, "EGL_KHR_surfaceless_context")) {
		const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
		surface = table.eglCreatePbufferSurface(dpy, config, pbufferAttribs);
		if (surface == EGL_NO_SURFACE) {
			log::source().error("gles::Device",
					"Neither a surfaceless context nor a pbuffer is available, error ",
					EGLint(table.eglGetError()));
			teardown();
			return false;
		}
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
	log::source().info("gles::Device", "Context ready on ", info.deviceName, " (", info.version, ")");

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
		t.eglTerminate(_display);
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
