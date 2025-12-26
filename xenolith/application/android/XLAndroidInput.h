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

#ifndef XENOLITH_APPLICATION_ANDROID_XLANDROIDINPUT_H_
#define XENOLITH_APPLICATION_ANDROID_XLANDROIDINPUT_H_

#include "XLAndroidWindow.h"
#include "XLCoreInput.h"

#include <android/input.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class AndroidActivity;

class InputQueue : public Ref {
public:
	static core::InputKeyCode KeyCodes[];

	virtual ~InputQueue();

	bool init(AndroidActivity *, AInputQueue *);

	int handleInputEventQueue(int fd, int events);
	int handleInputEvent(AInputEvent *event);
	int handleKeyEvent(AInputEvent *event);
	int handleMotionEvent(AInputEvent *event);

	void setBackButtonHandlerEnabled(bool);

	void handleBackInvoked();

protected:
	AndroidActivity *_activity = nullptr;
	AInputQueue *_queue = nullptr;

	core::InputModifier _activeModifiers = core::InputModifier::None;
	Vec2 _hoverLocation = Vec2::INVALID;

	Dso _selfHandle;

	int32_t (*_AMotionEvent_getActionButton)(const AInputEvent *) = nullptr;

	bool _backButtonHandlerEnabled = false;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_ANDROID_XLANDROIDINPUT_H_
