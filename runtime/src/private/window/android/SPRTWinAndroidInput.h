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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDINPUT_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDINPUT_H_

#include "private/window/android/SPRTWinAndroid.h" // IWYU pragma: keep

#if SPRT_ANDROID

#include "private/SPRTDso.h"

#include <sprt/runtime/window/input.h>

struct AInputQueue;
struct AInputEvent;

namespace sprt::window {

class AndroidActivity;

class InputQueue : public Ref {
public:
	static InputKeyCode KeyCodes[];

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

	InputModifier _activeModifiers = InputModifier::None;
	Vec2 _hoverLocation = Vec2(NaN<float>, NaN<float>);

	Dso _selfHandle;

	int32_t (*_AMotionEvent_getActionButton)(const AInputEvent *) = nullptr;

	bool _backButtonHandlerEnabled = false;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_ANDROID_SPRTWINANDROIDINPUT_H_
