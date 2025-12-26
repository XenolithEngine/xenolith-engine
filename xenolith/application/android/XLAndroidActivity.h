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

#ifndef XENOLITH_APPLICATION_ANDROID_XLANDROIDACTIVITY_H_
#define XENOLITH_APPLICATION_ANDROID_XLANDROIDACTIVITY_H_

#include "XLAndroid.h" // IWYU pragma: keep
#include "XLAndroidInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class AndroidContextController;

class AndroidActivity : public Ref {
public:
	static constexpr StringView NetworkConnectivityClassName =
			"org.stappler.xenolith.appsupport.NetworkConnectivity";
	static constexpr StringView NetworkConnectivityClassPath =
			"org/stappler/xenolith/appsupport/NetworkConnectivity";

	static constexpr int FLAG_ACTIVITY_NEW_TASK = 268'435'456;

	virtual ~AndroidActivity() = default;

	bool init(AndroidContextController *controller, ANativeActivity *, BytesView data);

	bool run();

	virtual void notifyWindowInputEvents(Vector<core::InputEventData> &&);

	virtual void notifyEnableState(WindowState);
	virtual void notifyDisableState(WindowState);

	AndroidContextController *getController() const { return _controller; }
	ANativeActivity *getActivity() const { return _activity; }
	WindowState getDecorationState() const { return _decorationState; }
	ActivityProxy *getProxy() const;

	Context *getContext() const;

	void handleActivityResult(jint request, jint result, jobject data);
	void handleInsetsVisible(bool statusBarVisible, bool navigationVisible);
	void handleContentInsets(const Padding &);
	void handleImeInsets(const Padding &);
	void handleBackInvoked();
	void handleDisplayChanged();

	void finish();
	void handleBackButton();

	void setBackButtonHandlerEnabled(bool);

protected:
	void handleContentRectChanged(const Padding &);

	void handleInputQueueCreated(AInputQueue *);
	void handleInputQueueDestroyed(AInputQueue *);
	void *handleSaveInstanceState(size_t *outLen);

	void handleNativeWindowCreated(ANativeWindow *);
	void handleNativeWindowDestroyed(ANativeWindow *);
	void handleNativeWindowRedrawNeeded(ANativeWindow *);
	void handleNativeWindowResized(ANativeWindow *);

	void handlePause();
	void handleStart();
	void handleResume();
	void handleStop();
	void handleDestroy();

	void handleWindowFocusChanged(int focused);

	void updateInsets();

	Rc<AndroidContextController> _controller;
	ANativeActivity *_activity = nullptr;
	Rc<ActivityProxy> _proxy;

	Rc<NetworkConnectivity> _networkConnectivity;

	struct InputQueueData {
		AInputQueue *queue;
		AndroidActivity *controller;
	};

	Map<AInputQueue *, Rc<InputQueue>> _input;
	Rc<AndroidWindow> _window;
	Padding _contentInsets;
	Padding _imeInsets;
	Padding _fullInsets;

	WindowState _decorationState;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_ANDROID_XLANDROIDACTIVITY_H_
