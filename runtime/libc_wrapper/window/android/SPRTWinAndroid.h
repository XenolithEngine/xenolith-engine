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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROID_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROID_H_

#include <sprt/jni/jni.h>
#include <sprt/runtime/window/types.h>

#if SPRT_ANDROID

#define XL_ANDROID_DEBUG 0

#ifndef XL_ANDROID_LOG
#if XL_ANDROID_DEBUG
#define XL_ANDROID_LOG(...) log::vpdebug(__SPRT_LOCATION, "Android", __VA_ARGS__)
#else
#define XL_ANDROID_LOG(...)
#endif
#endif

namespace sprt::window {

struct NetworkConnectivity;
struct ActivityProxy;

struct NativeBufferFormatSupport {
	static NativeBufferFormatSupport get();

	bool valid = false;
	bool R8G8B8A8_UNORM = true;
	bool R8G8B8X8_UNORM = true;
	bool R8G8B8_UNORM = true;
	bool R5G6B5_UNORM = true;
	bool R16G16B16A16_FLOAT = true;
	bool R10G10B10A2_UNORM = true;
};

struct SPRT_API ActivityProxy : public sprt::Ref {
	struct Activity : jni::ClassProxy {
		jni::Method<"getString", jstring(jint)> getString = this;
		jni::Method<"getPackageName", jstring()> getPackageName = this;

		jni::Method<"setNative", void(jlong)> setNative = this;

		jni::Method<"startActivity", void(jni::L<"android/content/Intent">)> startActivity = this;
		jni::Method<"runInput", void(jstring, jint, jint, jint)> runInput = this;
		jni::Method<"updateInput", void(jstring, jint, jint, jint)> updateInput = this;
		jni::Method<"updateCursor", void(jint, jint)> updateCursor = this;
		jni::Method<"cancelInput", void()> cancelInput = this;

		jni::Method<"getWindow", jni::L<"android/view/Window">()> getWindow = this;
		jni::Method<"getDisplay", jni::L<"android/view/Display">()> getDisplay = this;
		jni::Method<"getWindowManager", jni::L<"android/view/WindowManager">()> getWindowManager =
				this;
		jni::Method<"getPackageManager", jni::L<"android/content/pm/PackageManager">()>
				getPackageManager = this;
		jni::Method<"getContentResolver", jni::L<"android.content.ContentResolver">()>
				getContentResolver = this;

		jni::Method<"finishAffinity", void()> finishAffinity = this;
		jni::Method<"finish", void()> finish = this;
		jni::Method<"onBackPressed", void()> onBackPressed = this;
		jni::Method<"setBackButtonHandlerEnabled", void(jboolean)> setBackButtonHandlerEnabled =
				this;

		using jni::ClassProxy::ClassProxy;
	} Activity;

	jni::App *app = nullptr;

	ActivityProxy(ANativeActivity *);
};

struct SPRT_API ClipboardContentProviderProxy : jni::ClassProxy {
	jni::StaticField<"Self", jni::L<"org/stappler/runtime/ClipboardContentProvider">> Self =
			this;
	jni::Method<"getNative", jlong()> getNative = this;
	jni::Method<"setNative", void(jlong)> setNative = this;
	jni::Method<"getAuthority", jstring()> getAuthority = this;

	jni::Global thiz = nullptr;

	using jni::ClassProxy::ClassProxy;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROID_H_
