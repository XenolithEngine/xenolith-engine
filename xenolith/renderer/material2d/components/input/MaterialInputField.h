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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTFIELD_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTFIELD_H_

#include "MaterialSurface.h"
#include "MaterialInputTextContainer.h"
#include "MaterialIconSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

enum class InputFieldStyle {
	Filled,
	Outlined,
};

enum class InputFieldPasswordMode {
	NotPassword,
	ShowAll,
	ShowChar,
	ShowNone,
};

enum class InputFieldError : uint32_t {
	None,
	Overflow = 1 << 0,
	InvalidChar = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(InputFieldError)

class SP_PUBLIC InputField : public Surface {
public:
	static constexpr uint32_t InputEnabledActionTag = maxOf<uint32_t>() - 2;
	static constexpr uint32_t InputEnabledLabelActionTag = maxOf<uint32_t>() - 3;

	virtual ~InputField();

	virtual bool init(InputFieldStyle = InputFieldStyle::Filled);
	virtual bool init(InputFieldStyle, const SurfaceStyle &);

	virtual void handleEnter(xenolith::Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	virtual void setLabelText(StringView);
	virtual StringView getLabelText() const;

	virtual void setSupportingText(StringView);
	virtual StringView getSupportingText() const;

	virtual void setLeadingIconName(IconName);
	virtual IconName getLeadingIconName() const;

	virtual void setTrailingIconName(IconName);
	virtual IconName getTrailingIconName() const;

	virtual void setEnabled(bool);
	virtual bool isEnabled() const;

	virtual const TextInputState &getInput() const { return _inputState; }

	virtual WideStringView getInputString() const { return _inputState.getStringView(); }

	virtual void setInputType(TextInputType);
	virtual TextInputType getInputType() const { return _inputState.type; }

	virtual void setPasswordMode(InputFieldPasswordMode);
	virtual InputFieldPasswordMode getPasswordMode() const { return _passwordMode; }

protected:
	virtual bool handleTap(const Vec2 &);

	virtual bool handlePressBegin(const Vec2 &);
	virtual bool handleLongPress(const Vec2 &, uint32_t tickCount);
	virtual bool handlePressEnd(const Vec2 &);
	virtual bool handlePressCancel(const Vec2 &);

	virtual bool handleSwipeBegin(const Vec2 &pt, const Vec2 &d);
	virtual bool handleSwipe(const Vec2 &pt, const Vec2 &d, const Vec2 &v);
	virtual bool handleSwipeEnd(const Vec2 &v);

	virtual void updateActivityState();
	virtual void updateInputEnabled();

	virtual void acquireInputFromContainer();
	virtual void acquireInput(const Vec2 &targetLocation);
	virtual void updateCursorForLocation(const Vec2 &);

	virtual void handleTextInput(const TextInputState &);

	virtual bool handleInputChar(char16_t);

	virtual void handleError(InputFieldError);

	virtual InputFieldError validateInputData(TextInputState &);

	InputFieldStyle _style = InputFieldStyle::Filled;
	InputListener *_inputListener = nullptr;
	InputListener *_focusInputListener = nullptr;
	TypescaleLabel *_labelText = nullptr;
	TypescaleLabel *_supportingText = nullptr;
	InputTextContainer *_container = nullptr;
	IconSprite *_leadingIcon = nullptr;
	IconSprite *_trailingIcon = nullptr;
	Surface *_indicator = nullptr;

	TextInputHandler _handler;
	TextInputState _inputState;
	InputFieldPasswordMode _passwordMode = InputFieldPasswordMode::NotPassword;

	float _activityAnimationDuration = 0.25f;
	bool _mouseOver = false;
	bool _enabled = true;
	bool _focused = false;
	bool _pointerSwipeCaptured = false;
	bool _containerSwipeCaptured = false;
	bool _rangeSelectionAllowed = true;
	bool _isLongPress = false;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTFIELD_H_ */
