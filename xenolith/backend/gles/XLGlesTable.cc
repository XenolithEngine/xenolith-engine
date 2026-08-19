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

	if (!eglGetProcAddress) {
		return;
	}

	eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
			eglGetProcAddress("eglGetPlatformDisplayEXT"));
	eglQueryDevicesEXT = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
			eglGetProcAddress("eglQueryDevicesEXT"));
	eglQueryDeviceStringEXT = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(
			eglGetProcAddress("eglQueryDeviceStringEXT"));
	eglQueryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
			eglGetProcAddress("eglQueryDisplayAttribEXT"));
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
	glInvalidateFramebuffer =
			reinterpret_cast<decltype(glInvalidateFramebuffer)>(resolve("glInvalidateFramebuffer"));

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
