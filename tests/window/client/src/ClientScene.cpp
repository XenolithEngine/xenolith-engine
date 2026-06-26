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

#include "XLCommon.h"

#include "XLContext.h"
#include "XL2dSceneContent.h"
#include "XLRemoteWindow.h"
#include "XLSimpleButton.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XLInputListener.h" // DEBUG: verify forwarded input reaches the scene
#include "XLSimpleCloseGuardWidget.h"
#include "XLEntryPoint.h"

#include <stdlib.h> // getenv for the screenshot output path

#include "ClientScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::client {

bool ClientScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints) {
	// Используем примитивы из пакета simpleui
	// Это также подключает и примитивы из basic2d, поверх которого реализован simpleui
	using namespace simpleui;

	// Инициализируем суперкласс
	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	// Создаём объект, хранящий содержимое сцены
	// Содержимое сцены связывается с конкретным проходом очереди рендеринга
	// Для нескольких проходов в рамках одной сцены потребуется несколько таких объектов
	auto content = Rc<SceneContent2d>::create();

	// Задаём параметры освещения по умолчанию, чтобы эффекты теней работали
	// Модуль simpleui этого не делает, поскольку не использует тени,
	// но модуль material2d настраивает себе свет сам
	content->setDefaultLights();

	// Простой закрашенный квадрат (узел Layer на базе SolidImage). Шрифты пока недоступны,
	// поэтому рисуем только сплошной прямоугольник — минимальная проверка клиентского рендеринга.
	_square = content->addChild(Rc<basic2d::Layer>::create(Color4F(0.2f, 0.6f, 1.0f, 1.0f)));
	_square->setContentSize(Size2(256.0f, 256.0f));
	_square->setAnchorPoint(Anchor::Middle);

	// Текстовая метка — рендеринг шрифта на удалённом клиенте. Материал атласа шрифта компилируется на
	// сервере (RemoteWindow::compileMaterials -> WindowCode::CompileMaterials), глифы растеризуются там же.
	_label = content->addChild(Rc<basic2d::Label>::create("REMOTE"));
	_label->setFontSize(96);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setColor(Color4F::BLACK);
	// The headless client has no deferred-vertex work queue (the default path errors with "BusDelegate not
	// attached"); generate the label quads in the foreground instead.
	_label->setDeferred(false);

	// Применяем содержимое сцены
	setContent(content);

	// Remote runtime font materials are now forwarded to the server for compilation; enable text.
	setFpsVisible(true);

	// DEBUG: verify server-forwarded input (WindowCode::InputEvents) actually reaches the scene graph.
	// The server's XL_REMOTE_INPUT_SIM emitter sweeps a cursor and clicks; these recognizers log when
	// the replayed events surface as gestures in the client scene.
	auto listener = content->addSystem(Rc<InputListener>::create());
	listener->addMoveRecognizer([](const GestureData &data) -> bool {
		auto loc = data.input->currentLocation;
		log::source().info("ClientScene", "[input] move -> x=", loc.x, " y=", loc.y);
		return true;
	});
	listener->addTapRecognizer([](const GestureTap &tap) -> bool {
		auto loc = tap.location();
		log::source().info("ClientScene", "[input] TAP -> x=", loc.x, " y=", loc.y, " count=",
				tap.count);
		return true;
	});

	// DEBUG (reverse direction): a static region that requests a custom OS cursor. The scene graph
	// computes this as a WindowLayer; the headless client forwards it to the server (WindowCode::
	// UpdateLayers), which applies it to the real OS window. A fixed 200x120 region at (50, 600) so the
	// forwarded layer's rect + cursor can be verified in the server log.
	auto cursorRegion = content->addChild(Rc<basic2d::Layer>::create(Color4F(1.0f, 1.0f, 0.0f, 0.35f)));
	cursorRegion->setContentSize(Size2(200.0f, 120.0f));
	cursorRegion->setAnchorPoint(Anchor::BottomLeft);
	cursorRegion->setPosition(Vec2(50.0f, 600.0f));
	auto cursorListener = cursorRegion->addSystem(Rc<InputListener>::create());
	cursorListener->setCursor(WindowCursor::Pointer);

	return true;
}

// Геометрия сцены изменилась, обнвляем содержимое соотвественно
void ClientScene::handleContentSizeDirty() {
	Scene2d::handleContentSizeDirty();

	// Центрируем квадрат при каждом изменении размера сцены
	if (_square) {
		_square->setPosition(_contentSize / 2.0f);
	}
	if (_label) {
		_label->setPosition(_contentSize / 2.0f);
	}
}

void ClientScene::handleEnter(Scene *scene) {
	Scene2d::handleEnter(scene);

	// Проверяем, что система действий (runAction) работает в контексте удалённого клиента.
	// Квадрат бесконечно пульсирует масштабом. Пока действие активно,
	// Director::hasActiveInteractions() == true, поэтому после каждого кадра клиент посылает
	// серверу setReadyForNextFrame, а сервер обеспечивает непрерывную выдачу кадров.
	// CallFunc в конце каждого цикла логирует счётчик — так в логе клиента видно, что действие
	// реально продвигается во времени (то есть кадры действительно поступают).
	if (_square && !_animStarted) {
		_animStarted = true;
		_square->runAction(Rc<RepeatForever>::create(Rc<Sequence>::create(
				Rc<ScaleTo>::create(0.6f, 1.5f), Rc<ScaleTo>::create(0.6f, 1.0f), [this] {
			++_animTick;
			log::source().info("ClientScene", "animation tick ", _animTick);
		})));
	}

	// Через 2 секунды после входа (сервер уже рисует наши кадры) запрашиваем у него скриншот окна —
	// то есть нашего удалённого вывода — и сохраняем полученные пиксели в PNG. Запрос уходит через
	// RemoteWindow::captureScreenshot (WindowCode::RequestScreenshot), а пиксели возвращаются обратно
	// крупным блоком по Domain::Data. Делаем это ровно один раз.
	if (!_screenshotRequested) {
		_screenshotRequested = true;
		runAction(Rc<Sequence>::create(Rc<DelayTime>::create(2.0f), [this] {
			requestRemoteScreenshot();
		}));
	}
}

void ClientScene::requestRemoteScreenshot() {
	if (!_director) {
		return;
	}
	auto server = _director->getRenderServer();
	if (!server) {
		return;
	}

	// Путь к итоговому PNG: переменная окружения XL_REMOTE_SCREENSHOT_FILE или значение по умолчанию.
	String path("remote-screenshot.png");
	if (const char *file = ::getenv("XL_REMOTE_SCREENSHOT_FILE")) {
		path = file;
	}

	log::source().info("ClientScene", "requesting remote screenshot -> ", path);

	// captureScreenshot на RemoteWindow уходит на сервер; колбэк вызовется, когда блок с пикселями
	// будет полностью получен обратно (см. ClientAppThread::loadExtensions -> deliverScreenshot).
	server->captureScreenshot([path](const core::ImageInfoData &info, BytesView data) {
		if (data.empty()) {
			log::source().error("ClientScene", "remote screenshot failed (no data)");
			return;
		}
		if (core::saveImage(FileInfo(path), info, data)) {
			log::source().info("ClientScene", "remote screenshot saved: ", path, " (", data.size(),
					" bytes, ", info.extent.width, "x", info.extent.height, ")");
		} else {
			log::source().error("ClientScene", "failed to save remote screenshot: ", path);
		}
	});
}

StringView ClientScene::selectServerQueue(NotNull<AppThread> app,
		NotNull<core::RenderServerChannel> window) {
	auto rw = static_cast<RemoteWindow *>(window.get());
	for (auto &it : rw->getQueues()) {
		if (it.name == "RemoteClientQueue") {
			return it.name;
		}
	}
	return StringView();
}

// Регистрируем ClientScene как основной класс сцены для приложения
// Под капотом:
// - Создаётся функция, сопоставляющая окно приложения и сцену
// - Эта функция регистрируется через механизм ShaderModule в качестве функции выбора сцены
DEFINE_PRIMARY_SCENE_CLASS(ClientScene)

} // namespace stappler::xenolith::client
