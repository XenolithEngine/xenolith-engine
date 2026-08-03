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

#include "InstallerTitleBar.h"

#include "XLUiButton.h"
#include "XLInputListener.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

TitleBar::~TitleBar() { }

bool TitleBar::init() {
	if (!Node::init()) {
		return false;
	}

	setName("title-bar");

	// The OS buttons are created in a fixed order and reordered by CSS (`order`), so the macOS
	// media block can move the traffic lights to the left without touching this code.
	_osMinimize = addChild(Rc<ui::Button>::create(ui::ButtonType::OsMinimize));
	_osMinimize->setName("os-minimize");
	_osMinimize->addStyleClass("os-button");

	_osMaximize = addChild(Rc<ui::Button>::create(ui::ButtonType::OsMaximize));
	_osMaximize->setName("os-maximize");
	_osMaximize->addStyleClass("os-button");

	_osClose = addChild(Rc<ui::Button>::create(ui::ButtonType::OsClose));
	_osClose->setName("os-close");
	_osClose->addStyleClass("os-button");

	_osMenu = addChild(Rc<ui::Button>::create(ui::ButtonType::OsMenu));
	_osMenu->setName("os-menu");
	_osMenu->addStyleClass("os-button");

	{
		_osIcon = addChild(Rc<basic2d::Sprite>::create("app-icon.png"));
		_osIcon->setName("title-icon");
		_osIcon->addStyleClass("title-button");

		auto listener = _osIcon->addSystem(Rc<InputListener>::create());
		listener->setLayerFlags(
				WindowLayerFlags::WindowMenuRight | WindowLayerFlags::WindowMenuLeft);
	}

	_profile = addChild(Rc<Node>::create());
	_profile->setName("title-profile");
	_profile->addStyleClass("title-button");

	{
		// The strip is created opaque: `background-color` from CSS replaces the colour, but a
		// transparent placeholder would zero the subtree's opacity and hide the title.
		_titleLine = addChild(Rc<basic2d::Layer>::create(Color::Black));
		_titleLine->addStyleClass("title-line");

		auto titleLabel = _titleLine->addChild(Rc<basic2d::Label>::create());
		titleLabel->setType("label");
		titleLabel->setString("Xenolith SDK Installer");

		auto listener = _titleLine->addSystem(Rc<InputListener>::create());
		listener->setLayerFlags(WindowLayerFlags::MoveGrip | WindowLayerFlags::WindowMenuRight);
	}

	return true;
}

} // namespace stappler::xenolith::installer
