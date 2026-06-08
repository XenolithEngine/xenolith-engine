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

#include "XLContext.h"
#include "XL2dSceneContent.h"
#include "XLSimpleButton.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XLSimpleCloseGuardWidget.h"
#include "XLEntryPoint.h"

#include "ExampleScene.h"
#include "GeneralLayout.h"
#include "XLRemoteProtocol.h"

#include "SPBitmap.h"

#include "MonitorModeSelectionLayout.cc"

#if MODULE_XENOLITH_BACKEND_VK
#include "backend/vk/XL2dVkShadowPass.h"
#endif

#include <sys/random.h>
#include <stdlib.h> // getenv / atof for the screenshot workflow
#include <sprt/runtime/utils/base16.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool ExampleScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
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

	// Запускаем основной слой интерфейса
	content->pushLayout(Rc<GeneralLayout>::create());

	// Устанавливаем стандартный виджет для подтверждения выхода
	content->setCloseGuardWidgetContructor([](NotNull<SceneContent>) -> Rc<CloseGuardWidget> {
		return Rc<CloseGuardWidgetDefault>::create();
	});

	// Применяем содержимое сцены
	setContent(content);

	setFpsVisible(true);

	uint8_t buf[256] = {0};
	if (getrandom(buf, 256, 0) == 256) {
		sprt::base16::encode(buf, 256,
				[](const char *buf, size_t size) { slog().info("Random", StringView(buf, size)); });
	}


	return true;
}

// Геометрия сцены изменилась, обнвляем содержимое соотвественно
void ExampleScene::handleContentSizeDirty() { Scene2d::handleContentSizeDirty(); }

void ExampleScene::handleEnter(Scene *scene) { Scene2d::handleEnter(scene); }

// Сцена была собрана и запущена режиссёром
void ExampleScene::handlePresented(Director *dir) {
	Scene2d::handlePresented(dir);

	// Remote client needs a separate queue
	core::Queue::Builder builder("RemoteClientQueue");

	QueueInfo queueInfo{
		Extent2(_constraints.extent.width, _constraints.extent.height),
		Color4F::WHITE,
	};

	buildQueueResources(queueInfo, builder);

#if MODULE_XENOLITH_BACKEND_VK
	basic2d::vk::ShadowPass::RenderQueueInfo info{
		dir->getApplication()->getGlLoop(),
		queueInfo.extent,
		basic2d::vk::ShadowPass::Flags::None,
		queueInfo.backgroundColor,
	};

	basic2d::vk::ShadowPass::makeRenderQueue(builder, info);
#endif

	_remoteQueue = Rc<core::Queue>::create(sp::move(builder));
	if (_remoteQueue) {
		dir->getRenderServer()->compileRenderQueue(_remoteQueue,
				[guard = Rc<ExampleScene>(this)](bool success) {
			if (success && guard->isRunning()) {
				auto dir = guard->getDirector();
				// Dev/demo: launch the remote render-session listener on a fixed address and allow this
				// window to be taken over by a connecting client (X11-style split, transport bring-up).
				// The bearer key a client must present is the shared dev key; no server dictionary is set, so a
				// client's suggested dictionary will be used.
				dir->setBearerKey(remote::getDevBearerKey());
				dir->setListenAddress("127.0.0.1:4480");
				dir->shareWindow();
				dir->shareQueue(guard->_remoteQueue);
			}
		});
	}

	// Безголовый сценарий снятия скриншота, управляемый переменными окружения,
	// чтобы графический вывод можно было проверить из скрипта (сборка -> запуск
	// -> чтение PNG):
	//   XL_SCREENSHOT_FILE  - путь к итоговому PNG (включает сценарий)
	//   XL_SCREENSHOT_DELAY - задержка в секундах перед снимком (по умолчанию 1.0)
	if (const char *file = ::getenv("XL_SCREENSHOT_FILE")) {
		float delay = 1.0f;
		if (const char *d = ::getenv("XL_SCREENSHOT_DELAY")) {
			delay = float(::atof(d));
		}

		// Ждём заданное время через DelayTime, затем снимаем кадр и выходим
		runAction(Rc<Sequence>::create(Rc<DelayTime>::create(delay),
				[this, path = String(file)] {
			_director->getWindow()->captureScreenshot(
					[this, path](const core::ImageInfoData &image, BytesView data) {
				if (core::saveImage(FileInfo(path), image, data)) {
					log::info("ExampleScene", "Screenshot saved: ", path);
				} else {
					log::error("ExampleScene", "Failed to save screenshot: ", path);
				}
				_director->getWindow()->close(true);
			});
		}));
	}
}

void ExampleScene::buildQueueResources(QueueInfo &, core::Queue::Builder &builder) {
	builder.addImage("xenolith-2-480.png",
			core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
					core::ImageHints::Opaque),
			FileInfo("resources/xenolith-2-480.png", FileCategory::Bundled));
}

// Регистрируем ExampleScene как основной класс сцены для приложения
// Под капотом:
// - Создаётся функция, сопоставляющая окно приложения и сцену
// - Эта функция регистрируется через механизм ShaderModule в качестве функции выбора сцены
DEFINE_PRIMARY_SCENE_CLASS(ExampleScene)

} // namespace stappler::xenolith::app
