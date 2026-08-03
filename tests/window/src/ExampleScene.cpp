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
#include "XLSceneInspector.h"
#include "XLEntryPoint.h"

#include "ExampleScene.h"
#include "TestRegistry.h"
#include "GeneralLayout.h" // MonitorModeSelectionLayout.cc, included below, builds on it
#include "LiveReloadAppThread.h" // live-reload session addr+key, when active
#include "XLRemoteProtocol.h"

#include "MonitorModeSelectionLayout.cc"

#if MODULE_XENOLITH_BACKEND_VK
#include "backend/vk/XL2dVkShadowPass.h"
#endif

#include <sys/random.h>
#include <stdlib.h> // getenv for the render-queue variants below
#include <sprt/runtime/utils/base16.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Сколько секунд новая укладка должна отрисовываться, прежде чем команда `layout` ответит.
// Этого достаточно, чтобы отработали и раскладка, и анимации входа, и таймерные фазы теста,
// которые запускаются при входе.
static constexpr float s_layoutSettle = 1.0f;

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

	// Запускаем основной слой интерфейса — либо тестовый слой, если задана выбирающая его
	// переменная окружения. Список тестов, их описания и переменные живут в TestRegistry.
	content->pushLayout(makeSelectedTestLayout());

	// Устанавливаем стандартный виджет для подтверждения выхода
	content->setCloseGuardWidgetContructor([](NotNull<SceneContent>) -> Rc<CloseGuardWidget> {
		return Rc<CloseGuardWidgetDefault>::create();
	});

	// Применяем содержимое сцены
	setContent(content);

	// Внешнее управление сценой через сокет инспектора: список укладок и переключение между ними.
	// Инспектор живёт на SceneContent, поэтому регистрируем команды уже после setContent.
	registerCommands();

	// Тест, которому счётчик мешает, гасит его сам при входе (TestLayout::handleEnter)
	setFpsVisible(true);

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
	// The shared queue is vk-specific: building it on any other loop (e.g. --gapi soft, which has
	// no depth formats at all) would describe passes that device can not compile.
	if (static_cast<core::Loop *>(dir->getApplication()->getGlLoop())->getInstance()->getApi()
			== core::InstanceApi::Vulkan) {
		basic2d::vk::ShadowPass::RenderQueueInfo info{
			dir->getApplication()->getGlLoop(),
			queueInfo.extent,
			basic2d::vk::ShadowPass::Flags::None,
			queueInfo.backgroundColor,
		};

		basic2d::vk::ShadowPass::makeRenderQueue(builder, info);
	}
#endif

// Window sharing is Linux-only for now; the shared queue is vk-based and
// can not be compiled on a WebGPU device
#if SPRT_LINUX
	/*if (static_cast<core::Loop *>(dir->getApplication()->getGlLoop())->getInstance()->getApi()
			== core::InstanceApi::Vulkan) {
		// When live reload is active, listen on the session's negotiated address + bearer key (the same
		// pair the server hands each launched client). Otherwise fall back to the shared dev key on a
		// fixed port, for a manually launched client.
		StringView shareAddr("127.0.0.1:4480");
		BytesView shareKey = remote::getDevBearerKey();
		if (auto lr = dynamic_cast<LiveReloadAppThread *>(dir->getApplication())) {
			if (!lr->getServerAddress().empty()) {
				shareAddr = lr->getServerAddress();
				shareKey = lr->getBearerKey();
			}
		}
		dir->shareQueue(sp::move(builder), shareAddr, shareKey);
	}*/
#endif

}

// Внешнее управление приложением: реестр тестов, доступный через сокет инспектора.
//
//   layouts            -> { "layouts": [ { "name", "title", "description", "hideFps" } ] }
//   layout {name, settle} -> сменить укладку; ответ приходит, когда новая укладка уже
//                            отрисовывается settle секунд (по умолчанию 1 с) — то есть
//                            сразу после ответа можно снимать screenshot.
//
// Команды самих укладок регистрируются ими же (TestLayout::registerCommands) и видны в общем
// списке `commands`, пока укладка на экране.
void ExampleScene::registerCommands() {
	auto content = getContent();

	inspector::addCommand(content, "layouts", "List the test layouts this app can show",
			[](Value &&, Function<void(Value &&)> &&done) {
		Value layouts(Value::Type::ARRAY);
		for (auto &it : getTestRegistry()) {
			Value entry;
			entry.setString(it.name, "name");
			entry.setString(it.title, "title");
			entry.setString(it.description, "description");
			if (!it.env.empty()) {
				entry.setString(it.env, "env");
			}
			if (it.hideFps) {
				entry.setBool(true, "hideFps");
			}
			layouts.addValue(sp::move(entry));
		}

		Value result;
		result.setValue(sp::move(layouts), "layouts");
		done(sp::move(result));
	});

	inspector::addCommand(content, "layout",
			"Show a test layout: { name, settle } - answers once it has settled",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		auto name = args.getString("name");
		auto info = findTest(name);
		if (!info) {
			Value result;
			result.setBool(false, "ok");
			result.setString(toString("unknown layout: ", name), "error");
			done(sp::move(result));
			return;
		}

		auto settle = args.hasValue("settle") ? float(args.getDouble("settle")) : s_layoutSettle;
		switchLayout(*info, settle, sp::move(done));
	});
}

void ExampleScene::switchLayout(const TestInfo &info, float settle,
		Function<void(Value &&)> &&done) {
	auto content = static_cast<basic2d::SceneContent2d *>(getContent());

	// Смена укладки — первым звеном последовательности, чтобы задержка отсчитывалась уже
	// по новой сцене и та успела разложиться и доиграть свои анимации входа. Кадры на это время
	// обеспечивает RenderContinuously самой укладки (TestLayout::init).
	runAction(Rc<Sequence>::create(Function<void()>([content, info = &info] {
		content->replaceLayout(makeTestLayout(*info).get());
	}), settle, Function<void()>([done = sp::move(done), name = info.name]() mutable {
		Value result;
		result.setBool(true, "ok");
		result.setString(name, "layout");
		done(sp::move(result));
	})));
}

void ExampleScene::buildQueueResources(QueueInfo &info, core::Queue::Builder &builder) {
	// XL_FLAT_QUEUE=1 — облегчённая очередь отрисовки: без теней, частиц, буфера глубины
	// и шейдера постобработки
	if (auto value = ::getenv("XL_FLAT_QUEUE")) {
		if (StringView(value) != "0") {
			info.type = QueueType::Flat;
			log::source().info("ExampleScene", "Using flat (lightweight) 2d render queue");
		}
	}

	// XL_NO_PARTIAL_REDRAW=1 — оставить damage только подсказкой композитору, перерисовывая
	// кадр целиком. Нужно, чтобы измерить выигрыш от частичной перерисовки на одной сцене.
	if (auto value = ::getenv("XL_NO_PARTIAL_REDRAW")) {
		if (StringView(value) != "0") {
			info.damage = core::QueueDamageFlags::PresentHint;
			log::source().info("ExampleScene", "Partial redraw disabled");
		}
	}

	// XL_NO_SKIP_FRAMES=1 — оставить частичную перерисовку, но всегда перерисовывать кадр,
	// даже если в нём ничего не изменилось. Для сравнения с включённым пропуском.
	if (auto value = ::getenv("XL_NO_SKIP_FRAMES")) {
		if (StringView(value) != "0") {
			info.damage = core::QueueDamageFlags::PresentHint | core::QueueDamageFlags::PartialRedraw;
			log::source().info("ExampleScene", "Empty frame skipping disabled");
		}
	}

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
