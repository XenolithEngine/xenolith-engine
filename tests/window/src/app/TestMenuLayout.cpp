/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "app/TestMenuLayout.h"
#include "XL2dScrollController.h"
#include "XL2dSceneContent.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Высота элементов меню: у группы она больше, чтобы уровни различались и на глаз
static constexpr float MenuGroupHeight = 40.0f;
static constexpr float MenuTestHeight = 32.0f;

void TestMenuLayout::buildGroupItems(basic2d::ScrollController *controller,
		NotNull<basic2d::SceneLayout2d> owner, const TestGroup &group) {
	using namespace ui;

	// Сначала вложенные группы: они открывают следующий уровень того же меню. Ссылки на записи
	// реестра безопасны — таблица статическая.
	for (auto &it : group.groups) {
		auto name = toString(it.title, "  (", getTestCount(it), ") >");
		controller->addItem([owner, name, sub = &it](const ScrollController::Item &) -> Rc<Node> {
			return makeButton(name, [owner, sub] {
				owner->getSceneContent()->pushLayout(Rc<TestMenuLayout>::create(*sub));
			});
		}, MenuGroupHeight);
	}

	// Затем тесты самой группы. Запись без переменной окружения — это не тест, а сама передняя
	// страница приложения, и в меню ей делать нечего.
	for (auto &it : group.tests) {
		if (it.env.empty()) {
			continue;
		}
		controller->addItem([owner, test = &it](const ScrollController::Item &) -> Rc<Node> {
			return makeButton(test->title,
					[owner, test] { owner->getSceneContent()->pushLayout(makeTestLayout(*test)); });
		}, MenuTestHeight);
	}
}

bool TestMenuLayout::init(const TestGroup &group) {
	using namespace ui;

	if (!TestLayout::init()) {
		return false;
	}

	_group = &group;

	// Заголовок — та же полоса, в которой тест пишет своё название
	setCaption(group.title, group.description);

	// Дамп сцены через инспектор показывает имя укладки, поэтому уровень меню в нём виден
	setLayoutName(toString("menu:", group.name));

	// Меню с вертикальной прокруткой — такое же, как на передней странице
	_menu = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_menu->setAnchorPoint(Anchor::MiddleTop);
	_menu->setIndicatorColor(Color::Grey_500);

	auto controller = _menu->setController(Rc<ScrollController>::create());

	controller->addPlaceholder(32.0f);

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		return makeButton("Go back", [this] { this->pop(); });
	}, MenuTestHeight);

	buildGroupItems(controller, this, group);

	controller->addPlaceholder(32.0f);

	return true;
}

void TestMenuLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	auto cs = getContentSize();

	// Меню занимает всё, что осталось от полосы заголовка; ширина ограничена — как и на
	// передней странице
	_menu->setPosition(Vec2(cs.width / 2.0f, getWorkTop()));
	_menu->setContentSize(Size2(sprt::min(cs.width, 480.0f), getWorkSize().height));
}

} // namespace stappler::xenolith::app
