/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_

#include "XLUiPanel.h"
#include "XLUiInteractiveComponent.h"
#include "XL2dIconSprite.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

enum class ButtonType {
	General,
	OsMinimize,
	OsMaximize,
	OsClose,
	OsMenu,
	OsFullscreen
};

enum class ButtonIconTheme {
	Default,
	Apple,
};

// An interactive Panel: the fill / outline / border-radius chrome and its CSS appliers come from
// Panel (type "button"), the button itself adds the label, the icon and the input handling. CSS:
//   button { background-color:#1e88e5; outline-color:#0d47a1; outline-width:2px;
//            border-radius:20px; display:flex; align-items:center; ... }
//   button > label { color:#ffffff; font-size:16px; }
class SP_PUBLIC Button : public Panel {
public:
	virtual ~Button();

	virtual bool init(ButtonType, Function<void()> && = nullptr);
	virtual bool init(Function<void()> && = nullptr);

	virtual void handleEnter(Scene *scene) override;
	virtual void handleComponentsDirty(const ComponentMask &) override;

	virtual void setString(StringView);
	virtual StringView getString() const;

	virtual void setIcon(IconName);
	virtual IconName getIcon() const;

	// Direct label styling, for buttons built outside a stylesheet (auxiliary windows that do not
	// share the main StyleSystem). These forward to the internal label; CSS `color`/`font-weight`
	// remain the primary path for normally-styled buttons, but a popup/dialog needs the colour set
	// without a stylesheet in scope.
	virtual void setLabelColor(const Color4F &);
	virtual void setLabelFontWeight(font::FontWeight);
	virtual basic2d::Label *getLabel() const;

protected:
	virtual void updateState();

	virtual bool handleLeftTap();
	virtual bool handleRightTap();

	ButtonType _type = ButtonType::General;
	ButtonIconTheme _theme = ButtonIconTheme::Default;
	WindowState _windowState = WindowState::None;

	Function<void()> _leftCallback;
	Function<void()> _rightCallback;

	InputListener *_listener = nullptr;

	basic2d::Label *_label = nullptr;
	basic2d::IconSprite *_icon = nullptr;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_
