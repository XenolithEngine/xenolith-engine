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

#ifndef XENOLITH_APPLICATION_INPUT_XLINPUT_H_
#define XENOLITH_APPLICATION_INPUT_XLINPUT_H_

#include "XLNodeInfo.h" // IWYU pragma: keep
#include "XLCoreTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using core::InputFlags;
using core::InputMouseButton;
using core::InputModifier;
using core::InputKeyCode;
using core::InputKeyComposeState;
using core::InputEventName;
using core::InputEventData;
using core::TextInputType;
using core::TextCursor;
using core::TextCursorPosition;
using core::TextCursorLength;
using core::TextInputString;
using core::TextInputState;
using core::TextInputRequest;

static constexpr float TapDistanceAllowed = 12.0f;
static constexpr float TapDistanceAllowedMulti = 32.0f;
static constexpr TimeInterval TapIntervalAllowed = TimeInterval::microseconds(300'000ULL);

enum class InputEventState {
	Declined, // Получатель не заинтересован в этой цепочке событий
	Processed, // Получатель заинтересован в цепочке событий, но не требует эксклюзивности
	Captured, // Получатель запрашивает цепочку событий в эксклюзивную обработку
	Retain, // Получатель просит получать следующие события цепочки в обход фильтрафии
	Release, // Получатель больше не хочет получать события цепочки в обход фильтрации

	DelayedProcessed, // Получатель заинтересован в цепочке событий, но не требует эксклюзивности, и просит поддерживать обработку активной в течении некоторого времени
	DelayedCaptured, // Получатель запрашивает цепочку событий в эксклюзивную обработку, и просит поддерживать обработку активной в течении некоторого времени
};

struct SP_PUBLIC InputEvent {
	InputEventData data;
	Vec2 originalLocation;
	Vec2 currentLocation;
	Vec2 previousLocation;
	uint64_t originalTime = 0;
	uint64_t currentTime = 0;
	uint64_t previousTime = 0;
	InputModifier originalModifiers = InputModifier::None;
	InputModifier previousModifiers = InputModifier::None;
};

class SP_PUBLIC TextInputViewInterface {
public:
	SP_COVERAGE_TRIVIAL
	virtual ~TextInputViewInterface() { }

	virtual void updateTextCursor(uint32_t pos, uint32_t len) = 0;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) = 0;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) = 0;
	virtual void cancelTextInput() = 0;
};

using InputEventMask = std::bitset<toInt(InputEventName::Max)>;
using InputButtonMask = std::bitset<toInt(InputMouseButton::Max)>;
using InputKeyMask = std::bitset<toInt(InputKeyCode::Max)>;

SP_PUBLIC extern InputEventMask EventMaskTouch;
SP_PUBLIC extern InputEventMask EventMaskKey;

SP_PUBLIC InputButtonMask makeButtonMask(std::initializer_list<InputMouseButton> &&);
SP_PUBLIC InputButtonMask makeButtonMask(InputMouseButton);

SP_PUBLIC InputEventMask makeEventMask(std::initializer_list<InputEventName> &&);
SP_PUBLIC InputEventMask makeEventMask(InputEventName);

SP_PUBLIC InputKeyMask makeKeyMask(std::initializer_list<InputKeyCode> &&);
SP_PUBLIC InputKeyMask makeKeyMask(InputKeyCode);

struct InputTouchInfo {
	InputButtonMask buttonMask = makeButtonMask({InputMouseButton::Touch});

	InputTouchInfo() = default;
	InputTouchInfo(InputButtonMask &&mask) : buttonMask(sp::move(mask)) { }
};

struct InputTapInfo {
	InputButtonMask buttonMask = makeButtonMask({InputMouseButton::Touch});

	// Number of recognized sequential presses
	// If you only need to recognize one press, it is highly recommended to use 1
	uint32_t maxTapCount = 2;

	// No other input listener after this will receive Tap
	// (Listeners earlier in hierarchy still can receive taps)
	bool exclusive = false;

	InputTapInfo() = default;
	InputTapInfo(uint32_t count, bool ex = false) : maxTapCount(count), exclusive(ex) { }
	InputTapInfo(InputButtonMask &&mask, uint32_t count = 2, bool ex = false)
	: buttonMask(sp::move(mask)), maxTapCount(count), exclusive(ex) { }
};

enum class InputPressFlags : uint32_t {
	None,

	// if set - send GestureEvent::Activated every time after the interval after the previous activation
	Continuous = 1 << 0,

	// if set - capture gesture input excusively
	Capture = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(InputPressFlags)

struct InputPressInfo {
	InputButtonMask buttonMask = makeButtonMask({InputMouseButton::Touch});

	// Hold time after which the press is considered completed (GestureEvent::Activated)
	TimeInterval interval = TapIntervalAllowed;

	// if false - send GestureEvent::Activated only once upon initial activation
	InputPressFlags flags = InputPressFlags::Capture;

	InputPressInfo() = default;
	InputPressInfo(InputPressFlags f) : flags(f) { }
	InputPressInfo(TimeInterval iv, InputPressFlags c = InputPressFlags::Capture)
	: interval(iv), flags(c) { }
	InputPressInfo(InputButtonMask &&mask, TimeInterval iv = TapIntervalAllowed,
			InputPressFlags c = InputPressFlags::Capture)
	: buttonMask(sp::move(mask)), interval(iv), flags(c) { }
};

struct InputSwipeInfo {
	InputButtonMask buttonMask = makeButtonMask({InputMouseButton::Touch});

	// Gesture recognition begins after a certain distance has been traveled in order to distinguish this
	// gesture from stationary touches. This parameter determines this distance
	float threshold = TapDistanceAllowed;

	// If true - include the boundary protective distance in the gesture data.
	// If false - the system assumes that the gesture begins only after the boundary distance
	bool sendThreshold = false;

	InputSwipeInfo() = default;
	InputSwipeInfo(float t, bool s = false) : threshold(t), sendThreshold(s) { }
	InputSwipeInfo(InputButtonMask &&mask, float t = TapDistanceAllowed, bool s = false)
	: buttonMask(sp::move(mask)), threshold(t), sendThreshold(s) { }
};

struct InputPinchInfo {
	InputButtonMask buttonMask = makeButtonMask({InputMouseButton::Touch});

	InputPinchInfo() = default;
	InputPinchInfo(InputButtonMask &&mask) : buttonMask(sp::move(mask)) { }
};

struct InputScrollInfo {
	uint32_t unused = 0;

	InputScrollInfo() = default;
};

struct InputMoveInfo {
	bool withinNode = true;

	InputMoveInfo(bool w = true) : withinNode(w) { }
};

struct InputMouseOverInfo {
	float padding = 0.0f;
	bool onlyFocused = true;

	InputMouseOverInfo(float p = 0.0f, bool f = true) : padding(p), onlyFocused(f) { }
};

struct InputKeyInfo {
	InputKeyMask keyMask = InputKeyMask();

	InputKeyInfo() = default;
	InputKeyInfo(InputKeyMask &&mask) : keyMask(sp::move(mask)) { }
};

constexpr const TextCursor TextCursor::InvalidCursor(maxOf<uint32_t>(), 0.0f);

} // namespace stappler::xenolith

namespace std {

template <>
struct SP_PUBLIC hash<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::InputEventData> {
	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::InputEventData &ev) const {
		return std::hash<uint32_t>{}(ev.id);
	}
};

} // namespace std

#endif /* XENOLITH_APPLICATION_INPUT_XLINPUT_H_ */
