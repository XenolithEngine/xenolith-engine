/**
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

#include <sprt/runtime/config.h>
#include <sprt/c/bits/__sprt_def.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW && SPRT_ANDROID

#include <private/window/android/SPRTWinAndroid.h>

#include <android/hardware_buffer.h>
#include <sprt/runtime/mutex.h>
#include <sprt/jni/jni.h>
#include <sprt/jni/native_activity.h>

#include "private/SPRTDso.h"

namespace sprt::window {

static qmutex s_dataMutex;

NativeBufferFormatSupport NativeBufferFormatSupport::get() {
	static NativeBufferFormatSupport s_format;

	if (jni::Env::getApp()->sdkVersion >= 29 && !s_format.valid) {
		// check for available surface formats
		auto handle = Dso(StringView(), DsoFlags::Self);
		if (handle) {
			auto _AHardwareBuffer_isSupported = handle.sym<int (*)(const AHardwareBuffer_Desc *)>(
					"AHardwareBuffer_isSupported");
			if (_AHardwareBuffer_isSupported) {
				// check for common buffer formats
				auto checkSupported = [&](int format) -> bool {
					AHardwareBuffer_Desc desc;
					desc.width = 1'024;
					desc.height = 1'024;
					desc.layers = 1;
					desc.format = format;
					desc.usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER
							| AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
					desc.stride = 0;
					desc.rfu0 = 0;
					desc.rfu1 = 0;

					return _AHardwareBuffer_isSupported(&desc) != 0;
				};

				s_format = NativeBufferFormatSupport{
					true,
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT),
					checkSupported(AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM),
				};
			}
		}
	}
	return s_format;
}

ActivityProxy::ActivityProxy(ANativeActivity *a) : Activity(jni::Ref(a->clazz, a->env).getClass()) {
	app = jni::Env::getApp();
}

} // namespace sprt::window

#include "SPRTWinAndroidActivity.cc"
#include "SPRTWinAndroidClipboardListener.cc"
#include "SPRTWinAndroidContextController.cc"
#include "SPRTWinAndroidDisplayConfigManager.cc"
#include "SPRTWinAndroidInput.cc"
#include "SPRTWinAndroidNetworkConnectivity.cc"
#include "SPRTWinAndroidWindow.cc"

#endif
