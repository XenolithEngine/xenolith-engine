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

#ifndef XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEBUTTON_H_
#define XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEBUTTON_H_

#include "XLSimpleUiConfig.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

enum class InputNodeState : uint32_t {
	None,

	// Node is interactable by user. If not set - node should be completely inert.
	Enabled = 1 << 0,

	// Node has input focus and responds to keyboard events
	Focused = 1 << 1,

	// Node has mouse pointer over it or another way highlighted
	Hovered = 1 << 2,

	// Node is currently under user active interaction (active keyboard input or button pressed)
	Activated = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(InputNodeState)

class SP_PUBLIC Button : public Layer {
public:
	virtual ~Button() = default;

	bool init(Function<void()> &&);

	void setEnabled(bool);
	bool isEnabled() const { return hasFlag(_state, InputNodeState::Enabled); }

	void setCallback(Function<void()> &&);

protected:
	using Layer::init;

	virtual void handleFocusEnter();
	virtual void handleFocusLeave();
	virtual void handleTouch();

	virtual void updateState();

	Function<void()> _callback;
	InputListener *_listener = nullptr;
	InputNodeState _state = InputNodeState::Enabled;
};

class SP_PUBLIC ButtonWithLabel : public Button {
public:
	virtual ~ButtonWithLabel() = default;

	bool init(StringView, Function<void()> && = nullptr);

	virtual void handleContentSizeDirty() override;

	void setString(StringView);

protected:
	Label *_label = nullptr;
};

} // namespace stappler::xenolith::simpleui

#endif /* XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEBUTTON_H_ */
