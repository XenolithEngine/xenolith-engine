/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_CORE_XLCOREINPUT_H_
#define XENOLITH_CORE_XLCOREINPUT_H_

#include "XLCore.h"
#include <sprt/runtime/window/input.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

using sprt::window::InputFlags;
using sprt::window::InputMouseButton;
using sprt::window::InputModifier;
using sprt::window::InputKeyCode;
using sprt::window::InputKeyComposeState;
using sprt::window::InputEventName;
using sprt::window::InputEventType;
using sprt::window::InputEventDataType;
using sprt::window::InputEventData;

using sprt::window::getInputKeyCodeName;
using sprt::window::getInputKeyCodeKeyName;
using sprt::window::getInputEventName;
using sprt::window::getInputButtonName;
using sprt::window::getInputModifiersNames;

} // namespace stappler::xenolith::core

namespace std {

SP_PUBLIC inline std::ostream &operator<<(std::ostream &stream,
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::InputKeyCode v) {
	STAPPLER_VERSIONIZED_NAMESPACE::memory::makeCallback(stream) << v;
	return stream;
}

SP_PUBLIC inline std::ostream &operator<<(std::ostream &stream,
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::InputEventName v) {
	STAPPLER_VERSIONIZED_NAMESPACE::memory::makeCallback(stream) << v;
	return stream;
}

} // namespace std

#endif /* XENOLITH_CORE_XLCOREINPUT_H_ */
