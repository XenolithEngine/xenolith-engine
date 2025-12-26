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

#include "XLAndroidClipboardListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static void ClipboardListener_handleClipChanged(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = reinterpret_cast<ClipboardListener *>(nativePointer);
	native->handleClipChanged(env);
}

static JNINativeMethod s_clipboardMethods[] = {
	{"handleClipChanged", "(J)V", reinterpret_cast<void *>(&ClipboardListener_handleClipChanged)},
};

static void registerClipboardMethods(const jni::RefClass &cl) {
	static std::atomic<int> s_counter = 0;

	if (s_counter.fetch_add(1) == 0) {
		cl.registerNatives(s_clipboardMethods);
	}
}

void ClipboardListener::finalize() {
	if (proxy && thiz) {
		proxy.finalize(thiz.ref(jni::Env::getEnv()));
	}

	thiz = nullptr;
}

void ClipboardListener::handleClipChanged(JNIEnv *env) {
	if (callback) {
		callback();
	}
}

ClipboardListener::~ClipboardListener() { finalize(); }

ClipboardListener::ClipboardListener(const jni::Ref &context, Function<void()> &&cb)
: proxy(jni::Env::getClassLoader()->findClass(context.getEnv(), ClassName)) {

	auto clazz = proxy.getClass().ref(context.getEnv());
	if (proxy) {
		registerClipboardMethods(clazz);
		thiz = proxy.create(clazz, context, reinterpret_cast<jlong>(this));
		callback = sp::move(cb);
	}
}

} // namespace stappler::xenolith::platform
