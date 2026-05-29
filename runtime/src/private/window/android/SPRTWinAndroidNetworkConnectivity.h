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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDNETWORKCONNECTIVITY_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDNETWORKCONNECTIVITY_H_

#include "private/window/android/SPRTWinAndroid.h" // IWYU pragma: keep

#if SPRT_ANDROID

#include <sprt/runtime/window/notifications.h>

namespace sprt::window {

struct NetworkConnectivity : public Ref {
	static constexpr StringView NetworkConnectivityClassName =
			"org.stappler.runtime.NetworkConnectivity";
	static constexpr StringView NetworkConnectivityClassPath =
			"org/stappler/runtime/NetworkConnectivity";

	struct NetworkConnectivityProxy : jni::ClassProxy {
		jni::StaticMethod<"create",
				jni::L<"org/stappler/runtime/NetworkConnectivity">(
						jni::L<"android.content.Context">, jlong)>
				create = this;
		jni::Method<"finalize", void()> finalize = this;

		using jni::ClassProxy ::ClassProxy;
	} proxy = "org/stappler/runtime/NetworkConnectivity";

	jni::Global thiz = nullptr;
	NetworkFlags flags = NetworkFlags::None;
	Function<void(NetworkFlags)> callback;

	void finalize();

	void handleCreated(JNIEnv *, jobject, jobject);
	void handleFinalized(JNIEnv *);
	void handleAvailable(JNIEnv *, jobject, jobject);
	void handleLost(JNIEnv *);
	void handleCapabilitiesChanged(JNIEnv *, jobject);
	void handleLinkPropertiesChanged(JNIEnv *, jobject);

	virtual ~NetworkConnectivity();
	NetworkConnectivity(const jni::Ref &context, Function<void(NetworkFlags)> && = nullptr);
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDNETWORKCONNECTIVITY_H_
