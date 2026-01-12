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

#include "MonitorModeSelectionLayout.h"
#include "SPLog.h"
#include "XL2dScrollController.h"
#include "XLSimpleButton.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XlCoreMonitorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool MonitorModeSelectionLayout::init(NotNull<ScreenInfo> info, uint32_t index) {
	using namespace simpleui;

	if (!SceneLayout2d::init()) {
		return false;
	}

	_monitorIndex = index;
	_screenInfo = info;

	// Создаём меню с вертикальной прокруткой
	_menu = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_menu->setAnchorPoint(Anchor::MiddleTop);
	_menu->setIndicatorColor(Color::Grey_500);

	// Для работы прокрутке необходим контроллер, который наполняется элементами вдоль основной оси
	auto controller = _menu->setController(Rc<ScrollController>::create());

	// Задаём начальный отступ вдоль основной оси
	controller->addPlaceholder(32.0f);

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		return Rc<ButtonWithLabel>::create("Go back", [this] { this->pop(); });
	}, 32.0f);

	if (_monitorIndex < _screenInfo->monitors.size()) {
		auto &mon = _screenInfo->monitors[_monitorIndex];

		auto monName = toString("Fullscreen to: ", mon.name, " (", mon.edid.vendor, " ",
				mon.edid.model, " ", mon.edid.serial, ")");
		controller->addItem([monName](const ScrollController::Item &) -> Rc<Node> {
			return Rc<ButtonWithLabel>::create(monName);
		}, 48.0f);

		uint32_t modeIndex = 0;
		for (auto &mode : mon.modes) {
			auto name =
					toString(mode.width, " x ", mode.height, " @ ", mode.rate, " x", mode.scale);
			if (modeIndex == mon.currentMode) {
				name = toString(name, " (current)");
			}
			if (modeIndex == mon.preferredMode) {
				name = toString(name, " (preferred)");
			}
			controller->addItem([this, name, mode](const ScrollController::Item &) -> Rc<Node> {
				return Rc<ButtonWithLabel>::create(name, [this, mode] {
					_director->getWindow()->setFullscreen(
							FullscreenInfo{_screenInfo->monitors[_monitorIndex], mode,
								FullscreenFlags::Exclusive},
							[this](Status s) {
						if (s == Status::Ok || s == Status::Declined) {
							this->pop();
						} else {
							log::error("MonitorModeSelectionLayout", "Fail to set fullscreen: ", s);
						}
					}, this);
				});
			}, 32.0f);
			++modeIndex;
		}
	}

	controller->addPlaceholder(32.0f);

	return true;
}

void MonitorModeSelectionLayout::handleEnter(Scene *scene) { SceneLayout2d::handleEnter(scene); }

void MonitorModeSelectionLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	auto cs = getContentSize();

	// Размещаем меню
	// Якорь находится посередине сверху
	_menu->setPosition(Vec2(cs.width / 2.0f, cs.height));

	// Задаём размер меню
	// Разворачиваем на полную высоту, но ограничиваем максимальную ширину
	_menu->setContentSize(Size2(std::min(_contentSize.width, 480.0f), _contentSize.height));
}


} // namespace stappler::xenolith::app
