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
#include "XLUiButton.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XLUiCloseGuardWidget.h"
#include "XLSceneInspector.h"
#include "XLEntryPoint.h"

#include "app/ExampleScene.h"
#include "window/SecondaryWindow.h"
#include "XL2dLayerRounded.h" // marker layer for the second shared window
#include "app/TestRegistry.h"
#include "app/GeneralLayout.h" // MonitorModeSelectionLayout.cc, included below, builds on it
#include "app/LiveReloadAppThread.h" // live-reload session addr+key, when active
#include "XLRemoteProtocol.h"
#include "XLServerAppThread.h" // ServerAppThread: listener state for the `remote` command

#include "window/MonitorModeSelectionLayout.cc"

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
	// Используем примитивы из пакета ui
	// Это также подключает и примитивы из basic2d, поверх которого реализован ui
	using namespace ui;

	// Инициализируем суперкласс
	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	// Создаём объект, хранящий содержимое сцены
	// Содержимое сцены связывается с конкретным проходом очереди рендеринга
	// Для нескольких проходов в рамках одной сцены потребуется несколько таких объектов
	auto content = Rc<SceneContent2d>::create();

	// Задаём параметры освещения по умолчанию, чтобы эффекты теней работали
	// Модуль ui этого не делает, поскольку не использует тени
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

	// Тест, которому счётчик мешает, гасит его сам при входе (TestLayout::handleEnter).
	//
	// XL_HIDE_FPS=1 гасит счётчик для всех укладок сразу. Он меняется каждый кадр, поэтому сцена
	// с ним никогда не «устаканивается»: снимок, снятый по правилу «два одинаковых кадра подряд»,
	// не получить вовсе — а это единственный способ сравнить два бэкенда на одной укладке
	// (tests/parity/layouts.py).
	bool fpsVisible = true;
	if (auto value = ::getenv("XL_HIDE_FPS")) {
		fpsVisible = StringView(value) == "0";
	}
	setFpsVisible(fpsVisible);

	return true;
}

// Геометрия сцены изменилась, обнвляем содержимое соотвественно
void ExampleScene::handleContentSizeDirty() { Scene2d::handleContentSizeDirty(); }

void ExampleScene::handleWindowGeometryChanged(const sprt::window::WindowGeometry &g) {
	Scene2d::handleWindowGeometryChanged(g);
	++_geometryChangeCount;
	_lastGeometry = g;
}

void ExampleScene::handleEnter(Scene *scene) { Scene2d::handleEnter(scene); }

// Сцена была собрана и запущена режиссёром
void ExampleScene::handlePresented(Director *dir) {
	Scene2d::handlePresented(dir);

	// Очередь для удалённого клиента.
	//
	// Имя намеренно НЕ "RemoteClientQueue": клиент раньше искал очередь именно по этой строке, то
	// есть согласование держалось на договорённости, которой нет ни в одном протоколе. Теперь имя —
	// только ручка, а выбор идёт по тому, чем очередь ЯВЛЯЕТСЯ (gAPI и тип); имя, которого клиент
	// никогда не видел, — самая прямая проверка этого.
	core::Queue::Builder builder("SharedWindowQueue");

	QueueInfo queueInfo{
		Extent2(_constraints.extent.width, _constraints.extent.height),
		Color4F::WHITE,
	};

	describeQueue(queueInfo);
	buildQueueResources(queueInfo, builder);

	// Строим тот же граф, что построила бы локальная сцена на этом бэкенде, — а не жёстко
	// vk::ShadowPass. Раньше здесь была ветка «только Vulkan», потому что клиент всё равно не мог
	// узнать, на чём работает сервер; теперь очередь сама себя описывает (api/typeTag уходят в
	// SharedObjectsAnnounce), и клиент выбирает по этому описанию. Значит окно можно шарить с
	// любого бэкенда, включая --gapi soft.
	if (!buildQueue(dir->getApplication(), queueInfo, builder)) {
		log::source().error("ExampleScene", "fail to build the shared queue");
		return;
	}

// Window sharing is Linux-only for now (the listener needs a socket).
#if SPRT_LINUX
	{
		// Sharing is OFF unless asked for, because it opens a listening socket: a plain run of this
		// app must not start accepting connections.
		//
		// Three ways to ask, in priority order:
		//   1. live reload (--watch) -- the session's negotiated address + key, the same pair it
		//      hands each client it launches;
		//   2. XL_REMOTE_SHARE=<address> [+ XL_REMOTE_TOKEN=<token>, key = Sha512(token)] -- what
		//      remote-check.py drives, so an end-to-end run needs no code change;
		//   3. nothing -> no listener.
		StringView shareAddr;
		Bytes shareKey;

		if (auto lr = dynamic_cast<LiveReloadAppThread *>(dir->getApplication())) {
			if (!lr->getServerAddress().empty()) {
				shareAddr = lr->getServerAddress();
				shareKey = lr->getBearerKey().bytes<Interface>();
			}
		}

		if (shareAddr.empty()) {
			if (auto env = ::getenv("XL_REMOTE_SHARE")) {
				shareAddr = StringView(env);
				if (auto tok = ::getenv("XL_REMOTE_TOKEN")) {
					auto h = crypto::Sha512::perform(StringView(tok));
					shareKey = BytesView(h.data(), h.size()).bytes<Interface>();
				} else {
#if DEBUG
					shareKey = remote::getDevBearerKey().bytes<Interface>();
#else
					log::source().error("ExampleScene",
							"XL_REMOTE_SHARE without XL_REMOTE_TOKEN needs a debug build");
					shareAddr = StringView();
#endif
				}
			}
		}

		if (!shareAddr.empty()) {
			log::source().info("ExampleScene", "sharing this window at ", shareAddr);
			dir->shareQueue(sp::move(builder), shareAddr,
					BytesView(shareKey.data(), shareKey.size()));
		}
	}
#endif
}

// Внешнее управление приложением: реестр тестов, доступный через сокет инспектора.
//
//   layouts            -> { "layouts": [ { "name", "path", "group", "title", "description",
//                            "hideFps" } ], "tree": { вложенные группы реестра } }
//   layout {name, settle} -> сменить укладку; ответ приходит, когда новая укладка уже
//                            отрисовывается settle секунд (по умолчанию 1 с) — то есть
//                            сразу после ответа можно снимать screenshot.
//
// Реестр — дерево (директории в src/), поэтому список отдаётся дважды: плоским, каким он был
// всегда, и деревом, чтобы была видна группировка. Имя укладки при этом остаётся коротким и
// уникальным: `layout` принимает и его, и полный путь ("css/nth").
//
// Команды самих укладок регистрируются ими же (TestLayout::registerCommands) и видны в общем
// списке `commands`, пока укладка на экране.
void ExampleScene::registerCommands() {
	auto content = getContent();

	inspector::addCommand(content, "layouts", "List the test layouts this app can show",
			[](Value &&, Function<void(Value &&)> &&done) {
		Value layouts(Value::Type::ARRAY);

		// Обход дерева: плоский список наполняется попутно, чтобы путь до укладки считался один раз
		auto makeEntry = [](const TestInfo &it, StringView group) {
			Value entry;
			entry.setString(it.name, "name");
			entry.setString(group.empty() ? toString(it.name) : toString(group, "/", it.name),
					"path");
			if (!group.empty()) {
				entry.setString(group, "group");
			}
			entry.setString(it.title, "title");
			entry.setString(it.description, "description");
			if (!it.env.empty()) {
				entry.setString(it.env, "env");
			}
			if (it.hideFps) {
				entry.setBool(true, "hideFps");
			}
			return entry;
		};

		auto walk = [&](auto &&self, const TestGroup &group, StringView path) -> Value {
			Value node;
			if (!group.name.empty()) {
				node.setString(group.name, "name");
				node.setString(path, "path");
			}
			node.setString(group.title, "title");
			node.setString(group.description, "description");

			Value tests(Value::Type::ARRAY);
			for (auto &it : group.tests) {
				auto entry = makeEntry(it, path);
				tests.addValue(entry);
				layouts.addValue(sp::move(entry));
			}
			if (!tests.empty()) {
				node.setValue(sp::move(tests), "tests");
			}

			Value groups(Value::Type::ARRAY);
			for (auto &it : group.groups) {
				groups.addValue(self(self, it,
						path.empty() ? toString(it.name) : toString(path, "/", it.name)));
			}
			if (!groups.empty()) {
				node.setValue(sp::move(groups), "groups");
			}
			return node;
		};

		auto tree = walk(walk, getTestRegistry(), StringView());

		Value result;
		result.setValue(sp::move(layouts), "layouts");
		result.setValue(sp::move(tree), "tree");
		done(sp::move(result));
	});

	// Remote render session status, for tests/window/remote-check.py.
	//
	// A screenshot alone cannot tell a client-rendered frame from a server-rendered one -- both draw
	// the same scene through the same window. So the state that decides it is reported directly:
	// whether the listener is up, the SPKI the client must pin (only knowable after the listener
	// opens, which is why the driver polls for it), and whether a client holds the connection.
	inspector::addCommand(content, "remote", "Remote render session status: { listening, spki, "
										 "clientConnected }",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value result;
		auto app = dynamic_cast<ServerAppThread *>(getDirector()->getApplication());
		if (!app) {
			result.setBool(false, "ok");
			result.setString("not a server app thread", "error");
			done(sp::move(result));
			return;
		}
		result.setBool(true, "ok");
		result.setBool(app->isListening(), "listening");
		result.setBool(app->hasRemoteClient(), "clientConnected");
		// How many windows the session is offering. The driver waits on this rather than on a
		// sleep: a window opens asynchronously and shares itself once presented, so "did the second
		// window make it into the announce" has no other observable answer.
		auto objs = app->getSharedObjects();
		result.setInteger(objs ? int64_t(objs->getWindows().size()) : 0, "sharedWindows");
		auto fp = app->getListenerFingerprint();
		result.setString(fp.empty() ? String() : base16::encode<Interface>(fp), "spki");
		done(sp::move(result));
	});

	/* Call off every bulk transfer this server is still streaming.
	
	The only bulk producer today is the screenshot a remote client asks for, and it is precisely the
	thing that used to be uncancellable: Release meant "I stop referencing the blob" and stopped the
	stream only as a side effect, without telling the far side's waiter anything. Reports how many
	were actually in flight, so a check can tell "cancelled one" from "there was nothing to cancel".*/
	inspector::addCommand(content, "remote-cancel-transfers",
			"Cancel every in-flight Domain::Data transfer; answers { cancelled }",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value result;
		auto app = dynamic_cast<ServerAppThread *>(getDirector()->getApplication());
		if (!app) {
			result.setBool(false, "ok");
			result.setString("not a server app thread", "error");
			done(sp::move(result));
			return;
		}
		result.setBool(true, "ok");
		result.setInteger(int64_t(app->cancelOutgoingTransfers()), "cancelled");
		done(sp::move(result));
	});

	/* Open a SECOND Root window and offer it to the same remote session.
	
	This is what makes "one connection, several windows" observable end to end. It is a command
	rather than something the app does on startup because a plain run must keep opening exactly one
	window -- every other check in the suite compares against that.
	
	The reply says the window was REQUESTED, not that it arrived: it appears asynchronously and
	shares itself when its scene is first presented. Wait for `remote`.sharedWindows to reach 2. */
	inspector::addCommand(content, "remote-share-second",
			"Open a second Root window and share it with the running remote session",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value result;
		auto server = getDirector() ? getDirector()->getRenderServer() : nullptr;
		if (!server) {
			result.setBool(false, "ok");
			result.setString("no render server on this window", "error");
			done(sp::move(result));
			return;
		}
		if (_secondSharedWindow) {
			result.setBool(true, "ok");
			result.setBool(true, "alreadyOpen");
			done(sp::move(result));
			return;
		}

		_secondSharedWindow = SecondaryWindow::open(static_cast<AppWindow *>(server),
				"remote-second", Extent2(320, 240),
				[](StringView) -> Rc<basic2d::SceneLayout2d> {
			// Deliberately unlike the primary scene: the check that the client really drew THIS
			// window is a screenshot comparison, and two identical scenes would pass it while
			// proving nothing.
			auto layout = Rc<basic2d::SceneLayout2d>::create();
			auto marker = layout->addChild(Rc<basic2d::LayerRounded>::create(Color::Red_500, 0.0f));
			marker->setAnchorPoint(Anchor::Middle);
			marker->setContentSize(Size2(160, 90));
			marker->setPositionZ(0.0f);
			layout->setLayoutCallback([marker](Node *node) {
				marker->setPosition(node->getContentSize() / 2.0f);
			});
			return layout;
		}, [this](NotNull<WindowSceneInfo>) { _secondSharedWindow = nullptr; }, nullptr,
				sprt::nullopt, /* shareRemote */ true);

		result.setBool(_secondSharedWindow != nullptr, "ok");
		if (!_secondSharedWindow) {
			result.setString("could not request the window", "error");
		}
		done(sp::move(result));
	});

	inspector::addCommand(content, "layout",
			"Show a test layout: { name, settle } - name is \"nth\" or \"css/nth\"; "
			"answers once it has settled",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		// Через const-ссылку, как и в `dialog` ниже: неконстантный getString() на отсутствующем
		// ключе — это assert, а не пустая строка. Запрос приходит из сокета, то есть ключа может
		// не быть вовсе, и тогда команда обязана ответить ошибкой, а не уронить приложение.
		const Value &req = args;
		auto name = req.getString("name");
		auto info = findTest(name);
		if (!info) {
			Value result;
			result.setBool(false, "ok");
			result.setString(toString("unknown layout: ", name), "error");
			done(sp::move(result));
			return;
		}

		auto settle = req.hasValue("settle") ? float(req.getDouble("settle")) : s_layoutSettle;
		switchLayout(*info, settle, sp::move(done));
	});

	// Системные диалоги (runtime/window/dialog.h). Каждый принадлежит окну и отменяется вместе
	// с ним; ответ приходит на app-треде, поэтому его можно отдать прямо в `done`.
	//
	//   dialog {type, path, title, modal, multiple}
	//     type: open-file | open-directory | save-file | color | font | reveal | trash | restore
	//
	// `trash`, `restore` и `reveal` не открывают UI, поэтому проверяются без дисплея. Результат
	// содержит `supported` — ответ AppWindow::isDialogSupported для этого типа: `restore` есть не
	// на всех платформах, и это надо спросить до того, как действие предложено пользователю.
	inspector::addCommand(content, "dialog",
			"Open a system dialog: { type, path, title, modal, multiple }",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		static const Map<String, sprt::window::DialogType> types{
			{"open-file", sprt::window::DialogType::OpenFile},
			{"open-directory", sprt::window::DialogType::OpenDirectory},
			{"save-file", sprt::window::DialogType::SaveFile},
			{"color", sprt::window::DialogType::Color},
			{"font", sprt::window::DialogType::Font},
			{"reveal", sprt::window::DialogType::RevealInFileManager},
			{"trash", sprt::window::DialogType::MoveToTrash},
			{"restore", sprt::window::DialogType::RestoreFromTrash},
		};

		const Value &req = args;
		auto typeName = req.getString("type");
		auto typeIt = types.find(typeName);
		if (typeIt == types.end()) {
			Value result;
			result.setBool(false, "ok");
			result.setString(toString("unknown dialog type: ", typeName), "error");
			done(sp::move(result));
			return;
		}

		auto *window = static_cast<AppWindow *>(getDirector()->getRenderServer());
		if (!window) {
			Value result;
			result.setBool(false, "ok");
			result.setString("no window", "error");
			done(sp::move(result));
			return;
		}

		auto request = Rc<sprt::window::DialogRequest>::create();
		request->type = typeIt->second;
		request->title = req.getString("title");
		request->path = req.getString("path");
		request->filename = req.getString("filename");
		if (req.getBool("modal")) {
			request->flags |= sprt::window::DialogFlags::Modal;
		}
		if (req.getBool("multiple")) {
			request->flags |= sprt::window::DialogFlags::Multiple;
		}
		for (auto &it : req.getValue("paths").asArray()) {
			request->paths.emplace_back(it.getString());
		}

		const bool supported = window->isDialogSupported(typeIt->second);

		request->callback = [supported, done = sp::move(done)](
									const sprt::window::DialogResult &res) mutable {
			Value result;
			result.setBool(sprt::status::isSuccessful(res.status), "ok");
			result.setBool(supported, "supported");
			result.setString(sprt::status::getStatusName(res.status), "status");
			Value paths(Value::Type::ARRAY);
			for (auto &it : res.paths) { paths.addString(it); }
			result.setValue(sp::move(paths), "paths");
			result.setString(
					toString(res.color.r, " ", res.color.g, " ", res.color.b, " ", res.color.a),
					"color");
			if (!res.font.description.empty()) {
				result.setString(res.font.description, "font");
			}
			done(sp::move(result));
		};

		auto st = window->openDialog(request);
		if (!sprt::status::isSuccessful(st)) {
			// openDialog already answered through the callback; nothing more to do here.
			log::source().debug("ExampleScene",
					"openDialog refused: ", sprt::status::getStatusName(st));
		}
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
	}),
			settle, Function<void()>([done = sp::move(done), name = info.name]() mutable {
		Value result;
		result.setBool(true, "ok");
		result.setString(name, "layout");
		done(sp::move(result));
	})));
}

// Какую очередь просит сцена. Живёт отдельно от buildQueueResources, потому что этот ответ нужен
// и удалённой сцене: она ничего не строит, но по нему выбирает очередь сервера.
void ExampleScene::describeQueue(QueueInfo &info) {
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
			info.damage =
					core::QueueDamageFlags::PresentHint | core::QueueDamageFlags::PartialRedraw;
			log::source().info("ExampleScene", "Empty frame skipping disabled");
		}
	}
}

void ExampleScene::buildQueueResources(QueueInfo &info, core::Queue::Builder &builder) {
	builder.addImage("xenolith-2-480.png",
			core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
					core::ImageHints::Opaque),
			FileInfo("resources/xenolith-2-480.png", FileCategory::Bundled));
}

// Фабрика сцены по умолчанию: она отвечает только за окна, которые приложение не создавало само —
// прежде всего за корневое. Вторичные окна (SecondaryWindow) называют свою сцену прямо в данных
// окна, через WindowSceneInfo, поэтому диспетчер по WindowInfo::id, который был здесь раньше,
// больше не нужен.
DEFINE_PRIMARY_SCENE_CLASS(ExampleScene)

} // namespace stappler::xenolith::app
