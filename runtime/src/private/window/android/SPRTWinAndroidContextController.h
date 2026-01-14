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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDCONTEXTCONTROLLER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDCONTEXTCONTROLLER_H_

#include "private/window/android/SPRTWinAndroid.h" // IWYU pragma: keep

#if SPRT_ANDROID

#include <sprt/runtime/window/controller.h>

struct ANativeWindow;

namespace sprt::window {

class AndroidActivity;
class ClipboardListener;

class SPRT_API AndroidContextController : public ContextController {
public:
	static constexpr int FLAG_ACTIVITY_NEW_TASK = 268'435'456;

	static void acquireDefaultConfig(ContextConfig &);

	static Rc<AndroidContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<LooperAdapter>);

	virtual ~AndroidContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<LooperAdapter>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override;
	virtual WindowCapabilities getCapabilities() const override;

	bool loadActivity(ANativeActivity *a, BytesView data);
	void destroyActivity(NotNull<AndroidActivity>);

	jni::Ref getSelf() const;

	Rc<WindowInfo> makeWindowInfo(ANativeWindow *) const;

	virtual Status readFromClipboard(Rc<ClipboardRequest> &&) override;
	virtual Status probeClipboard(Rc<ClipboardProbe> &&) override;
	virtual Status writeToClipboard(Rc<ClipboardData> &&) override;

	String getClipboardTypeForUri(StringView);
	String getClipboardPathForUri(StringView);

	virtual void openUrl(StringView) override;

protected:
	void readClipboardStream(Rc<ClipboardRequest> &&, const jni::Ref &, StringView type);

	void handleClipboardUpdate();
	void clearClipboard();

	Rc<NetworkConnectivity> _networkConnectivity;
	Rc<ClipboardListener> _clipboardListener;
	Rc<ContextContainer> _container;
	Set<Rc<AndroidActivity>> _activities;
	Rc<HandleAdapter> _stopTimer;

	jni::Global _clipboardClip = nullptr;
	Rc<ClipboardData> _clipboardData;
	String _clipboardAuthority;
	ClipboardContentProviderProxy ClipboardContentProvider =
			"org/stappler/xenolith/core/ClipboardContentProvider";
};

} // namespace sprt::window

#endif

#endif //CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDCONTEXTCONTROLLER_H_
