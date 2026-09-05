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

#ifndef XENOLITH_BACKEND_GLES_XLGLES_H_
#define XENOLITH_BACKEND_GLES_XLGLES_H_

#include "XLCore.h" // IWYU pragma: keep
#include "XLCoreInfo.h" // IWYU pragma: keep

#include "XLGlesTable.h"

// The GLES backend implements the core contract on OpenGL ES 3.1 over EGL: no command buffers
// (commands are recorded into a list and executed against the current context on submit), no
// descriptor sets (a fixed binding table built at queue compilation), one texture per draw. It
// exists for GPUs with no usable Vulkan driver - embedded Linux stacks (Mali/VideoCore/i.MX),
// older Android devices and emulators.

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

class Instance;
class Loop;
class Device;

// core::ImageFormat -> the GLES triple (internal format, pixel format, pixel type) for the
// subset the backend actually accepts. `internalFormat` is zero for anything the backend can
// not allocate - that is how callers detect an unsupported image.
// B8G8R8A8_UNORM maps to RGBA8 storage: on Linux the loop reports it as the common format
// (SPRTWinLinuxController), and a frame whose output attachment is tagged with it must be
// allocatable, exactly like it is for soft/vk where B8G8R8A8 is native. Channel semantics are
// preserved - shaders write r/g/b/a into channels and capture reads them back through
// glReadPixels(GL_RGBA) - so the tag only distinguishes byte order in memory, which nothing in
// this backend consumes raw.
struct GlFormat {
	GLenum internalFormat = 0;
	GLenum format = 0;
	GLenum type = 0;
};

inline GlFormat getGlFormat(core::ImageFormat format) {
	switch (format) {
	case core::ImageFormat::R8_UNORM:
		return GlFormat{GL_R8, GL_RED, GL_UNSIGNED_BYTE};
	case core::ImageFormat::R8G8B8A8_UNORM:
	case core::ImageFormat::B8G8R8A8_UNORM:
		return GlFormat{GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
	case core::ImageFormat::R8G8B8A8_SRGB:
		return GlFormat{GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
	default:
		return GlFormat{};
	}
}

inline uint32_t getGlPixelSize(core::ImageFormat format) {
	switch (format) {
	case core::ImageFormat::R8_UNORM: return 1; break;
	case core::ImageFormat::R8G8B8A8_UNORM:
	case core::ImageFormat::B8G8R8A8_UNORM:
	case core::ImageFormat::R8G8B8A8_SRGB: return 4; break;
	default: return 0; break;
	}
}

// GL object names travel through the core Object machinery as ObjectHandle. The name is stored
// in the pointer-sized word and never dereferenced.
inline core::ObjectHandle glObjectHandle(GLuint name) {
#if (XL_USE_64_BIT_PTR_DEFINES == 1)
	return core::ObjectHandle(reinterpret_cast<void *>(uintptr_t(name)));
#else
	return core::ObjectHandle(uint64_t(name));
#endif
}

inline GLuint glObjectName(core::ObjectHandle handle) {
	return GLuint(uintptr_t(handle.get()));
}

// The baseline the backend is designed against. A context below it can not run the flat queue
// (transforms are read from a shader storage buffer, which needs 3.1).
static constexpr uint32_t RequiredVersionMajor = 3;
static constexpr uint32_t RequiredVersionMinor = 1;

// Pack a major/minor pair the way DeviceProperties::apiVersion carries it: the same bit layout
// the Vulkan backend produces with VK_API_VERSION, so both backends decode identically.
inline constexpr uint32_t makeApiVersion(uint32_t major, uint32_t minor) {
	return (major << 22) | (minor << 12);
}

// One probed EGL device. The GL strings are read once, through a temporary context, when the
// instance is created - the GL_RENDERER string is only available with a context current, and
// readDeviceProperties must stay const and side-effect free. eglDevice/surfaceless record HOW the
// probe opened its display so Device::init can reopen exactly that one.
struct SP_PUBLIC DeviceInfo {
	String deviceName; // GL_RENDERER
	String vendor; // GL_VENDOR
	String version; // raw GL_VERSION string
	String glslVersion; // GL_SHADING_LANGUAGE_VERSION
	uint32_t majorVersion = 0;
	uint32_t minorVersion = 0;
	Vector<String> extensions;
	bool presentationSupported = false;

	EGLDeviceEXT eglDevice = nullptr; // probed through EGL_EXT_platform_device
	bool surfaceless = false;         // probed through EGL_MESA_platform_surfaceless
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLES_H_ */
