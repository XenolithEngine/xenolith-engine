/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTEXT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTEXT_H_

#include <sprt/runtime/window/context_info.h>
#include <sprt/runtime/window/theme_info.h>
#include <sprt/runtime/window/notifications.h>
#include <sprt/runtime/window/text_input.h>
#include <sprt/runtime/window/presentation.h>
#include <sprt/runtime/window/gapi.h>

namespace sprt::window {

class NativeWindow;
class LooperAdapter;
class ContextController;

class SPRT_API Context : public Ref {
public:
	virtual ~Context() = default;

	const ContextInfo *getInfo() const { return _info; }
	ContextController *getController() const { return _controller; }

	virtual void handleConfigurationChanged(Rc<ContextInfo> &&) = 0;

	virtual void handleNativeWindowCreated(NotNull<NativeWindow>) = 0;
	virtual void handleNativeWindowDestroyed(NotNull<NativeWindow>) = 0;
	virtual void handleNativeWindowConstraintsChanged(NotNull<NativeWindow>,
			UpdateConstraintsFlags) = 0;
	virtual void handleNativeWindowInputEvents(NotNull<NativeWindow>,
			memory::dynvector<InputEventData> &&) = 0;
	virtual void handleNativeWindowTextInput(NotNull<NativeWindow>, const TextInputState &) = 0;

	virtual void handleSystemNotification(SystemNotification) = 0;

	virtual void handleWillDestroy() = 0;
	virtual void handleDidDestroy() = 0;

	virtual void handleWillStop() = 0;
	virtual void handleDidStop() = 0;

	virtual void handleWillPause() = 0;
	virtual void handleDidPause() = 0;

	virtual void handleWillResume() = 0;
	virtual void handleDidResume() = 0;

	virtual void handleWillStart() = 0;
	virtual void handleDidStart() = 0;

	virtual void handleNetworkStateChanged(NetworkFlags) = 0;
	virtual void handleThemeInfoChanged(const ThemeInfo &) = 0;

	virtual void handleGraphicsLoaded(NotNull<gapi::Loop>) = 0;

	virtual bool configureWindow(NotNull<WindowInfo>) = 0;

	virtual Rc<sprt::window::gapi::Instance> makeInstance(
			NotNull<sprt::window::gapi::InstanceInfo>) = 0;

	virtual Rc<gapi::Loop> makeLoop(NotNull<gapi::Instance>, NotNull<gapi::LoopInfo>) = 0;

	virtual memory::pool_t *getTmpPool() const = 0;

	virtual Bytes saveState() const = 0;

	virtual const gapi::Loop *getGlLoop() const = 0;

protected:
	Rc<ContextInfo> _info;
	Rc<ContextController> _controller;
};

} // namespace sprt::window

#endif
