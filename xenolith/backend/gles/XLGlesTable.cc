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

#include "XLGlesTable.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

void EglTable::loadEgl(sprt::Dso &dso) {
	eglGetError = dso.sym<decltype(eglGetError)>("eglGetError");
	eglGetDisplay = dso.sym<decltype(eglGetDisplay)>("eglGetDisplay");
	eglInitialize = dso.sym<decltype(eglInitialize)>("eglInitialize");
	eglTerminate = dso.sym<decltype(eglTerminate)>("eglTerminate");
	eglQueryString = dso.sym<decltype(eglQueryString)>("eglQueryString");
	eglBindAPI = dso.sym<decltype(eglBindAPI)>("eglBindAPI");
	eglChooseConfig = dso.sym<decltype(eglChooseConfig)>("eglChooseConfig");
	eglGetConfigAttrib = dso.sym<decltype(eglGetConfigAttrib)>("eglGetConfigAttrib");
	eglCreateContext = dso.sym<decltype(eglCreateContext)>("eglCreateContext");
	eglDestroyContext = dso.sym<decltype(eglDestroyContext)>("eglDestroyContext");
	eglCreatePbufferSurface = dso.sym<decltype(eglCreatePbufferSurface)>("eglCreatePbufferSurface");
	eglDestroySurface = dso.sym<decltype(eglDestroySurface)>("eglDestroySurface");
	eglMakeCurrent = dso.sym<decltype(eglMakeCurrent)>("eglMakeCurrent");
	eglGetProcAddress = dso.sym<decltype(eglGetProcAddress)>("eglGetProcAddress");
	eglReleaseThread = dso.sym<decltype(eglReleaseThread)>("eglReleaseThread");

	// EGL 1.5 exports the platform display entrypoint directly; a 1.4 libEGL only has the EXT
	// twin, reachable through eglGetProcAddress like any other extension.
	eglGetPlatformDisplay = dso.sym<decltype(eglGetPlatformDisplay)>("eglGetPlatformDisplay");

	// Windowed WSI (M2): eglSwapBuffers is core EGL 1.0 and always in the library; the platform
	// window surface creator is an EXT entrypoint that most loaders also export, but resolve it
	// through eglGetProcAddress as a fallback for a thin libEGL.
	eglSwapBuffers = dso.sym<decltype(eglSwapBuffers)>("eglSwapBuffers");
	eglSwapInterval = dso.sym<decltype(eglSwapInterval)>("eglSwapInterval");
	eglCreatePlatformWindowSurfaceEXT =
			dso.sym<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC>(
					"eglCreatePlatformWindowSurfaceEXT");

	if (!eglGetProcAddress) {
		return;
	}

	eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
			eglGetProcAddress("eglGetPlatformDisplayEXT"));
	if (!eglCreatePlatformWindowSurfaceEXT) {
		// A thin libEGL keeps the window-surface creator out of its own export table; it is an
		// extension entrypoint, so eglGetProcAddress is the guaranteed route to it.
		eglCreatePlatformWindowSurfaceEXT =
				reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC>(
						eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
	}
	eglQueryDevicesEXT = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
			eglGetProcAddress("eglQueryDevicesEXT"));
	eglQueryDeviceStringEXT = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(
			eglGetProcAddress("eglQueryDeviceStringEXT"));
	eglQueryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
			eglGetProcAddress("eglQueryDisplayAttribEXT"));

#if SPRT_LINUX
	// wayland-egl ships as its own library, not as part of libEGL: a wayland window surface needs
	// it to wrap the wl_surface into the wl_egl_window the platform extension expects. A missing
	// library is not a failure - the wayland branch of createWindowSurface reports it instead, and
	// headless and xcb presentation do not use it at all.
	_waylandEglModule = sprt::Dso(StringView("libwayland-egl.so.1"));
	if (_waylandEglModule) {
		wl_egl_window_create =
				_waylandEglModule.sym<WlEglWindowCreateProc>(StringView("wl_egl_window_create"));
		wl_egl_window_destroy =
				_waylandEglModule.sym<WlEglWindowDestroyProc>(StringView("wl_egl_window_destroy"));
	}
#endif
}

void EglTable::loadGl() {
	if (!eglGetProcAddress) {
		return;
	}

	auto resolve = [this](const char *name) -> void * {
		auto proc = eglGetProcAddress(name);
		if (!proc) {
			// Some dispatchers (glvnd among them) answer eglGetProcAddress only for extension
			// entrypoints and keep the core ones in the GLES library itself.
			if (!_glesModule) {
				_glesModule = sprt::Dso(StringView("libGLESv2.so.2"));
			}
			if (_glesModule) {
				return _glesModule.sym<void *>(StringView(name), sprt::DsoSymFlags::Executable);
			}
		}
		return reinterpret_cast<void *>(proc);
	};

	glGetString = reinterpret_cast<decltype(glGetString)>(resolve("glGetString"));
	glGetStringi = reinterpret_cast<decltype(glGetStringi)>(resolve("glGetStringi"));
	glGetIntegerv = reinterpret_cast<decltype(glGetIntegerv)>(resolve("glGetIntegerv"));
	glGetError = reinterpret_cast<decltype(glGetError)>(resolve("glGetError"));
	glFinish = reinterpret_cast<decltype(glFinish)>(resolve("glFinish"));
	glFlush = reinterpret_cast<decltype(glFlush)>(resolve("glFlush"));

	glGenBuffers = reinterpret_cast<decltype(glGenBuffers)>(resolve("glGenBuffers"));
	glDeleteBuffers = reinterpret_cast<decltype(glDeleteBuffers)>(resolve("glDeleteBuffers"));
	glBindBuffer = reinterpret_cast<decltype(glBindBuffer)>(resolve("glBindBuffer"));
	glBufferData = reinterpret_cast<decltype(glBufferData)>(resolve("glBufferData"));
	glBufferSubData = reinterpret_cast<decltype(glBufferSubData)>(resolve("glBufferSubData"));
	glGetBufferSubData = reinterpret_cast<decltype(glGetBufferSubData)>(resolve("glGetBufferSubData"));
	glMapBufferRange = reinterpret_cast<decltype(glMapBufferRange)>(resolve("glMapBufferRange"));
	glUnmapBuffer = reinterpret_cast<decltype(glUnmapBuffer)>(resolve("glUnmapBuffer"));

	glGenTextures = reinterpret_cast<decltype(glGenTextures)>(resolve("glGenTextures"));
	glDeleteTextures = reinterpret_cast<decltype(glDeleteTextures)>(resolve("glDeleteTextures"));
	glBindTexture = reinterpret_cast<decltype(glBindTexture)>(resolve("glBindTexture"));
	glTexParameteri = reinterpret_cast<decltype(glTexParameteri)>(resolve("glTexParameteri"));
	glTexStorage2D = reinterpret_cast<decltype(glTexStorage2D)>(resolve("glTexStorage2D"));
	glTexSubImage2D = reinterpret_cast<decltype(glTexSubImage2D)>(resolve("glTexSubImage2D"));

	glGenFramebuffers = reinterpret_cast<decltype(glGenFramebuffers)>(resolve("glGenFramebuffers"));
	glDeleteFramebuffers =
			reinterpret_cast<decltype(glDeleteFramebuffers)>(resolve("glDeleteFramebuffers"));
	glBindFramebuffer = reinterpret_cast<decltype(glBindFramebuffer)>(resolve("glBindFramebuffer"));
	glFramebufferTexture2D =
			reinterpret_cast<decltype(glFramebufferTexture2D)>(resolve("glFramebufferTexture2D"));
	glCheckFramebufferStatus = reinterpret_cast<decltype(glCheckFramebufferStatus)>(
			resolve("glCheckFramebufferStatus"));
	glGetFramebufferAttachment =
			reinterpret_cast<decltype(glGetFramebufferAttachment)>(
					resolve("glGetFramebufferAttachment"));
	glInvalidateFramebuffer =
			reinterpret_cast<decltype(glInvalidateFramebuffer)>(resolve("glInvalidateFramebuffer"));
	glBlitFramebuffer = reinterpret_cast<decltype(glBlitFramebuffer)>(resolve("glBlitFramebuffer"));

	glGenSamplers = reinterpret_cast<decltype(glGenSamplers)>(resolve("glGenSamplers"));
	glDeleteSamplers = reinterpret_cast<decltype(glDeleteSamplers)>(resolve("glDeleteSamplers"));
	glBindSampler = reinterpret_cast<decltype(glBindSampler)>(resolve("glBindSampler"));
	glSamplerParameteri =
			reinterpret_cast<decltype(glSamplerParameteri)>(resolve("glSamplerParameteri"));

	glFenceSync = reinterpret_cast<decltype(glFenceSync)>(resolve("glFenceSync"));
	glDeleteSync = reinterpret_cast<decltype(glDeleteSync)>(resolve("glDeleteSync"));
	glClientWaitSync = reinterpret_cast<decltype(glClientWaitSync)>(resolve("glClientWaitSync"));

	glViewport = reinterpret_cast<decltype(glViewport)>(resolve("glViewport"));
	glClearBufferfv = reinterpret_cast<decltype(glClearBufferfv)>(resolve("glClearBufferfv"));
	glReadPixels = reinterpret_cast<decltype(glReadPixels)>(resolve("glReadPixels"));
	glPixelStorei = reinterpret_cast<decltype(glPixelStorei)>(resolve("glPixelStorei"));

	// M2: shaders and programs. glShaderSource/glGetShaderInfoLog are core entrypoints in the
	// GLES 3.x ABI, so they resolve exactly like everything above when a dispatcher keeps them
	// out of eglGetProcAddress.
	glCreateShader = reinterpret_cast<decltype(glCreateShader)>(resolve("glCreateShader"));
	glDeleteShader = reinterpret_cast<decltype(glDeleteShader)>(resolve("glDeleteShader"));
	glShaderSource = reinterpret_cast<decltype(glShaderSource)>(resolve("glShaderSource"));
	glCompileShader = reinterpret_cast<decltype(glCompileShader)>(resolve("glCompileShader"));
	glGetShaderiv = reinterpret_cast<decltype(glGetShaderiv)>(resolve("glGetShaderiv"));
	glGetShaderInfoLog = reinterpret_cast<decltype(glGetShaderInfoLog)>(resolve("glGetShaderInfoLog"));

	glCreateProgram = reinterpret_cast<decltype(glCreateProgram)>(resolve("glCreateProgram"));
	glDeleteProgram = reinterpret_cast<decltype(glDeleteProgram)>(resolve("glDeleteProgram"));
	glAttachShader = reinterpret_cast<decltype(glAttachShader)>(resolve("glAttachShader"));
	glLinkProgram = reinterpret_cast<decltype(glLinkProgram)>(resolve("glLinkProgram"));
	glUseProgram = reinterpret_cast<decltype(glUseProgram)>(resolve("glUseProgram"));
	glGetProgramiv = reinterpret_cast<decltype(glGetProgramiv)>(resolve("glGetProgramiv"));
	glGetProgramInfoLog = reinterpret_cast<decltype(glGetProgramInfoLog)>(resolve("glGetProgramInfoLog"));

	glGenVertexArrays = reinterpret_cast<decltype(glGenVertexArrays)>(resolve("glGenVertexArrays"));
	glBindVertexArray = reinterpret_cast<decltype(glBindVertexArray)>(resolve("glBindVertexArray"));
	glDeleteVertexArrays =
			reinterpret_cast<decltype(glDeleteVertexArrays)>(resolve("glDeleteVertexArrays"));
	glVertexAttribPointer =
			reinterpret_cast<decltype(glVertexAttribPointer)>(resolve("glVertexAttribPointer"));
	glEnableVertexAttribArray =
			reinterpret_cast<decltype(glEnableVertexAttribArray)>(resolve("glEnableVertexAttribArray"));
	glDisableVertexAttribArray =
			reinterpret_cast<decltype(glDisableVertexAttribArray)>(resolve("glDisableVertexAttribArray"));
	glVertexAttribIPointer =
			reinterpret_cast<decltype(glVertexAttribIPointer)>(resolve("glVertexAttribIPointer"));
	glVertexAttrib4f = reinterpret_cast<decltype(glVertexAttrib4f)>(resolve("glVertexAttrib4f"));
	glGetVertexAttribiv =
			reinterpret_cast<decltype(glGetVertexAttribiv)>(resolve("glGetVertexAttribiv"));
	glGetVertexAttribPointerv = reinterpret_cast<decltype(glGetVertexAttribPointerv)>(
			resolve("glGetVertexAttribPointerv"));
	glIsEnabled = reinterpret_cast<decltype(glIsEnabled)>(resolve("glIsEnabled"));

	glDrawArrays = reinterpret_cast<decltype(glDrawArrays)>(resolve("glDrawArrays"));
	glDrawElements = reinterpret_cast<decltype(glDrawElements)>(resolve("glDrawElements"));
	glDrawElementsInstanced =
			reinterpret_cast<decltype(glDrawElementsInstanced)>(resolve("glDrawElementsInstanced"));

	glBindBufferBase = reinterpret_cast<decltype(glBindBufferBase)>(resolve("glBindBufferBase"));

	glEnable = reinterpret_cast<decltype(glEnable)>(resolve("glEnable"));
	glDisable = reinterpret_cast<decltype(glDisable)>(resolve("glDisable"));
	glBlendFuncSeparate = reinterpret_cast<decltype(glBlendFuncSeparate)>(resolve("glBlendFuncSeparate"));
	glBlendEquationSeparate =
			reinterpret_cast<decltype(glBlendEquationSeparate)>(resolve("glBlendEquationSeparate"));
	glColorMask = reinterpret_cast<decltype(glColorMask)>(resolve("glColorMask"));
	glScissor = reinterpret_cast<decltype(glScissor)>(resolve("glScissor"));
	glActiveTexture = reinterpret_cast<decltype(glActiveTexture)>(resolve("glActiveTexture"));

	glGetUniformLocation = reinterpret_cast<decltype(glGetUniformLocation)>(resolve("glGetUniformLocation"));
	glUniform1i = reinterpret_cast<decltype(glUniform1i)>(resolve("glUniform1i"));
	glUniform4i = reinterpret_cast<decltype(glUniform4i)>(resolve("glUniform4i"));
}

bool hasExtension(const char *list, StringView name) {
	if (!list) {
		return false;
	}

	bool ret = false;
	StringView(list).split<StringView::Chars<' ', '\t', '\n'>>([&](StringView ext) {
		if (!ret && ext == name) {
			ret = true;
		}
	});
	return ret;
}

} // namespace stappler::xenolith::gles
