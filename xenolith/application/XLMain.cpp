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

#include "SPCommon.h" // IWYU pragma: keep
#include "SPStringView.h" // IWYU pragma: keep
#include "SPSharedModule.h"
#include "SPLog.h"

#if MODULE_XENOLITH_APPLICATION
#include "XLContext.h"
#endif

#if ANDROID

#include "jni/SPRuntimeJni.h"

#include <android/native_activity.h>
#include <android/configuration.h>

SP_EXTERN_C JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
	sprt::jni::Env::loadJava(vm);
#if MODULE_XENOLITH_APPLICATION
	auto runFn = STAPPLER_VERSIONIZED_NAMESPACE::SharedModule::acquireTypedSymbol<
			STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolRunNativeSignature>(
			STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_XENOLITH_APPLICATION_NAME,
			STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolContextRunName);
	if (runFn) {
		auto app = sprt::jni::Env::getApp()->getCurrentInfo();
		if (runFn(app.get()) != 0) {
			return -1;
		}
	} else {
		STAPPLER_VERSIONIZED_NAMESPACE::slog().error("main",
				"Fail to load entry point `Context::run` from MODULE_XENOLITH_APPLICATION_NAME");
		return -1;
	}
	return JNI_VERSION_1_6;
#else
	STAPPLER_VERSIONIZED_NAMESPACE::log::source().error("main",
			"MODULE_XENOLITH_APPLICATION is not defined for the default entry point");
	return -1;
#endif
}

SP_EXTERN_C JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *unused) {
	sprt::jni::Env::finalizeJava();
}

SP_EXTERN_C JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState,
		size_t savedStateSize) {
	auto app = sprt::jni::Env::getApp();

	auto savedData = sprt::BytesView((const uint8_t *)savedState, savedStateSize);
	if (!app->loadActivity(activity, savedData)) {
		abort();
	}
}

#else

int main(int argc, const char *argv[]) {
	// Main symbol should depend only on stappler_core for successful linkage
	// So, use SharedModule to load `Context::run`
#if MODULE_XENOLITH_APPLICATION
	auto runFn = STAPPLER_VERSIONIZED_NAMESPACE::SharedModule::acquireTypedSymbol<
			STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolRunCmdSignature>(
			STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_XENOLITH_APPLICATION_NAME,
			STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolContextRunName);
	if (runFn) {
		return runFn(argc, argv);
	}
	STAPPLER_VERSIONIZED_NAMESPACE::log::source().error("main",
			"Fail to load entry point `Context::run` from MODULE_XENOLITH_APPLICATION_NAME");
	return -1;
#else
	STAPPLER_VERSIONIZED_NAMESPACE::log::source().error("main",
			"MODULE_XENOLITH_APPLICATION is not defined for the default entry point");
	return -1;
#endif
}

#endif
