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

#ifndef XENOLITH_BACKEND_GLES_XLGLESTABLE_H_
#define XENOLITH_BACKEND_GLES_XLGLESTABLE_H_

#include "XLGles.h"

// The backend never talks to a native window system through EGL's platform types: displays and
// surfaces are created through the EGL_EXT/EGL_KHR platform extensions with our own handle
// casts, so the X11 headers eglplatform.h would otherwise pull in are not needed.
#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include <sprt/runtime/utils/dso.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// Function pointers for the loaded EGL and OpenGL ES stacks. EGL entrypoints come from the
// libEGL shared object (opened through sprt::Dso - there is no link-time dependency, a machine
// without GL still loads the module); GL entrypoints are resolved with eglGetProcAddress once a
// context exists, with libGLESv2 as the fallback carrier some dispatchers need for core calls.
//
// Only the subset the current milestone uses is resolved: the table grows as the backend does.
struct SP_PUBLIC EglTable {
	EglTable() = default;
	EglTable(EglTable &&) = default;
	EglTable &operator=(EglTable &&) = default;

	// Resolve the EGL core entrypoints from an opened libEGL. Leaves the table empty (operator
	// bool is false) when a required entrypoint is missing.
	void loadEgl(sprt::Dso &);

	// Resolve GL entrypoints through eglGetProcAddress. Requires eglMakeCurrent to have been
	// called on the calling thread: dispatchers are allowed to answer per-context.
	void loadGl();

	// The M0 minimum: enough EGL to probe a device and enough GL to name it.
	explicit operator bool() const {
		return eglGetError && eglGetDisplay && eglInitialize && eglTerminate && eglQueryString
				&& eglBindAPI && eglChooseConfig && eglCreateContext && eglDestroyContext
				&& eglCreatePbufferSurface && eglDestroySurface && eglMakeCurrent
				&& eglGetProcAddress;
	}

	// --- EGL core ---
	decltype(&eglGetError) eglGetError = nullptr;
	decltype(&eglGetDisplay) eglGetDisplay = nullptr;
	decltype(&eglInitialize) eglInitialize = nullptr;
	decltype(&eglTerminate) eglTerminate = nullptr;
	decltype(&eglQueryString) eglQueryString = nullptr;
	decltype(&eglBindAPI) eglBindAPI = nullptr;
	decltype(&eglChooseConfig) eglChooseConfig = nullptr;
	decltype(&eglGetConfigAttrib) eglGetConfigAttrib = nullptr;
	decltype(&eglCreateContext) eglCreateContext = nullptr;
	decltype(&eglDestroyContext) eglDestroyContext = nullptr;
	decltype(&eglCreatePbufferSurface) eglCreatePbufferSurface = nullptr;
	decltype(&eglDestroySurface) eglDestroySurface = nullptr;
	decltype(&eglMakeCurrent) eglMakeCurrent = nullptr;
	decltype(&eglGetProcAddress) eglGetProcAddress = nullptr;
	decltype(&eglReleaseThread) eglReleaseThread = nullptr;

	// EGL 1.5 entrypoint; older libEGL exports only the EXT twin, resolved below.
	decltype(&eglGetPlatformDisplay) eglGetPlatformDisplay = nullptr;

	// --- Windowed WSI (M2): create an EGLWindowSurface on the session's platform display and
	//     swap it to the screen. Resolved through the DSO like the rest; null when the driver
	//     lacks them, in which case windowed presentation is unavailable (headless still works).
	PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = nullptr;
	decltype(&eglSwapBuffers) eglSwapBuffers = nullptr;

	// --- EGL extensions, resolved through eglGetProcAddress ---
	PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = nullptr;
	PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT = nullptr;
	PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT = nullptr;
	PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT = nullptr;

	// --- GLES 3.x core, resolved through eglGetProcAddress ---
	decltype(&glGetString) glGetString = nullptr;
	decltype(&glGetStringi) glGetStringi = nullptr;
	decltype(&glGetIntegerv) glGetIntegerv = nullptr;
	decltype(&glGetError) glGetError = nullptr;
	decltype(&glFinish) glFinish = nullptr;
	decltype(&glFlush) glFlush = nullptr;

	// buffers
	decltype(&glGenBuffers) glGenBuffers = nullptr;
	decltype(&glDeleteBuffers) glDeleteBuffers = nullptr;
	decltype(&glBindBuffer) glBindBuffer = nullptr;
	decltype(&glBufferData) glBufferData = nullptr;
	decltype(&glBufferSubData) glBufferSubData = nullptr;
	// Declared by hand: the GLES3 headers this build resolves do not expose the prototype, though
	// every ES 3.0 implementation exports it (it is core, not an extension).
	using GlGetBufferSubDataProc = void (*)(GLenum target, GLintptr offset, GLsizeiptr size, void *params);
	GlGetBufferSubDataProc glGetBufferSubData = nullptr;
	decltype(&glMapBufferRange) glMapBufferRange = nullptr;
	decltype(&glUnmapBuffer) glUnmapBuffer = nullptr;

	// textures
	decltype(&glGenTextures) glGenTextures = nullptr;
	decltype(&glDeleteTextures) glDeleteTextures = nullptr;
	decltype(&glBindTexture) glBindTexture = nullptr;
	decltype(&glTexParameteri) glTexParameteri = nullptr;
	decltype(&glTexStorage2D) glTexStorage2D = nullptr;
	decltype(&glTexSubImage2D) glTexSubImage2D = nullptr;

	// framebuffers
	decltype(&glGenFramebuffers) glGenFramebuffers = nullptr;
	decltype(&glDeleteFramebuffers) glDeleteFramebuffers = nullptr;
	decltype(&glBindFramebuffer) glBindFramebuffer = nullptr;
	decltype(&glFramebufferTexture2D) glFramebufferTexture2D = nullptr;
	decltype(&glCheckFramebufferStatus) glCheckFramebufferStatus = nullptr;
	// Windowed WSI (M2): copy a rendered texture onto the default framebuffer (the window
	// surface). Core GLES 3.0.
	decltype(&glBlitFramebuffer) glBlitFramebuffer = nullptr;
	// Desktop-GL entrypoint (not part of the ES API, absent from the GLES3 headers): desktop
	// drivers behind EGL still export it. Used for diagnostics only - callers must null-check.
	using GlGetFramebufferAttachmentProc = void (*)(GLenum target, GLenum attachment,
			GLenum pname, GLint *params);
	GlGetFramebufferAttachmentProc glGetFramebufferAttachment = nullptr;
	decltype(&glInvalidateFramebuffer) glInvalidateFramebuffer = nullptr;

	// sampler objects (separable filtering state, GLES 3.0+)
	decltype(&glGenSamplers) glGenSamplers = nullptr;
	decltype(&glDeleteSamplers) glDeleteSamplers = nullptr;
	decltype(&glBindSampler) glBindSampler = nullptr;
	decltype(&glSamplerParameteri) glSamplerParameteri = nullptr;

	// sync objects
	decltype(&glFenceSync) glFenceSync = nullptr;
	decltype(&glDeleteSync) glDeleteSync = nullptr;
	decltype(&glClientWaitSync) glClientWaitSync = nullptr;

	// draw state used by the pass executor
	decltype(&glViewport) glViewport = nullptr;
	decltype(&glClearBufferfv) glClearBufferfv = nullptr;
	decltype(&glReadPixels) glReadPixels = nullptr;
	decltype(&glPixelStorei) glPixelStorei = nullptr;

	// --- M2: shaders, programs, VAOs, draws and per-draw state ---
	decltype(&glCreateShader) glCreateShader = nullptr;
	decltype(&glDeleteShader) glDeleteShader = nullptr;
	decltype(&glShaderSource) glShaderSource = nullptr;
	decltype(&glCompileShader) glCompileShader = nullptr;
	decltype(&glGetShaderiv) glGetShaderiv = nullptr;
	decltype(&glGetShaderInfoLog) glGetShaderInfoLog = nullptr;

	decltype(&glCreateProgram) glCreateProgram = nullptr;
	decltype(&glDeleteProgram) glDeleteProgram = nullptr;
	decltype(&glAttachShader) glAttachShader = nullptr;
	decltype(&glLinkProgram) glLinkProgram = nullptr;
	decltype(&glUseProgram) glUseProgram = nullptr;
	decltype(&glGetProgramiv) glGetProgramiv = nullptr;
	decltype(&glGetProgramInfoLog) glGetProgramInfoLog = nullptr;

	decltype(&glGenVertexArrays) glGenVertexArrays = nullptr;
	decltype(&glBindVertexArray) glBindVertexArray = nullptr;
	decltype(&glDeleteVertexArrays) glDeleteVertexArrays = nullptr;
	decltype(&glVertexAttribPointer) glVertexAttribPointer = nullptr;
	decltype(&glEnableVertexAttribArray) glEnableVertexAttribArray = nullptr;
	decltype(&glDisableVertexAttribArray) glDisableVertexAttribArray = nullptr;
	// Integer vertex attributes: the flat layout carries two uint words, and a driver that
	// delivers an int-typed array through the float entrypoint is not guaranteed correct.
	decltype(&glVertexAttribIPointer) glVertexAttribIPointer = nullptr;
	// Constant (non-array) attribute value for a disabled array slot.
	decltype(&glVertexAttrib4f) glVertexAttrib4f = nullptr;
	decltype(&glGetVertexAttribiv) glGetVertexAttribiv = nullptr;
	decltype(&glGetVertexAttribPointerv) glGetVertexAttribPointerv = nullptr;
	decltype(&glIsEnabled) glIsEnabled = nullptr;

	// Base-vertex draws (the desktop GL 4.3 family) are not part of the ES API, so a span's
	// indexes are rewritten into a contiguous block at record time and drawn with these instead.
	decltype(&glDrawArrays) glDrawArrays = nullptr;
	decltype(&glDrawElements) glDrawElements = nullptr;
	decltype(&glDrawElementsInstanced) glDrawElementsInstanced = nullptr;

	// shader storage buffer binding: the flat vertex shader reads its transforms from one, and a
	// uniform array cannot hold them (the 16 KiB uniform block ceiling is reached at ~3 entries)
	decltype(&glBindBufferBase) glBindBufferBase = nullptr;

	decltype(&glEnable) glEnable = nullptr;
	decltype(&glDisable) glDisable = nullptr;
	decltype(&glBlendFuncSeparate) glBlendFuncSeparate = nullptr;
	// Separate equations: the core glBlendEquation applies one to both RGB and alpha, but a
	// BlendInfo may ask for different ones (the flat contract does not, yet - still, map it all).
	decltype(&glBlendEquationSeparate) glBlendEquationSeparate = nullptr;
	decltype(&glColorMask) glColorMask = nullptr;
	decltype(&glScissor) glScissor = nullptr;
	decltype(&glActiveTexture) glActiveTexture = nullptr;

	decltype(&glGetUniformLocation) glGetUniformLocation = nullptr;
	decltype(&glUniform1i) glUniform1i = nullptr;
	decltype(&glUniform4i) glUniform4i = nullptr;

	// The M2 draw path on top of hasGlDevice: compiling programs and executing a recorded span
	// list needs all of these, so the pass fails with a named diagnostic rather than a null call.
	bool hasGlDraw() const {
		return hasGlDevice() && glCreateShader && glDeleteShader && glShaderSource
				&& glCompileShader && glGetShaderiv && glCreateProgram && glDeleteProgram
				&& glAttachShader && glLinkProgram && glUseProgram && glGenVertexArrays
				&& glBindVertexArray && glDeleteVertexArrays && glVertexAttribPointer
				&& glEnableVertexAttribArray && glDrawArrays && glDrawElements
				&& glDrawElementsInstanced
				&& glBindBufferBase && glBlendFuncSeparate && glColorMask && glScissor
				&& glActiveTexture && glUniform4i;
	}

	// The M1 minimum on top of operator bool: everything a device needs to create objects, run
	// a clear pass and read the result back.
	bool hasGlDevice() const {
		return glGetString && glGetIntegerv && glGetError && glFinish
				&& glGenBuffers && glDeleteBuffers && glBindBuffer && glBufferData
				&& glGenTextures && glDeleteTextures && glBindTexture && glTexStorage2D
				&& glTexSubImage2D
				&& glGenFramebuffers && glDeleteFramebuffers && glBindFramebuffer
				&& glFramebufferTexture2D && glCheckFramebufferStatus
				&& glGenSamplers && glDeleteSamplers
				&& glFenceSync && glDeleteSync && glClientWaitSync
				&& glViewport && glClearBufferfv && glReadPixels;
	}

	// Open lazily by loadGl when eglGetProcAddress refuses a core GL entrypoint.
	sprt::Dso _glesModule;
};

// Does the space-separated extension list exported by eglQueryString/glGetString name this
// extension? A null list answers false.
SP_PUBLIC bool hasExtension(const char *list, StringView name);

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESTABLE_H_ */
