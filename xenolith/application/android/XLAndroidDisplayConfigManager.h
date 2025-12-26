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

#ifndef XENOLITH_APPLICATION_ANDROID_XLANDROIDDISPLAYCONFIGMANAGER_H_
#define XENOLITH_APPLICATION_ANDROID_XLANDROIDDISPLAYCONFIGMANAGER_H_

#include "XLAndroid.h"
#include "platform/XLDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class AndroidContextController;

class SP_PUBLIC AndroidDisplayConfigManager : public DisplayConfigManager {
public:
	virtual ~AndroidDisplayConfigManager();

	virtual bool init(NotNull<AndroidContextController>,
			Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate() override;

	void updateDisplayConfig(Function<void(DisplayConfig *_Nullable)> && = nullptr);

protected:
	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *_Nullable)> &&) override;
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&) override;

	struct DisplayConfigListener : jni::ClassProxy {
		jni::StaticMethod<"create",
				jni::L<"org/stappler/xenolith/core/DisplayConfigListener">(
						jni::L<"android.content.Context">, jlong)>
				create = this;
		jni::Method<"finalize", void()> finalize = this;

		using jni::ClassProxy ::ClassProxy;
	} proxy = "org/stappler/xenolith/core/DisplayConfigListener";

	jni::Global thiz = nullptr;

	Rc<AndroidContextController> _controller;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_ANDROID_XLANDROIDDISPLAYCONFIGMANAGER_H_
