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

#include "InstallerSceneContent.h"
#include "InstallerLayout.h"
#include "XLWindowDecorations.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstallerSceneContent::~InstallerSceneContent() { }

bool InstallerSceneContent::init() {
	if (!basic2d::SceneContent2d::init()) {
		return false;
	}

	setWindowDecorationsContructor([](NotNull<SceneContent>) -> Rc<WindowDecorations> {
		return Rc<WindowDecorations>::create();
	});

	_rootStyle = addSystem(
			Rc<ui::StyleSystem>::create(FileInfo{"resources/style.css", FileCategory::Bundled}));

	_globalBackground =
			addChild(Rc<basic2d::Layer>::create(Color::White), ZOrder::min() + ZOrder(2));
	_globalBackground->setName("global-background");
	_globalBackground->addSystem(Rc<ui::StyleResolver>::create());

	pushLayout(Rc<InstallerLayout>::create());

	/*_top = addChild(Rc<basic2d::Layer>::create(Color::Grey_500));

	auto l = _top->addSystem(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::MoveGrip);

	_button = addChild(Rc<ui::Button>::create());
	_button->setName("button");
	_button->setString("Создать проект");

	_button2 = addChild(Rc<ui::Button>::create());
	_button2->setName("button2");
	_button2->setIcon(basic2d::IconName::Content_create_solid);
	_button2->addStyleClass("outlined");
	_button2->setString("Создать проект");*/

	return true;
}

void InstallerSceneContent::handleContentSizeDirty() {
	basic2d::SceneContent2d::handleContentSizeDirty();

	if (_testNode) {
		_testNode->setAnchorPoint(Anchor::MiddleTop);
		_testNode->setContentSize(Size2(100, 100));
		_testNode->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 10.0f));
	}

	if (_top) {
		_top->setAnchorPoint(Anchor::MiddleTop);
		_top->setContentSize(Size2(_contentSize.width - 20.0f, 20.0f));
		_top->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 10.0f));
	}
}

} // namespace stappler::xenolith::installer
