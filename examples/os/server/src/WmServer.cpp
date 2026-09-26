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

#include "WmServer.h"
#include "WmClientScene.h"

#include "XLAppWindow.h"
#include "XLServerAppThread.h"
#include "XLRemoteSession.h"
#include "XLSceneInspector.h"
#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "SPValid.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/utils/base16.h>

#include <stdlib.h>
#include <unistd.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

static Value WmServer_encodeRect(int64_t x, int64_t y, int64_t w, int64_t h) {
	Value ret;
	ret.addInteger(x);
	ret.addInteger(y);
	ret.addInteger(w);
	ret.addInteger(h);
	return ret;
}

// A colour from XL_WM_BG, "RRGGBB"; the default is a dark grey.
static Color4F WmServer_readBackground() {
	if (auto env = ::getenv("XL_WM_BG")) {
		auto value = StringView(env).readInteger(16);
		if (value.valid()) {
			auto rgb = uint32_t(value.get());
			return Color4F(float((rgb >> 16) & 0xFF) / 255.0f, float((rgb >> 8) & 0xFF) / 255.0f,
					float(rgb & 0xFF) / 255.0f, 1.0f);
		}
	}
	return Color4F(0.12f, 0.12f, 0.14f, 1.0f);
}

WmServer::~WmServer() { stop(); }

bool WmServer::init(NotNull<ServerAppThread> app, NotNull<AppWindow> host) {
	_app = app.get();
	_host = host.get();
	_pipe = Rc<compositor::SwapchainDisplayPipe>::create(app, host);
	if (!_pipe) {
		return false;
	}

	// XL_WM_FPS: the vblank rate the planes draw at; a headless host has no pace of its own.
	uint64_t fps = 60;
	if (auto env = ::getenv("XL_WM_FPS")) {
		fps = uint64_t(StringView(env).readInteger(10).get(60));
	}
	_pipe->setMinFrameInterval(fps > 0 ? 1'000'000 / fps : 0);
	_pipe->setBackground(WmServer_readBackground());
	return true;
}

Status WmServer::start() {
	if (_started) {
		return Status::ErrorAlreadyPerformed;
	}
	_started = true;

	auto st = _pipe->attach([](Status st) {
		if (sprt::status::isSuccessful(st)) {
			log::source().info("WmServer", "the compositor owns the host window");
		} else {
			log::source().error("WmServer",
					"fail to attach the compositor: ", sprt::status::getStatusName(st));
		}
	});
	if (!sprt::status::isSuccessful(st)) {
		auto loop = _app->getGlLoop();
		log::source().error("WmServer", "no compositor for this graphics API (",
				loop && loop->getInstance() ? toInt(loop->getInstance()->getApi()) : 0,
				"); run with --gapi vulkan or --gapi soft");
		return st;
	}

	if (auto env = ::getenv("XL_WM_ADDRESS")) {
		_address = StringView(env).str<Interface>();
	} else {
		_address = toString("shm:/dev/shm/wmserver-", ::getpid());
	}

	// Every client presents a key made for it; the session's own key is one nobody holds.
	uint8_t raw[32];
	valid::makeRandomBytes(raw, sizeof(raw));
	auto sessionKey = crypto::Sha512::perform(BytesView(raw, sizeof(raw)));

	Vector<StringView> commands;
	if (auto env = ::getenv("XL_WM_APPS")) {
		StringView(env).split<StringView::Chars<';'>>([&](StringView cmd) {
			cmd.trimChars<StringView::WhiteSpace>();
			if (!cmd.empty()) {
				commands.emplace_back(cmd);
			}
		});
	}

	_app->setRequireLabelledKeys(true);
	_app->setMaxClientWindows(1);
	_app->setMaxRemoteClients(uint32_t(commands.size() + 2));
	_app->setClientWindowHandler(
			[this](NotNull<RemoteSession> session, NotNull<sprt::window::WindowInfo> info,
					Rc<WindowSceneInfo> &out) -> Status {
		return handleClientWindow(session, info, out);
	});

	if (!_app->startSession(_address, BytesView(sessionKey.data(), sessionKey.size()))) {
		log::source().error("WmServer", "fail to open the session on '", _address, "'");
		return Status::ErrorInvalidArguemnt;
	}

	log::source().info("WmServer", "session on '", _address, "', ", commands.size(), " app(s)");

	for (auto &it : commands) { launch(it); }
	return Status::Ok;
}

void WmServer::stop() {
	if (!_started) {
		return;
	}
	_started = false;

	if (_app) {
		_app->setClientWindowHandler(nullptr);
	}

	auto apps = sp::move(_apps);
	_apps.clear();
	for (auto &it : apps) {
		it->server = nullptr;
		if (it->process) {
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

void WmServer::relayout() {
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
		log::source().error("WmServer", "layout refused: ", sprt::status::getStatusName(st));
		for (auto &it : _pipe->getPlanes()) { it->revert(); }
	}
	_pipe->setFocusedPlane(top);
}

Status WmServer::handleClientWindow(NotNull<RemoteSession> session,
		NotNull<sprt::window::WindowInfo> info, Rc<WindowSceneInfo> &out) {
	auto app = getApp(session->getLabel());
	if (!app || app->window) {
		// not an application of ours, or one that has its window already
		return Status::Declined;
	}

	auto extent = _pipe->getExtent();

	// The window is the screen: virtual, full size, no decorations of its own.
	info->type = sprt::window::WindowType::Root;
	info->title = app->label;
	info->flags |= sprt::window::WindowCreationFlags::Virtual;
	info->flags &= ~sprt::window::WindowCreationFlags::UserSpaceDecorations;
	info->rect = IRect(0, 0, extent.width, extent.height);
	// Host pixels at the host's density: the plane shows the window 1:1.
	info->density = _host->getConstraints().density;

	auto ref = Rc<App>(app);
	out = Rc<WindowSceneInfo>::create(
			[ref](NotNull<AppThread> thread, NotNull<core::RenderServerChannel> channel,
					const core::FrameConstraints &constraints) -> Rc<Scene> {
		auto window = dynamic_cast<AppWindow *>(channel.get());
		if (auto server = ref->server; server && window) {
			ref->plane = server->_pipe->createPlane(window);
			server->relayout();
		}
		return Rc<WmClientScene>::create(thread, channel, constraints, ref->label);
	},
			[ref](NotNull<WindowSceneInfo>) {
		if (auto server = ref->server) {
			server->handleWindowClosed(ref);
		}
	});

	app->window = out;
	return out ? Status::Ok : Status::Declined;
}

void WmServer::handleWindowClosed(App *app) {
	if (app->plane) {
		_pipe->destroyPlane(app->plane);
		app->plane = nullptr;
	}
	app->window = nullptr;
	relayout();
}

void WmServer::handleProcessExit(App *app, int exitCode) {
	log::source().info("WmServer", app->label, " exited with ", exitCode);

	app->running = false;
	app->exitCode = exitCode;
	app->process = nullptr;
	_app->removeBearerKey(app->label);

	// Its session may take seconds to time out; the window goes now.
	if (app->window) {
		if (auto window = app->window->getWindow()) {
			window->close(false);
		}
	}
}

void WmServer::launch(StringView command) {
	auto app = Rc<App>::alloc();
	app->server = this;
	app->index = uint32_t(_apps.size() + 1);
	app->label = toString("app", app->index);
	app->command = command.str<Interface>();

	auto pid = ::getpid();
	app->inspector = toString("unix:/tmp/wmserver-", pid, "-", app->label, ".sock");
	app->log = toString("/tmp/wmserver-", pid, "-", app->label, ".log");

	// The launch token is the application's credential: its hash is the key its session presents.
	uint8_t raw[16];
	valid::makeRandomBytes(raw, sizeof(raw));
	auto token = base16::encode<Interface>(BytesView(raw, sizeof(raw)));
	auto key = crypto::Sha512::perform(StringView(token));
	_app->addBearerKey(BytesView(key.data(), key.size()), app->label);

	auto extent = _pipe->getExtent();
	auto cmd = toString("exec env XL_LAUNCH_TOKEN=", token,
			" XENOLITH_INSPECTOR_ADDRESS=", app->inspector, " XL_CLIENT_CREATE_WINDOW=", app->label,
			":", extent.width, "x", extent.height, " ", command, " '", _address, "' >'", app->log,
			"' 2>&1");

	app->process = _app->getLooper()->spawnProcess(cmd, [](StringView) { },
			[this, app](int exitCode, Status) { handleProcessExit(app, exitCode); }, this,
			sprt::dispatch::ProcessFlags::KillProcessTree);

	if (!app->process) {
		log::source().error("WmServer", "fail to launch ", app->label, ": ", command);
		_app->removeBearerKey(app->label);
		return;
	}

	app->running = true;
	_apps.emplace_back(app);
	log::source().info("WmServer", "launched ", app->label, ": ", command);
}

WmServer::App *WmServer::getApp(StringView label) {
	for (auto &it : _apps) {
		if (it->label == label) {
			return it;
		}
	}
	return nullptr;
}

const WmServer::App *WmServer::getApp(const compositor::DisplayPlane *plane) const {
	for (auto &it : _apps) {
		if (it->plane == plane) {
			return it;
		}
	}
	return nullptr;
}

compositor::DisplayPlane *WmServer::resolvePlane(const Value &args) const {
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

Value WmServer::commitOrRevert() {
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

Value WmServer::encodePlane(const compositor::DisplayPlane *plane) const {
	Value ret;
	auto &state = plane->getState();
	auto window = plane->getWindow();
	ret.setInteger(plane->getId(), "id");
	ret.setString(window->getId(), "window");
	if (auto app = getApp(plane)) {
		ret.setString(app->label, "app");
		ret.setString(app->inspector, "inspector");
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
		input.addValue(WmServer_encodeRect(it.x, it.y, it.width, it.height));
	}
	ret.setDouble(state.alpha, "alpha");
	ret.setString(state.blend == compositor::PlaneBlend::Opaque ? "opaque" : "premultiplied",
			"blend");
	ret.setValue(WmServer_encodeRect(state.src.x, state.src.y, state.src.width, state.src.height),
			"src");
	ret.setValue(WmServer_encodeRect(state.dst.x, state.dst.y, state.dst.width, state.dst.height),
			"dst");
	ret.setInteger(plane->getPublished(), "published");
	ret.setInteger(plane->getLatestSerial(), "latestSerial");
	ret.setInteger(plane->getDisplayLinks(), "displayLinks");
	ret.setBool(plane->isPaced(), "paced");
	return ret;
}

Value WmServer::encodeState() const {
	Value ret;
	ret.setBool(_pipe->isAttached(), "attached");
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

void WmServer::registerCommands(Node *root) {
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
