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

#ifndef XENOLITH_APPLICATION_WINDOWS_XLWINDOWSWINDOWCLASS_H_
#define XENOLITH_APPLICATION_WINDOWS_XLWINDOWSWINDOWCLASS_H_

#include "XLWindows.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class WindowsWindow;
class WindowsContextController;

class WindowClass : public Ref {
public:
	virtual ~WindowClass();

	bool init(WideStringView);

	WideStringView getName() const { return _name; }
	HINSTANCE getModule() const { return _module; }

	void attachWindow(WindowsWindow *);
	void detachWindow(WindowsWindow *);

protected:
	static LRESULT _wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT wndProc(WindowsWindow *, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	WindowsWindow *getWindow(HWND) const;

	WideString _name;
	HINSTANCE _module = nullptr;
	WNDCLASSW _windowClass;
	bool _registred = false;
	Map<HWND, Rc<WindowsWindow>> _windows;
};

} // namespace stappler::xenolith::platform

#endif
