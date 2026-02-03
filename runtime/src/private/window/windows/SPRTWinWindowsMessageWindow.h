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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSMESSAGEWINDOW_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSMESSAGEWINDOW_H_

#include "private/window/windows/SPRTWinWindows.h" // IWYU pragma: keep

#if SPRT_WINDOWS

#include <sprt/runtime/window/notifications.h>
#include <sprt/runtime/window/theme_info.h>
#include <sprt/runtime/window/clipboard.h>

namespace sprt::window {

class WindowsContextController;

class MessageWindow : public Ref {
public:
	static constexpr auto ClassName = L"org.stappler.xenolith.MessageWindow";

	virtual ~MessageWindow();

	virtual bool init(NotNull<WindowsContextController>);

	void setWindowRect(IRect);

	Status handleDisplayChanged(Extent2);
	Status handleNetworkStateChanged(NetworkFlags);
	Status handleSystemNotification(SystemNotification);

	Status handleSettingsChanged();

	Status readFromClipboard(Rc<ClipboardRequest> &&);
	Status writeToClipboard(Rc<ClipboardData> &&);

protected:
	struct WinRtAdapter;

	static LRESULT wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	ThemeInfo getThemeInfo();

	Rc<WindowsContextController> _controller;
	HINSTANCE _module = nullptr;
	WNDCLASSW _windowClass;
	HWND _window = nullptr;

	Rc<Ref> _networkConnectivity;

	bool _winrtInit = false;
	WinRtAdapter *_adapter = nullptr;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSMESSAGEWINDOW_H_
