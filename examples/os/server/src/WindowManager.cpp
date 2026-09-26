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


#include "WindowManager.h"
#include "WmSurfaceScene.h"

#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLServerAppThread.h"
#include "XLRemoteSession.h"
#include "XLRemotePeer.h"
#include "XLSceneInspector.h"
#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "SPFilesystem.h"
#include "SPValid.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/utils/base16.h>

#include <stdlib.h>
#include <unistd.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

// Shell mode z order
static constexpr int32_t WindowManager_zShell = 0;
static constexpr int32_t WindowManager_zApplication = 10;
static constexpr int32_t WindowManager_zShade = 100;
static constexpr int32_t WindowManager_zPending = -1; // drawing, not seen yet

// A window its client has not drawn into is shown anyway after this long.
static constexpr uint64_t WindowManager_showTimeout = 5'000'000;

// The shell and the shade come back after this long, unless they exit this quickly this many
// times in a row.
static constexpr uint64_t WindowManager_restartDelay = 1'000'000;
static constexpr uint64_t WindowManager_quickExit = 5'000'000;
static constexpr uint32_t WindowManager_maxQuickExits = 3;

static Value WindowManager_encodeRect(int64_t x, int64_t y, int64_t w, int64_t h) {
	Value ret;
	ret.addInteger(x);
	ret.addInteger(y);
	ret.addInteger(w);
	ret.addInteger(h);
	return ret;
}

static StringView WindowManager_getRoleName(WindowManager::Role role) {
	switch (role) {
	case WindowManager::Role::Application: return "application"; break;
	case WindowManager::Role::Shell: return "shell"; break;
	case WindowManager::Role::Shade: return "shade"; break;
	}
	return StringView();
}

// A colour from XL_WM_BG, "RRGGBB"; the default is a dark grey.
static Color4F WindowManager_readBackground() {
	if (auto env = ::getenv(protocol::EnvBackground)) {
		auto value = StringView(env).readInteger(16);
		if (value.valid()) {
			auto rgb = uint32_t(value.get());
			return Color4F(float((rgb >> 16) & 0xFF) / 255.0f, float((rgb >> 8) & 0xFF) / 255.0f,
					float(rgb & 0xFF) / 255.0f, 1.0f);
		}
	}
	return Color4F(0.12f, 0.12f, 0.14f, 1.0f);
}

WindowManager::~WindowManager() { stop(); }

bool WindowManager::init(NotNull<ServerAppThread> app, NotNull<AppWindow> host) {
	_app = app.get();
	_host = host.get();
	_pipe = Rc<compositor::SwapchainDisplayPipe>::create(app, host);
	if (!_pipe) {
		return false;
	}

	// XL_WM_FPS: the vblank rate the planes draw at; a headless host has no pace of its own.
	uint64_t fps = 60;
	if (auto env = ::getenv(protocol::EnvFps)) {
		fps = uint64_t(StringView(env).readInteger(10).get(60));
	}
	_pipe->setMinFrameInterval(fps > 0 ? 1'000'000 / fps : 0);
	_pipe->setBackground(WindowManager_readBackground());
	_pipe->setPlanePublishedCallback([this](compositor::DisplayPlane *plane, uint64_t serial) {
		handlePlanePublished(plane, serial);
	});
	return true;
}

Status WindowManager::start() {
	if (_started) {
		return Status::ErrorAlreadyPerformed;
	}
	_started = true;

	auto st = _pipe->attach([](Status st) {
		if (sprt::status::isSuccessful(st)) {
			log::source().info("WindowManager", "the compositor owns the host window");
		} else {
			log::source().error("WindowManager",
					"fail to attach the compositor: ", sprt::status::getStatusName(st));
		}
	});
	if (!sprt::status::isSuccessful(st)) {
		auto loop = _app->getGlLoop();
		log::source().error("WindowManager", "no compositor for this graphics API (",
				loop && loop->getInstance() ? toInt(loop->getInstance()->getApi()) : 0,
				"); run with --gapi vulkan or --gapi soft");
		return st;
	}

	Vector<StringView> commands;
	if (auto env = ::getenv(protocol::EnvApps)) {
		StringView(env).split<StringView::Chars<';'>>([&](StringView cmd) {
			cmd.trimChars<StringView::WhiteSpace>();
			if (!cmd.empty()) {
				commands.emplace_back(cmd);
			}
		});
	}
	_mode = commands.empty() ? Mode::Shell : Mode::Stack;

	if (auto env = ::getenv(protocol::EnvAddress)) {
		_address = StringView(env).str<Interface>();
	} else if (_mode == Mode::Stack) {
		_address = toString("shm:/dev/shm/wmserver-", ::getpid());
	} else {
		_address = toString("shm:/dev/shm/xl-wm-", ::getpid());
	}

	if (_mode == Mode::Shell) {
		_catalog = Rc<AppCatalog>::create();
		if (!_catalog) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	// Every client presents a key made for it; the session's own key is one nobody holds.
	uint8_t raw[32];
	valid::makeRandomBytes(raw, sizeof(raw));
	auto sessionKey = crypto::Sha512::perform(BytesView(raw, sizeof(raw)));

	_app->setRequireLabelledKeys(true);
	_app->setMaxClientWindows(1);
	_app->setMaxRemoteClients(_mode == Mode::Stack ? uint32_t(commands.size() + 2) : 16);
	_app->setClientWindowHandler(
			[this](NotNull<RemoteSession> session, NotNull<sprt::window::WindowInfo> info,
					Rc<WindowSceneInfo> &out) -> Status {
		return handleClientWindow(session, info, out);
	});

	if (_mode == Mode::Shell) {
		_app->setAppMessageHandler(
				[this](NotNull<RemoteSession> session, Value &&value, Rc<AppReply> &&reply) {
			handleAppMessage(session, sp::move(value), sp::move(reply));
		});
		_app->setSessionObserver(
				[this](NotNull<RemoteSession> session, ServerAppThread::SessionEvent event) {
			auto app = getApp(session->getLabel());
			if (!app) {
				return;
			}
			if (event == ServerAppThread::SessionEvent::Started) {
				app->session = session.get();
				if (app->role == Role::Shade) {
					sendApps();
					sendStatus();
				}
			} else if (app->session == session.get()) {
				app->session = nullptr;
			}
		});
	}

	if (!_app->startSession(_address, BytesView(sessionKey.data(), sessionKey.size()))) {
		log::source().error("WindowManager", "fail to open the session on '", _address, "'");
		return Status::ErrorInvalidArguemnt;
	}

	if (_mode == Mode::Stack) {
		log::source().info("WindowManager", "session on '", _address, "', ", commands.size(),
				" app(s)");
		for (auto &it : commands) {
			launch(Role::Application, toString("app", _apps.size() + 1), it,
					LaunchStyle::Positional, _pipe->getExtent());
		}
	} else {
		log::source().info("WindowManager", "session on '", _address, "', shell mode");
		launchSystem(Role::Shell);
		launchSystem(Role::Shade);
		scheduleStatus();
	}
	return Status::Ok;
}

void WindowManager::stop() {
	if (!_started) {
		return;
	}
	_started = false;

	if (_app) {
		_app->setClientWindowHandler(nullptr);
		if (_mode == Mode::Shell) {
			_app->setAppMessageHandler(nullptr);
			_app->setSessionObserver(nullptr);
		}
	}

	if (_statusTimer) {
		_statusTimer->cancel();
		_statusTimer = nullptr;
	}

	_pipe->setPlanePublishedCallback(nullptr);

	_active = nullptr;
	_pending = nullptr;

	auto apps = sp::move(_apps);
	_apps.clear();
	for (auto &it : apps) {
		it->server = nullptr;
		it->session = nullptr;
		if (it->showTimer) {
			it->showTimer->cancel();
			it->showTimer = nullptr;
		}
		if (it->process) {
			it->closing = true;
			it->process->cancel();
			it->process = nullptr;
		}
		if (it->plane) {
			_pipe->destroyPlane(it->plane);
			it->plane = nullptr;
		}
		it->window = nullptr;
	}

	_pipe->detach();
}

Status WindowManager::launchApp(StringView id) {
	auto entry = _catalog ? _catalog->get(id) : nullptr;
	if (!entry) {
		return Status::ErrorNotFound;
	}
	if (!entry->available) {
		return Status::ErrorNotSupported;
	}

	// One window per program: launching a running one brings it up.
	for (auto &it : _apps) {
		if (it->role == Role::Application && it->running && it->catalogId == id) {
			return switchTo(it->label);
		}
	}

	uint32_t n = 1;
	auto it = _launchCounts.find(id);
	if (it != _launchCounts.end()) {
		n = ++it->second;
	} else {
		_launchCounts.emplace(id.str<Interface>(), n);
	}

	auto app = launch(Role::Application, toString("app:", id, ":", n),
			toString("'", entry->path, "'"), entry->launch, getWindowExtent(Role::Application));
	if (!app) {
		return Status::ErrorInvalidArguemnt;
	}
	app->catalogId = id.str<Interface>();
	_pending = app;
	sendApps();
	return Status::Ok;
}

Status WindowManager::switchTo(StringView label) {
	auto app = getApp(label);
	if (!app || app->role != Role::Application || !app->running) {
		return Status::ErrorNotFound;
	}
	if (!app->shown) {
		// it comes up with its first frame
		_pending = app;
		return Status::Ok;
	}
	_pending = nullptr;
	_active = app;
	applyLayout();
	sendApps();
	return Status::Ok;
}

Status WindowManager::closeApp(StringView label) {
	auto app = getApp(label);
	if (!app || app->role != Role::Application || !app->process) {
		return Status::ErrorNotFound;
	}
	app->closing = true;
	app->process->cancel();
	return Status::Ok;
}

void WindowManager::goHome() {
	_pending = nullptr;
	_active = getSystem(Role::Shell);
	applyLayout();
	sendApps();
}

void WindowManager::setShadeOpen(bool open) {
	_shadeOpen = open;
	applyLayout();
}

Status WindowManager::handleClientWindow(NotNull<RemoteSession> session,
		NotNull<sprt::window::WindowInfo> info, Rc<WindowSceneInfo> &out) {
	auto app = getApp(session->getLabel());
	if (!app || app->window) {
		// not a program of ours, or one that has its window already
		return Status::Declined;
	}

	auto extent = _mode == Mode::Stack ? _pipe->getExtent() : getWindowExtent(app->role);

	// The window is its plane: virtual, the plane's size, no decorations of its own, and none of
	// the program's own limits - they would make it larger than the plane.
	info->type = sprt::window::WindowType::Root;
	info->title = app->label;
	info->flags |= sprt::window::WindowCreationFlags::Virtual;
	info->flags &= ~sprt::window::WindowCreationFlags::UserSpaceDecorations;
	info->rect = IRect(0, 0, extent.width, extent.height);
	info->minExtent = extent;
	info->maxExtent = extent;
	// Host pixels at the host's density: the plane shows the window 1:1.
	info->density = _host->getConstraints().density;

	auto ref = Rc<App>(app);
	auto transparent = app->role == Role::Shade;
	out = Rc<WindowSceneInfo>::create(
			[ref, transparent](NotNull<AppThread> thread,
					NotNull<core::RenderServerChannel> channel,
					const core::FrameConstraints &constraints) -> Rc<Scene> {
		auto window = dynamic_cast<AppWindow *>(channel.get());
		if (auto server = ref->server; server && window) {
			server->handleWindowCreated(ref, window);
		}
		return Rc<WmSurfaceScene>::create(thread, channel, constraints, ref->label, transparent,
				[ref] { ++ref->serverFrames; });
	},
			[ref](NotNull<WindowSceneInfo>) {
		if (auto server = ref->server) {
			server->handleWindowClosed(ref);
		}
	});

	app->window = out;
	return out ? Status::Ok : Status::Declined;
}

void WindowManager::handleWindowCreated(App *app, AppWindow *window) {
	app->plane = _pipe->createPlane(window);
	if (!app->plane) {
		return;
	}

	if (_mode == Mode::Stack) {
		relayout();
		return;
	}

	// Enabled, so the client draws, but invisible until it has
	applyLayout();

	app->showTimer = _app->getLooper()->schedule(
			sprt::dispatch::TimeInterval::microseconds(WindowManager_showTimeout),
			[this, app = Rc<App>(app)](sprt::dispatch::Handle *, bool success) {
		app->showTimer = nullptr;
		if (success && app->server && app->plane && !app->shown) {
			log::source().warn("WindowManager", app->label,
					": no frame from the client in time; shown as it is");
			show(app);
		}
	}, this);
}

void WindowManager::handleWindowClosed(App *app) {
	if (app->showTimer) {
		app->showTimer->cancel();
		app->showTimer = nullptr;
	}
	if (app->plane) {
		_pipe->destroyPlane(app->plane);
		app->plane = nullptr;
	}
	app->window = nullptr;

	if (_mode == Mode::Stack) {
		relayout();
		return;
	}

	if (_pending == app) {
		_pending = nullptr;
	}
	if (_active == app) {
		_active = app->role == Role::Application ? getSystem(Role::Shell) : nullptr;
	}

	// A program that is gone for good leaves the list; a relaunched shell or shade takes its label
	if (!app->running) {
		for (auto it = _apps.begin(); it != _apps.end(); ++it) {
			if (it->get() == app) {
				_apps.erase(it);
				break;
			}
		}
	}

	applyLayout();
	sendApps();
}

void WindowManager::handleProcessExit(App *app, int exitCode, Status status) {
	log::source().info("WindowManager", app->label, " exited with ", exitCode, " (",
			sprt::status::getStatusName(status), ")");

	app->running = false;
	app->exitCode = exitCode;
	app->process = nullptr;
	_app->removeBearerKey(app->label);

	// Its session may take seconds to time out; the window goes now.
	if (app->window) {
		if (auto window = app->window->getWindow()) {
			window->close(false);
		}
	} else if (_mode == Mode::Shell) {
		for (auto it = _apps.begin(); it != _apps.end(); ++it) {
			if (it->get() == app) {
				_apps.erase(it);
				break;
			}
		}
	}

	if (!_started || _mode != Mode::Shell || app->role == Role::Application) {
		sendApps();
		return;
	}

	// The shell and the shade come back, unless they keep failing
	auto &restarts = app->role == Role::Shell ? _shellRestarts : _shadeRestarts;
	auto lived = sp::platform::clock(ClockType::Monotonic) - app->launchTime;
	restarts.quickExits = lived < WindowManager_quickExit ? restarts.quickExits + 1 : 0;
	if (restarts.quickExits >= WindowManager_maxQuickExits) {
		restarts.failed = true;
		log::source().error("WindowManager", app->label, " exited ", restarts.quickExits,
				" times in a row within ", WindowManager_quickExit / 1'000'000,
				" s; not restarting it");
		return;
	}

	auto role = app->role;
	_app->getLooper()->schedule(
			sprt::dispatch::TimeInterval::microseconds(WindowManager_restartDelay),
			[this, role](sprt::dispatch::Handle *, bool success) {
		if (success && _started) {
			launchSystem(role);
		}
	}, this);
}

void WindowManager::handlePlanePublished(compositor::DisplayPlane *plane, uint64_t serial) {
	if (_mode != Mode::Shell) {
		return;
	}
	auto app = getApp(plane);
	if (!app || app->shown) {
		return;
	}

	// The client's frame: the window is its now, and the frame is newer than any the server's
	// own scene presented
	auto window = plane->getWindow();
	if (window->getRenderClient() == static_cast<core::RenderClientChannel *>(window->getDirector())
			|| serial <= app->serverFrames) {
		return;
	}
	show(app);
}

void WindowManager::handleAppMessage(NotNull<RemoteSession> session, Value &&value,
		Rc<AppReply> &&reply) {
	auto app = getApp(session->getLabel());
	const Value &msg = value;
	auto type = protocol::getType(msg);

	if (reply) {
		if (type == protocol::Catalog) {
			reply->send(protocol::makeCatalog(_catalog->getEntries()));
		} else if (type == protocol::ListApps) {
			reply->send(protocol::makeApps(protocol::ListApps, getRunningApps()));
		} else {
			reply->refuse();
		}
		return;
	}

	if (!app) {
		return;
	}

	if (type == protocol::Launch) {
		launchApp(msg.getString("id"));
	} else if (type == protocol::Home) {
		goHome();
	} else if (type == protocol::Switch) {
		switchTo(msg.getString("label"));
	} else if (type == protocol::Close) {
		closeApp(msg.getString("label"));
	} else if (type == protocol::Shade && app->role == Role::Shade) {
		setShadeOpen(msg.getBool("open"));
	} else if (type == protocol::InputRegion && app->role == Role::Shade) {
		_shadeRegion = protocol::readInputRegion(msg);
		applyLayout();
	}
}

Rc<WindowManager::App> WindowManager::launch(Role role, StringView label, StringView command,
		LaunchStyle style, Extent2 window) {
	auto app = Rc<App>::alloc();
	app->server = this;
	app->role = role;
	app->index = uint32_t(_apps.size() + 1);
	app->label = label.str<Interface>();
	app->command = command.str<Interface>();

	// The label names files: no path or address separators in them
	auto name = app->label;
	for (auto &c : name) {
		if (c == ':' || c == '/') {
			c = '-';
		}
	}

	auto pid = ::getpid();
	app->inspector = toString("unix:/tmp/wmserver-", pid, "-", name, ".sock");
	app->log = toString("/tmp/wmserver-", pid, "-", name, ".log");

	// The launch token is the program's credential: its hash is the key its session presents.
	uint8_t raw[16];
	valid::makeRandomBytes(raw, sizeof(raw));
	auto token = base16::encode<Interface>(BytesView(raw, sizeof(raw)));
	auto key = crypto::Sha512::perform(StringView(token));
	_app->addBearerKey(BytesView(key.data(), key.size()), app->label);

	String cmd;
	if (style == LaunchStyle::Positional) {
		cmd = toString("exec env XL_LAUNCH_TOKEN=", token,
				" XENOLITH_INSPECTOR_ADDRESS=", app->inspector,
				" XL_CLIENT_CREATE_WINDOW=", app->label, ":", window.width, "x", window.height, " ",
				command, " '", _address, "' >'", app->log, "' 2>&1");
	} else {
		cmd = toString("exec env XL_LAUNCH_TOKEN=", token,
				" XENOLITH_INSPECTOR_ADDRESS=", app->inspector, " ", command, " --connect '",
				_address, "' >'", app->log, "' 2>&1");
	}

	app->process = _app->getLooper()->spawnProcess(cmd, [](StringView) { },
			[this, app](int exitCode, Status st) { handleProcessExit(app, exitCode, st); }, this,
			sprt::dispatch::ProcessFlags::KillProcessTree);

	if (!app->process) {
		log::source().error("WindowManager", "fail to launch ", app->label, ": ", command);
		_app->removeBearerKey(app->label);
		return nullptr;
	}

	app->running = true;
	app->launchTime = sp::platform::clock(ClockType::Monotonic);
	_apps.emplace_back(app);
	log::source().info("WindowManager", "launched ", app->label, ": ", command);
	return app;
}

void WindowManager::launchSystem(Role role) {
	auto &restarts = role == Role::Shell ? _shellRestarts : _shadeRestarts;
	if (restarts.failed) {
		return;
	}

	auto path = role == Role::Shell ? _catalog->resolve("examples/os/shell", "wmshell")
									: _catalog->resolve("examples/os/shade", "wmshade");
	if (!filesystem::exists(FileInfo(path))) {
		restarts.failed = true;
		log::source().error("WindowManager", "not built: ", path);
		return;
	}

	launch(role, WindowManager_getRoleName(role), toString("'", path, "'"), LaunchStyle::Connect,
			getWindowExtent(role));
}

void WindowManager::show(App *app) {
	if (app->shown || !app->plane) {
		return;
	}

	app->shown = true;
	if (app->showTimer) {
		app->showTimer->cancel();
		app->showTimer = nullptr;
	}

	switch (app->role) {
	case Role::Shell:
		if (!_active) {
			_active = app;
		}
		break;
	case Role::Application:
		if (_pending == app) {
			_pending = nullptr;
			_active = app;
		}
		break;
	case Role::Shade: break;
	}

	applyLayout();
	sendApps();
}

void WindowManager::relayout() {
	auto extent = _pipe->getExtent();

	compositor::DisplayPlane *top = nullptr;
	int32_t z = 0;
	for (auto &it : _apps) {
		if (!it->plane) {
			continue;
		}
		auto &state = it->plane->getPending();
		state.src = URect(0, 0, extent.width, extent.height);
		state.dst = IRect(0, 0, extent.width, extent.height);
		state.z = z++;
		state.enabled = false;
		top = it->plane;
	}

	if (top) {
		top->getPending().enabled = true;
	}

	auto st = _pipe->commit();
	if (!sprt::status::isSuccessful(st)) {
		log::source().error("WindowManager", "layout refused: ", sprt::status::getStatusName(st));
		for (auto &it : _pipe->getPlanes()) { it->revert(); }
	}
	_pipe->setFocusedPlane(top);
}

void WindowManager::applyLayout() {
	auto extent = _pipe->getExtent();
	auto bar = getBarHeight();
	auto below = IRect(0, int32_t(bar), extent.width, extent.height - bar);
	auto screen = IRect(0, 0, extent.width, extent.height);

	for (auto &it : _apps) {
		if (!it->plane) {
			continue;
		}

		auto &state = it->plane->getPending();
		auto window = getWindowExtent(it->role);
		state.src = URect(0, 0, window.width, window.height);
		state.dst = it->role == Role::Shade ? screen : below;
		state.blend = it->role == Role::Shade ? compositor::PlaneBlend::Premultiplied
											  : compositor::PlaneBlend::Opaque;

		if (!it->shown) {
			state.z = WindowManager_zPending;
			state.alpha = 0.0f;
			state.enabled = true;
			continue;
		}

		state.alpha = 1.0f;
		switch (it->role) {
		case Role::Shell:
			state.z = WindowManager_zShell;
			state.enabled = _active == it;
			break;
		case Role::Application:
			state.z = WindowManager_zApplication;
			state.enabled = _active == it;
			break;
		case Role::Shade:
			state.z = WindowManager_zShade;
			state.enabled = true;
			state.inputRegion = _shadeRegion;
			if (state.inputRegion.empty()) {
				state.inputRegion.emplace_back(IRect(0, 0, extent.width, bar));
			}
			break;
		}
	}

	auto st = _pipe->commit();
	if (!sprt::status::isSuccessful(st)) {
		log::source().error("WindowManager", "layout refused: ", sprt::status::getStatusName(st));
		for (auto &it : _pipe->getPlanes()) { it->revert(); }
	}

	App *focus = _shadeOpen ? getSystem(Role::Shade) : _active.get();
	_pipe->setFocusedPlane(focus && focus->shown ? focus->plane.get() : nullptr);
}

void WindowManager::sendApps() {
	auto shade = getSystem(Role::Shade);
	if (shade && shade->session) {
		_app->sendAppNotification(shade->session,
				protocol::makeApps(protocol::Apps, getRunningApps()));
	}
}

void WindowManager::sendStatus() {
	uint32_t apps = 0;
	for (auto &it : _apps) {
		if (it->role == Role::Application && it->running) {
			++apps;
		}
	}

	auto focused = _pipe->getFocusedPlane();
	auto &stats = _stats.update(_pipe, focused, apps);

	auto shade = getSystem(Role::Shade);
	if (shade && shade->session) {
		_app->sendAppNotification(shade->session, protocol::makeStatus(stats));
	}
}

void WindowManager::scheduleStatus() {
	_statusTimer = _app->getLooper()->schedule(sprt::dispatch::TimeInterval::seconds(1),
			[this](sprt::dispatch::Handle *, bool success) {
		_statusTimer = nullptr;
		if (success && _started) {
			sendStatus();
			scheduleStatus();
		}
	}, this);
}

Vector<protocol::RunningApp> WindowManager::getRunningApps() const {
	Vector<protocol::RunningApp> ret;
	for (auto &it : _apps) {
		if (it->role != Role::Application || !it->running) {
			continue;
		}
		protocol::RunningApp app;
		app.label = it->label;
		app.id = it->catalogId;
		if (auto entry = _catalog ? _catalog->get(it->catalogId) : nullptr) {
			app.title = entry->title;
			app.icon = entry->icon;
		}
		app.active = _active == it;
		ret.emplace_back(sp::move(app));
	}
	return ret;
}

WindowManager::App *WindowManager::getApp(StringView label) const {
	// newest first: a relaunched shell or shade has the label its predecessor had
	for (auto it = _apps.rbegin(); it != _apps.rend(); ++it) {
		if ((*it)->label == label) {
			return *it;
		}
	}
	return nullptr;
}

WindowManager::App *WindowManager::getApp(const compositor::DisplayPlane *plane) const {
	for (auto &it : _apps) {
		if (it->plane == plane) {
			return it;
		}
	}
	return nullptr;
}

WindowManager::App *WindowManager::getSystem(Role role) const {
	for (auto it = _apps.rbegin(); it != _apps.rend(); ++it) {
		if ((*it)->role == role && (*it)->running) {
			return *it;
		}
	}
	return nullptr;
}

uint32_t WindowManager::getBarHeight() const {
	return uint32_t(protocol::StatusBarHeight * _host->getConstraints().density + 0.5f);
}

Extent2 WindowManager::getWindowExtent(Role role) const {
	auto extent = _pipe->getExtent();
	if (role == Role::Shade) {
		return extent;
	}
	return Extent2(extent.width, extent.height - getBarHeight());
}

compositor::DisplayPlane *WindowManager::resolvePlane(const Value &args) const {
	if (args.isInteger("plane")) {
		return _pipe->getPlane(uint32_t(args.getInteger("plane")));
	}
	if (args.isString("window")) {
		auto id = args.getString("window");
		for (auto &it : _pipe->getPlanes()) {
			if (it->getWindow()->getId() == id) {
				return it;
			}
		}
	}
	return nullptr;
}

Value WindowManager::commitOrRevert() {
	Value ret;
	auto st = _pipe->commit();
	if (sprt::status::isSuccessful(st)) {
		ret.setBool(true, "ok");
		return ret;
	}

	for (auto &it : _pipe->getPlanes()) { it->revert(); }
	ret.setBool(false, "ok");
	ret.setInteger(toInt(st), "status");
	ret.setString(sprt::status::getStatusName(st), "error");
	return ret;
}

Value WindowManager::encodePlane(const compositor::DisplayPlane *plane) const {
	Value ret;
	auto &state = plane->getState();
	auto window = plane->getWindow();
	ret.setInteger(plane->getId(), "id");
	ret.setString(window->getId(), "window");
	if (auto app = getApp(plane)) {
		ret.setString(app->label, "app");
		ret.setString(app->inspector, "inspector");
		ret.setString(WindowManager_getRoleName(app->role), "role");
		ret.setBool(app->shown, "shown");
	}
	ret.setInteger(state.z, "z");
	ret.setBool(state.enabled, "enabled");
	ret.setBool(hasFlag(window->getWindowState(), core::WindowState::Minimized), "minimized");
	ret.setBool(hasFlag(window->getWindowState(), core::WindowState::Focused), "focused");
	ret.setBool(hasFlag(window->getWindowState(), core::WindowState::Pointer), "pointer");
	ret.setBool(state.focusable, "focusable");
	auto &input = ret.emplace("input");
	input.setArray(Value::ArrayType());
	for (auto &it : state.inputRegion) {
		input.addValue(WindowManager_encodeRect(it.x, it.y, it.width, it.height));
	}
	ret.setDouble(state.alpha, "alpha");
	ret.setString(state.blend == compositor::PlaneBlend::Opaque ? "opaque" : "premultiplied",
			"blend");
	ret.setValue(
			WindowManager_encodeRect(state.src.x, state.src.y, state.src.width, state.src.height),
			"src");
	ret.setValue(
			WindowManager_encodeRect(state.dst.x, state.dst.y, state.dst.width, state.dst.height),
			"dst");
	ret.setInteger(plane->getPublished(), "published");
	ret.setInteger(plane->getLatestSerial(), "latestSerial");
	ret.setInteger(plane->getDisplayLinks(), "displayLinks");
	ret.setBool(plane->isPaced(), "paced");
	return ret;
}

Value WindowManager::encodeState() const {
	Value ret;
	ret.setBool(_pipe->isAttached(), "attached");
	ret.setString(_mode == Mode::Stack ? "stack" : "shell", "mode");
	if (_active) {
		ret.setString(_active->label, "active");
	}
	if (_pending) {
		ret.setString(_pending->label, "pending");
	}
	ret.setBool(_shadeOpen, "shadeOpen");
	ret.setInteger(getBarHeight(), "bar");
	auto &region = ret.emplace("shadeRegion");
	region.setArray(Value::ArrayType());
	for (auto &it : _shadeRegion) {
		region.addValue(WindowManager_encodeRect(it.x, it.y, it.width, it.height));
	}
	auto &failed = ret.emplace("failed");
	failed.setBool(_shellRestarts.failed, "shell");
	failed.setBool(_shadeRestarts.failed, "shade");
	if (_catalog) {
		auto &catalog = ret.emplace("catalog");
		catalog.setArray(Value::ArrayType());
		for (auto &it : _catalog->getApps()) {
			Value entry;
			entry.setString(it.id, "id");
			entry.setString(it.title, "title");
			entry.setString(it.path, "path");
			entry.setBool(it.available, "available");
			catalog.addValue(sp::move(entry));
		}
	}
	auto extent = _pipe->getExtent();
	ret.setValue(Value({Value(int64_t(extent.width)), Value(int64_t(extent.height))}), "extent");
	ret.setString(_address, "address");
	ret.setInteger(_pipe->getMinFrameInterval(), "minFrameInterval");
	ret.setInteger(_pipe->getHostFrames(), "hostFrames");
	ret.setInteger(_pipe->getVblanks(), "vblanks");
	ret.setInteger(_pipe->getSnapshot() ? _pipe->getSnapshot()->serial : 0, "snapshot");
	if (auto focused = _pipe->getFocusedPlane()) {
		ret.setInteger(focused->getId(), "focused");
	}

	auto &planes = ret.emplace("planes");
	planes.setArray(Value::ArrayType());
	for (auto &it : _pipe->getPlanes()) { planes.addValue(encodePlane(it)); }

	auto &apps = ret.emplace("apps");
	apps.setArray(Value::ArrayType());
	for (auto &it : _apps) {
		Value app;
		app.setInteger(it->index, "index");
		app.setString(it->label, "label");
		app.setString(WindowManager_getRoleName(it->role), "role");
		if (!it->catalogId.empty()) {
			app.setString(it->catalogId, "id");
		}
		app.setBool(it->shown, "shown");
		app.setBool(it->session != nullptr, "session");
		app.setString(it->command, "command");
		app.setString(it->inspector, "inspector");
		app.setString(it->log, "log");
		app.setBool(it->running, "running");
		app.setInteger(it->exitCode, "exitCode");
		if (it->window && it->window->getWindow()) {
			app.setString(it->window->getWindow()->getId(), "window");
		}
		if (it->plane) {
			app.setInteger(it->plane->getId(), "plane");
		}
		apps.addValue(sp::move(app));
	}
	return ret;
}

void WindowManager::registerCommands(Node *root) {
	inspector::addCommand(root, "wm-state", "The compositor, its planes and the applications",
			[this](Value &&, Function<void(Value &&)> &&done) { done(encodeState()); });

	inspector::addCommand(root, "wm-plane",
			"Change a plane and commit: { plane|window, dst?, src?: [x,y,w,h], z?, alpha?, "
			"enabled?, blend?: opaque|premultiplied, focusable?, input?: [[x,y,w,h]...] }",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		auto plane = resolvePlane(args);
		if (!plane) {
			Value ret;
			ret.setBool(false, "ok");
			ret.setString("no such plane", "error");
			done(sp::move(ret));
			return;
		}

		auto &state = plane->getPending();
		const Value &req = args;
		if (req.isArray("dst")) {
			auto &v = req.getValue("dst");
			state.dst = IRect(int32_t(v.getInteger(0)), int32_t(v.getInteger(1)),
					uint32_t(v.getInteger(2)), uint32_t(v.getInteger(3)));
		}
		if (req.isArray("src")) {
			auto &v = req.getValue("src");
			state.src = URect(uint32_t(v.getInteger(0)), uint32_t(v.getInteger(1)),
					uint32_t(v.getInteger(2)), uint32_t(v.getInteger(3)));
		}
		if (req.isInteger("z")) {
			state.z = int32_t(req.getInteger("z"));
		}
		if (req.isDouble("alpha") || req.isInteger("alpha")) {
			state.alpha = float(req.getDouble("alpha"));
		}
		if (req.isBool("enabled")) {
			state.enabled = req.getBool("enabled");
		}
		if (req.isString("blend")) {
			state.blend = req.getString("blend") == "premultiplied"
					? compositor::PlaneBlend::Premultiplied
					: compositor::PlaneBlend::Opaque;
		}
		if (req.isBool("focusable")) {
			state.focusable = req.getBool("focusable");
		}
		if (req.isArray("input")) {
			// in the window's pixels, y down; empty: the whole destination
			state.inputRegion.clear();
			for (auto &v : req.getArray("input")) {
				state.inputRegion.emplace_back(IRect(int32_t(v.getInteger(0)),
						int32_t(v.getInteger(1)), uint32_t(v.getInteger(2)),
						uint32_t(v.getInteger(3))));
			}
		}

		auto ret = commitOrRevert();
		ret.setValue(encodePlane(plane), "plane");
		done(sp::move(ret));
	});

	auto setEnabled = [this](bool enabled, Value &&args, Function<void(Value &&)> &&done) {
		auto plane = resolvePlane(args);
		if (!plane) {
			Value ret;
			ret.setBool(false, "ok");
			ret.setString("no such plane", "error");
			done(sp::move(ret));
			return;
		}
		plane->getPending().enabled = enabled;
		auto ret = commitOrRevert();
		ret.setValue(encodePlane(plane), "plane");
		done(sp::move(ret));
	};

	inspector::addCommand(root, "wm-pause", "Pause a plane: { plane|window }",
			[setEnabled](Value &&args, Function<void(Value &&)> &&done) {
		setEnabled(false, sp::move(args), sp::move(done));
	});

	inspector::addCommand(root, "wm-resume", "Resume a plane: { plane|window }",
			[setEnabled](Value &&args, Function<void(Value &&)> &&done) {
		setEnabled(true, sp::move(args), sp::move(done));
	});

	auto answer = [](Status st) {
		Value ret;
		ret.setBool(sprt::status::isSuccessful(st), "ok");
		if (!sprt::status::isSuccessful(st)) {
			ret.setString(sprt::status::getStatusName(st), "error");
		}
		return ret;
	};

	inspector::addCommand(root, "wm-launch", "Launch a catalog application: { id }",
			[this, answer](Value &&args, Function<void(Value &&)> &&done) {
		const Value &req = args;
		done(answer(launchApp(req.getString("id"))));
	});

	inspector::addCommand(root, "wm-home", "Bring the shell up",
			[this, answer](Value &&, Function<void(Value &&)> &&done) {
		goHome();
		done(answer(Status::Ok));
	});

	inspector::addCommand(root, "wm-switch", "Bring an application up: { label }",
			[this, answer](Value &&args, Function<void(Value &&)> &&done) {
		const Value &req = args;
		done(answer(switchTo(req.getString("label"))));
	});

	inspector::addCommand(root, "wm-close", "Close an application: { label }",
			[this, answer](Value &&args, Function<void(Value &&)> &&done) {
		const Value &req = args;
		done(answer(closeApp(req.getString("label"))));
	});

	inspector::addCommand(root, "wm-focus",
			"Give a plane the keyboard: { plane|window }; without one, nobody has it",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		Value ret;
		const Value &req = args;
		auto plane = resolvePlane(args);
		if (!plane && (req.isInteger("plane") || req.isString("window"))) {
			ret.setBool(false, "ok");
			ret.setString("no such plane", "error");
		} else if (plane && !plane->getState().focusable) {
			ret.setBool(false, "ok");
			ret.setString("the plane is not focusable", "error");
		} else {
			_pipe->setFocusedPlane(plane);
			ret.setBool(true, "ok");
			if (plane) {
				ret.setInteger(plane->getId(), "focused");
			}
		}
		done(sp::move(ret));
	});

	inspector::addCommand(root, "wm-routes",
			"The last input routes, oldest first: { clear? } -> { routes }",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		auto router = _pipe->getInputRouter();
		Value ret;
		auto &routes = ret.emplace("routes");
		routes.setArray(Value::ArrayType());
		for (auto &it : router->getRoutes()) {
			Value route;
			route.setString(core::getInputEventName(it.event), "event");
			route.setInteger(it.id, "id");
			route.setValue(Value({Value(it.host.x), Value(it.host.y)}), "host");
			route.setInteger(it.plane, "plane");
			route.setValue(Value({Value(it.local.x), Value(it.local.y)}), "local");
			route.setString(compositor::InputRouter::getReasonName(it.reason), "reason");
			routes.addValue(sp::move(route));
		}
		const Value &req = args;
		if (req.getBool("clear")) {
			router->clearRoutes();
		}
		ret.setBool(true, "ok");
		done(sp::move(ret));
	});
}

} // namespace stappler::xenolith::wm
