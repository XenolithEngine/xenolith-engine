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

#include "XLSimpleUiConfig.h" // IWYU pragma: keep
#include "XLWindowDecorations.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

class WindowDecorationsButton;

struct SP_PUBLIC WindowDecorationsState {
	static const ComponentId Id;

	WindowState state = WindowState::None;
	WindowCapabilities capabilities = WindowCapabilities::None;
};

struct SP_PUBLIC WindowDecorationsTheme {
	static const ComponentId Id;

	enum class ColorTheme {
		Default,
		Dark,
	};

	enum class IconTheme {
		Default,
		Macos,
	};

	ColorTheme color = ColorTheme::Default;
	IconTheme icon = IconTheme::Default;
};

enum class WindowDecorationsButtonType {
	Close,
	Maximize,
	Minimize,
	Fullscreen,
	ContextMenu,
};

class SP_PUBLIC WindowDecorationsButton : public Node {
public:
	virtual ~WindowDecorationsButton() = default;

	virtual bool init(WindowDecorationsButtonType);

	virtual void handleContentSizeDirty() override;
	virtual void handleComponentsDirty() override;

protected:
	virtual void handleTap();
	virtual void updateState();

	WindowDecorationsButtonType _type = WindowDecorationsButtonType::Close;
	VectorSprite *_icon = nullptr;
	VectorSprite *_background = nullptr;
	WindowState _state = WindowState::None;
	WindowDecorationsTheme::IconTheme _iconTheme = WindowDecorationsTheme::IconTheme::Default;
	bool _selected = false;
};

// Simple window header implementation
class SP_PUBLIC WindowDecorationsDefault : public WindowDecorations {
public:
	static constexpr float HeaderHeight = 24.0f;

	virtual ~WindowDecorationsDefault() = default;

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;
	virtual void handleComponentsDirty() override;

	virtual Padding getPadding() const override;

protected:
	virtual void updateWindowState(WindowState) override;
	virtual void updateWindowTheme(const ThemeInfo &) override;

	Layer *_header = nullptr;

	WindowDecorationsButton *_buttonClose = nullptr;
	WindowDecorationsButton *_buttonMaximize = nullptr;
	WindowDecorationsButton *_buttonMinimize = nullptr;
	WindowDecorationsButton *_buttonFullscreen = nullptr;
	WindowDecorationsButton *_buttonMenu = nullptr;
};

} // namespace stappler::xenolith::simpleui
