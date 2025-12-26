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

#ifndef XENOLITH_APPLICATION_MACOS_XLWINDOWS_H_
#define XENOLITH_APPLICATION_MACOS_XLWINDOWS_H_

#include "XLContextInfo.h"
#include "XLCoreTextInput.h"
#include "SPPlatformUnistd.h"

#define XL_WIN32_DEBUG 0

#ifndef XL_WIN32_LOG
#if XL_WIN32_DEBUG
#define XL_WIN32_LOG(...) log::source().debug("Win32", __VA_ARGS__)
#else
#define XL_WIN32_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct KeyCodes {
	static const KeyCodes &getInstance();
	static core::InputModifier getKeyMods();

	core::InputKeyCode keycodes[512];
	uint16_t scancodes[toInt(core::InputKeyCode::Max)];

protected:
	KeyCodes();
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_MACOS_XLWINDOWS_H_
