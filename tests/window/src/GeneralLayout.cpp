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

#include "XLCommon.h"

#include "GeneralLayout.h"
#include "SPLog.h"
#include "XL2dSceneLayout.h"
#include "XLContextInfo.h"
#include "XLSimpleButton.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XL2dSceneContent.h"
#include "MonitorModeSelectionLayout.h"
#include "XlCoreMonitorInfo.h"
#include "XLEventListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool GeneralLayout::init() {
	using namespace simpleui;

	if (!SceneLayout2d::init()) {
		return false;
	}

	// Создаём меню с вертикальной прокруткой
	_menu = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_menu->setAnchorPoint(Anchor::MiddleTop);
	_menu->setIndicatorColor(Color::Grey_500);

	// Для работы прокрутке необходим контроллер, который наполняется элементами вдоль основной оси
	_menu->setController(Rc<ScrollController>::create());

	auto eventListener = addSystem(Rc<EventListener>::create());
	eventListener->listenForEvent(Context::onSystemNotification, [this](const Event &ev) {
		auto n = SystemNotification(ev.getDataValue().getInteger());
		if (n == SystemNotification::DisplayChanged) {
			updateScreenInfo();
		}
	});

	auto inputListener = addSystem(Rc<InputListener>::create());
	inputListener->setWindowStateCallback([this](WindowState state, WindowState changes) {
		// enter or exit fullscreen mode
		if (hasFlag(changes, WindowState::Fullscreen)) {
			rebuildMenu();
		}
		return true;
	});

	return true;
}

void GeneralLayout::handleEnter(Scene *scene) {
	log::debug("GeneralLayout", "handleEnter");
	SceneLayout2d::handleEnter(scene);
	updateScreenInfo();
	rebuildMenu();
}

void GeneralLayout::handleContentSizeDirty() {
	log::debug("GeneralLayout", "handleContentSizeDirty");
	SceneLayout2d::handleContentSizeDirty();

	auto cs = getContentSize();

	// Размещаем меню
	// Якорь находится посередине сверху
	_menu->setPosition(Vec2(cs.width / 2.0f, cs.height));

	// Задаём размер меню
	// Разворачиваем на полную высоту, но ограничиваем максимальную ширину
	_menu->setContentSize(Size2(std::min(_contentSize.width, 480.0f), _contentSize.height));
}

void GeneralLayout::handleForeground(basic2d::SceneContent2d *l, basic2d::SceneLayout2d *overlay) {
	log::debug("GeneralLayout", "handleForeground");
	basic2d::SceneLayout2d::handleForeground(l, overlay);

	updateScreenInfo();
	rebuildMenu();
}

void GeneralLayout::rebuildMenu() {
	using namespace simpleui;

	auto controller = _menu->getController();

	controller->clear();

	// Задаём начальный отступ вдоль основной оси
	controller->addPlaceholder(32.0f);

	controller->addItem([](const ScrollController::Item &) -> Rc<Node> {
		auto sprite = Rc<Sprite>::create("xenolith-2-480.png");
		sprite->setTextureAutofit(Autofit::Contain);
		return sprite;
	}, 180.0f);

	// Добавляем кнопку с фиксированным размером вдоль основной оси (высотой)
	// Побочная ось (в этом случае - ширина) наследуется от базового элемента
	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		// Cоздаём простую конпку
		// Её позиционирование и размер контроллер прокрутки настроит сам
		return Rc<ButtonWithLabel>::create("Hello world", [this] {
			log::debug("ExampleScene", "Hello world");
			++_helloWorldCounter;
			rebuildMenu();
		});
	}, 32.0f);

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		return Rc<ButtonWithLabel>::create("Probe clibboard", [this] {
			_director->getApplication()->probeClipboard([](Status st, SpanView<StringView> types) {
				slog().debug("GeneralLayout", "Clipboard status: ", st);
				for (auto &it : types) { slog().debug("GeneralLayout", "Clipboard type: ", it); }
			}, this);
		});
	}, 32.0f, ZOrder(0), "ClipboardButton");

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		return Rc<ButtonWithLabel>::create("Read from clibboard", [this] {
			_director->getApplication()->readFromClipboard(
					[this](Status, BytesView bytes, StringView type) {
				if ((type == "image/png" || type == "image/jpeg") && !bytes.empty()) {
					auto tex = _director->getResourceCache()->addExternalEncodedImage(
							toString("Clipboard", Time::now().toMicros()),
							core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM,
									core::ImageUsage::Sampled),
							bytes);

					_menu->getController()->addItem(
							[tex = move(tex)](const ScrollController::Item &) mutable -> Rc<Node> {
						auto sprite = Rc<Sprite>::create(move(tex));
						sprite->setTextureAutofit(Autofit::Contain);
						return sprite;
					}, 200.0f);
					_menu->getController()->onScrollPosition(true);
				}
				if (type.starts_with("text/plain") && !bytes.empty()) {
					log::info("GeneralLayout", "Clipboard: ", bytes.readString());
				}
				if (type.starts_with("text/html") && !bytes.empty()) {
					log::info("GeneralLayout", "Clipboard: ", bytes.readString());
				}
			}, [](SpanView<StringView> typeList) -> StringView {
				StringView ret;
				for (auto &it : typeList) {
					if (it == "image/png") {
						ret = it;
						break;
					}
					if (it == "image/jpeg") {
						ret = it;
						break;
					}
				}
				if (ret.empty()) {
					for (auto &it : typeList) {
						if (it.starts_with("text/html")) {
							ret = it;
							break;
						}
						if (it.starts_with("text/plain")) {
							ret = it;
							break;
						}
					}
				}
				return ret;
			}, this);
		});
	}, 32.0f, ZOrder(0), "ClipboardButton");

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		return Rc<ButtonWithLabel>::create("Write to clipboard", [this] {
			_director->getApplication()->writeToClipboard(BytesView("Xenolith clipboard text"));
		});
	}, 32.0f);

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		return Rc<ButtonWithLabel>::create("Capture screenshot", [this] {
			_director->getWindow()->captureScreenshot(
					[this](const core::ImageInfoData &image, BytesView data) {
				struct BitmapContainer : public Ref {
					Bitmap bmp;
					BitmapContainer(Bitmap &&b) : bmp(sp::move(b)) { }
				};

				auto container = Rc<BitmapContainer>::create(core::getBitmap(image, data));

				auto imageTypes = Vector<StringView>{
					"image/png",
					//	"image/webp",
				};
				_director->getApplication()->writeToClipboard(
						[container](StringView type) -> sprt::window::Bytes {
					if (type == "image/png") {
						return container->bmp.write<sprt::window::Bytes>(bitmap::FileFormat::Png);
					} else if (type == "image/webp") {
						return container->bmp.write<sprt::window::Bytes>(
								bitmap::FileFormat::WebpLossless);
					}
					return sprt::window::Bytes();
				}, imageTypes);
			});
		});
	}, 32.0f);

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		// Cоздаём простую конпку
		// Её позиционирование и размер контроллер прокрутки настроит сам
		return Rc<ButtonWithLabel>::create(_exitGuardRetained ? "Disable Exit Guard"
															  : "Enable Exit Guard",
				[this] { toggleExitGuard(); });
	}, 32.0f, ZOrder(0), "ExitGuardButton");

	if (_director) {
		auto w = _director->getWindow();
		if (hasFlag(w->getCapabilities(), WindowCapabilities::Fullscreen)) {
			if (hasFlag(w->getWindowState(), WindowState::Fullscreen)) {
				_menu->getController()->addItem([this](const ScrollController::Item &) -> Rc<Node> {
					return Rc<ButtonWithLabel>::create("Exit fullscreen", [this] {
						_director->getWindow()->setFullscreen(FullscreenInfo(FullscreenInfo::None),
								[](Status) { });
					});
				}, 64.0f, ZOrder(0));
			} else {
				_menu->getController()->addItem([this](const ScrollController::Item &) -> Rc<Node> {
					return Rc<ButtonWithLabel>::create("Fullscreen on current", [this] {
						_director->getWindow()->setFullscreen(
								FullscreenInfo(FullscreenInfo::Current), [](Status s) {
							if (s != Status::Ok) {
								log::error("GeneralLayout", "Fail to set fullscreen: ", s);
							}
						});
					});
				}, 64.0f, ZOrder(0));
			}

			if (_screenInfo) {
				uint32_t index = 0;
				for (auto &it : _screenInfo->monitors) {
					auto name = toString("Fullscreen to: ", it.name, " (", it.edid.vendor, " ",
							it.edid.model, " ", it.edid.serial, ")");
					_menu->getController()->addItem(
							[this, name, mon = it, index](
									const ScrollController::Item &) -> Rc<Node> {
						return Rc<ButtonWithLabel>::create(name, [this, mon, index] {
							auto l = Rc<MonitorModeSelectionLayout>::create(_screenInfo, index);
							getSceneContent()->pushLayout(l);
						});
					},
							64.0f, ZOrder(0));
					++index;
				}
			}
		}

		if (hasFlag(w->getCapabilities(), WindowCapabilities::DecorationState)) {
			_menu->getController()->addItem([this](const ScrollController::Item &) -> Rc<Node> {
				return Rc<ButtonWithLabel>::create("Toggle status bar", [this] {
					if (_director) {
						auto w = _director->getWindow();
						auto state = w->getWindowState();
						if (hasFlag(state, WindowState::DecorationStatusBarVisible)) {
							w->disableState(WindowState::DecorationStatusBarVisible);
						} else {
							w->enableState(WindowState::DecorationStatusBarVisible);
						}
					}
				});
			}, 64.0f, ZOrder(0));
		}

		if (hasFlag(w->getCapabilities(), WindowCapabilities::DecorationState)) {
			_menu->getController()->addItem([this](const ScrollController::Item &) -> Rc<Node> {
				return Rc<ButtonWithLabel>::create("Toggle navigation bar", [this] {
					if (_director) {
						auto w = _director->getWindow();
						auto state = w->getWindowState();
						if (hasFlag(state, WindowState::DecorationNavigationVisible)) {
							w->disableState(WindowState::DecorationNavigationVisible);
						} else {
							w->enableState(WindowState::DecorationNavigationVisible);
						}
					}
				});
			}, 64.0f, ZOrder(0));
		}
	}

	controller->addItem([this](const ScrollController::Item &) -> Rc<Node> {
		// Cоздаём простую конпку
		// Её позиционирование и размер контроллер прокрутки настроит сам
		return Rc<ButtonWithLabel>::create("Close", [this] {
			auto w = _director->getWindow();
			w->close(true);
		});
	}, 32.0f);

	controller->addPlaceholder(32.0f);

	controller->commitChanges();
}

void GeneralLayout::toggleExitGuard() {
	if (!_exitGuardRetained) {
		_sceneContent->setCloseGuardEnabled(true);
		_exitGuardRetained = true;

		auto item = _menu->getController()->getItem("ExitGuardButton");
		if (item && item->node) {
			static_cast<simpleui::ButtonWithLabel *>(item->node)->setString("Disable Exit guard");
		}
	} else {
		_sceneContent->setCloseGuardEnabled(false);
		_exitGuardRetained = false;

		auto item = _menu->getController()->getItem("ExitGuardButton");
		if (item && item->node) {
			static_cast<simpleui::ButtonWithLabel *>(item->node)->setString("Enable Exit guard");
		}
	}
}

void GeneralLayout::updateScreenInfo() {
	log::debug("GeneralLayout", "updateScreenInfo");
	using namespace simpleui;
	if (_director) {
		_director->getWindow()->acquireScreenInfo([this](NotNull<ScreenInfo> screenInfo) {
			_screenInfo = screenInfo;
			if (_running) {
				rebuildMenu();
			}
		}, this);
	}
}

} // namespace stappler::xenolith::app
