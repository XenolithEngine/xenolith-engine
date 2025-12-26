/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_BUTTON_MATERIALBUTTON_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_BUTTON_MATERIALBUTTON_H_

#include "MaterialSurface.h"
#include "MaterialIconSprite.h"
#include "MaterialMenuSource.h"
#include "XLSubscriptionListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class TypescaleLabel;
class MenuSource;
class MenuSourceButton;

class SP_PUBLIC Button : public Surface {
public:
	enum NodeMask : uint32_t {
		None = 0,
		LabelText = 1 << 0,
		LabelValue = 1 << 1,
		LeadingIcon = 1 << 2,
		TrailingIcon = 1 << 3,
		Labels = LabelText | LabelValue,
		Icons = LeadingIcon | TrailingIcon,
		All = LabelText | LabelValue | LeadingIcon | TrailingIcon
	};

	static constexpr TimeInterval LongPressInterval = TimeInterval::milliseconds(350);

	virtual ~Button() { }

	virtual bool init(NodeStyle = NodeStyle::Filled, ColorRole = ColorRole::Primary,
			uint32_t schemeTag = SurfaceStyle::PrimarySchemeTag);
	virtual bool init(const SurfaceStyle &) override;

	virtual void handleContentSizeDirty() override;

	virtual void setFollowContentSize(bool);
	virtual bool isFollowContentSize() const;

	virtual void setSwallowEvents(bool);
	virtual bool isSwallowEvents() const;

	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }
	virtual bool isMenuSourceButtonEnabled() const;

	virtual void setNodeMask(NodeMask);
	virtual NodeMask getNodeMask() const { return _nodeMask; }

	virtual void setSelected(bool);
	virtual bool isSelected() const;

	virtual void setText(StringView);
	virtual StringView getText() const;

	virtual void setTextValue(StringView);
	virtual StringView getTextValue() const;

	virtual void setIconSize(float);
	virtual float getIconSize() const;

	virtual void setLeadingIconName(IconName, float progress = 0.0f);
	virtual IconName getLeadingIconName() const;

	virtual void setLeadingIconProgress(float progress, float animation = 0.0f);
	virtual float getLeadingIconProgress() const;

	virtual void setTrailingIconName(IconName);
	virtual IconName getTrailingIconName() const;

	virtual void setTrailingIconProgress(float progress, float animation = 0.0f);
	virtual float getTrailingIconProgress() const;

	virtual void setTapCallback(Function<void()> &&);
	virtual const Function<void()> &getTapCallback() const { return _callbackTap; }

	virtual void setLongPressCallback(Function<void()> &&);
	virtual const Function<void()> &getLongPressCallback() const { return _callbackLongPress; }

	virtual void setDoubleTapCallback(Function<void()> &&);
	virtual const Function<void()> &getDoubleTapCallback() const { return _callbackDoubleTap; }

	virtual void setMenuSourceButton(Rc<MenuSourceButton> &&);
	virtual MenuSourceButton *getMenuSourceButton() const;

	virtual void setBlendColor(ColorRole, float value);
	virtual void setBlendColor(const Color4F &, float value);

	virtual ColorRole getBlendColorRule() const;
	virtual const Color4F &getBlendColor() const;
	virtual float getBlendColorValue() const;

	virtual TypescaleLabel *getLabelTextNode() const { return _labelText; }
	virtual TypescaleLabel *getLabelValueNode() const { return _labelValue; }
	virtual IconSprite *getLeadingIconNode() const { return _leadingIcon; }
	virtual IconSprite *getTrailingIconNode() const { return _trailingIcon; }

	virtual InputListener *getInputListener() const { return _inputListener; }

	virtual DataListener<MenuSourceButton> *getMenuButtonListener() const {
		return _menuButtonListener;
	}

	// in input coordinates
	Vec2 getTouchLocation() const { return _touchLocation; }

protected:
	virtual bool hasContent() const;
	virtual void updateSizeFromContent();
	virtual void updateActivityState();

	virtual void handleTap();
	virtual void handleLongPress();
	virtual void handleDoubleTap();

	virtual float getWidthForContent() const;

	virtual void updateMenuButtonSource();
	virtual void layoutContent();

	InputListener *_inputListener = nullptr;
	TypescaleLabel *_labelText = nullptr;
	TypescaleLabel *_labelValue = nullptr;
	IconSprite *_leadingIcon = nullptr;
	IconSprite *_trailingIcon = nullptr;

	Rc<MenuSource> _floatingMenuSource;
	DataListener<MenuSourceButton> *_menuButtonListener = nullptr;

	Function<void()> _callbackTap;
	Function<void()> _callbackLongPress;
	Function<void()> _callbackDoubleTap;

	Vec2 _touchLocation;

	float _activityAnimationDuration = 0.25f;

	NodeMask _nodeMask = All;

	bool _followContentSize = true;
	bool _mouseOver = false;
	bool _enabled = true;
	bool _focused = false;
	bool _pressed = false;
	bool _selected = false;
	bool _longPressInit = false;
};

SP_DEFINE_ENUM_AS_MASK(Button::NodeMask)

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_BUTTON_MATERIALBUTTON_H_ */
