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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWS_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWS_H_

#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/input.h>

#if SPRT_WINDOWS

#define XL_WIN32_DEBUG 0

#ifndef XL_WIN32_LOG
#if XL_WIN32_DEBUG
#define XL_WIN32_LOG(...) log::source().debug("Win32", __VA_ARGS__)
#else
#define XL_WIN32_LOG(...)
#endif
#endif

namespace sprt::window {

struct SPRT_API KeyCodes {
	static const KeyCodes &getInstance();
	static InputModifier getKeyMods();

	InputKeyCode keycodes[512];
	uint16_t scancodes[toInt(InputKeyCode::Max)];

protected:
	KeyCodes();
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWS_H_
