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

#include "XLContext.h"
#include "SPStringDetail.h"
#include "XLAppThread.h"
#include "XLAppWindow.h"
#include "XLContextInfo.h"
#include "XLCoreEnum.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "platform/XLContextController.h"
#include "platform/XLContextNativeWindow.h"

#if MODULE_XENOLITH_FONT
#include "XLFontLocale.h"
#include "XLFontComponent.h"
#endif

#include "SPSharedModule.h"

#include <dlfcn.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ContentInitializer::ContentInitializer() { }

ContentInitializer::~ContentInitializer() { terminate(); }

ContentInitializer::ContentInitializer(ContentInitializer &&other) {
	alloc = other.alloc;
	pool = other.pool;
	tmpPool = other.tmpPool;
	init = other.init;

	liveReloadPath = sp::move(other.liveReloadPath);
	liveReloadCachePath = sp::move(other.liveReloadCachePath);
	liveReloadLibrary = sp::move(other.liveReloadLibrary);

	other.tmpPool = nullptr;
	other.pool = nullptr;
	other.alloc = nullptr;
	other.init = false;
}

ContentInitializer &ContentInitializer::operator=(ContentInitializer &&other) {
	terminate();

	alloc = other.alloc;
	pool = other.pool;
	tmpPool = other.tmpPool;
	init = other.init;

	liveReloadPath = sp::move(other.liveReloadPath);
	liveReloadCachePath = sp::move(other.liveReloadCachePath);
	liveReloadLibrary = sp::move(other.liveReloadLibrary);

	other.tmpPool = nullptr;
	other.pool = nullptr;
	other.alloc = nullptr;
	other.init = false;
	return *this;
}

bool ContentInitializer::initialize(int argc, const char *argv[]) {
	if (init) {
		return true;
	}

	alloc = memory::allocator::create();
	pool = memory::pool::create(alloc);
	tmpPool = memory::pool::create(pool);

	// Context pool should be main thread's pool
	thread::ThreadInfo::setThreadPool(pool);

	int result = 0;
	if (!sp::initialize(argc, argv, result)) {
		init = false;
	} else {
		init = true;
	}
	return init;
}

void ContentInitializer::terminate() {
	if (alloc) {
		memory::pool::destroy(tmpPool);
		memory::pool::destroy(pool);
		memory::allocator::destroy(alloc);
		sp::terminate();

		tmpPool = nullptr;
		pool = nullptr;
		alloc = nullptr;
		init = false;
	}
}

XL_DECLARE_EVENT_CLASS(Context, onNetworkStateChanged);
XL_DECLARE_EVENT_CLASS(Context, onThemeChanged);
XL_DECLARE_EVENT_CLASS(Context, onSystemNotification);
XL_DECLARE_EVENT_CLASS(Context, onLiveReload);
XL_DECLARE_EVENT_CLASS(Context, onMessageToken)
XL_DECLARE_EVENT_CLASS(Context, onRemoteNotification)

static int Context_runWithConfig(ContextConfig &&config, ContentInitializer &&init) {
	Rc<Context> ctx;

	auto makeContextSymbol = SharedModule::acquireTypedSymbol<Context::SymbolMakeContextSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, Context::SymbolMakeContextName);
	if (makeContextSymbol) {
		ctx = makeContextSymbol(move(config), sp::move(init));
	} else {
		ctx = Rc<Context>::create(move(config), sp::move(init));
	}

	if (!ctx) {
		log::source().error("Context", "Fail to create Context");
		return -1;
	}

	auto container = Rc<platform::ContextContainer>::create();
	container->context = sp::move(ctx);
	container->controller = container->context->getController();

	return container->controller->run(container);
}

int Context::run(int argc, const char **argv) {
	auto runWithConfig = [&](ContextConfig &&config, ContentInitializer &&init) -> int {
		if (hasFlag(config.flags, CommonFlags::Help)) {
			auto printHelpSymbol = SharedModule::acquireTypedSymbol<SymbolPrintHelpSignature>(
					buildconfig::MODULE_APPCOMMON_NAME, SymbolPrintHelpName);
			if (printHelpSymbol) {
				printHelpSymbol(config, argc, argv);
			} else {
				auto appName = filepath::lastComponent(StringView(argv[0]));

				std::cout << appName << " <options>:\n";
				ContextConfig::getCommandLineParser().describe(
						[&](StringView str) { std::cout << str; });

				auto helpString = SharedModule::acquireTypedSymbol<SymbolHelpStringSignature>(
						buildconfig::MODULE_APPCOMMON_NAME, SymbolHelpStringName);
				if (helpString) {
					std::cout << helpString << "\n";
				}
			}
			return 0;
		}

		if (hasFlag(config.flags, CommonFlags::Verbose)) {
			std::cerr << " Current work dir: " << stappler::filesystem::currentDir<Interface>()
					  << "\n";
			std::cerr << " Options: " << stappler::data::EncodeFormat::Pretty << config.encode()
					  << "\n";
		}

		return Context_runWithConfig(sp::move(config), sp::move(init));
	};

	ContentInitializer init;
	init.initialize(argc, argv);

#ifdef EXEC_LIVE_RELOAD
	// clear live reload cache first
	filesystem::mkdir(FileInfo("live_reload_cache", FileInfo::AppRuntime));
	auto liveReloadCache =
			filesystem::findPath<Interface>(FileInfo("live_reload_cache", FileInfo::AppRuntime));
	filesystem::remove(FileInfo(liveReloadCache), true, false);

	auto liveReloadLib = SharedModule::acquireTypedSymbol<const char *>(
			buildconfig::MODULE_APPCONFIG_NAME, "APPCONFIG_EXEC_LIVE_RELOAD_LIBRARY");
	auto libName = filepath::lastComponent(liveReloadLib);
	auto execPath = filesystem::platform::_getApplicationPath<Interface>();
	auto execDir = filepath::root(execPath);

	auto libPath = filepath::merge<Interface>(execDir, libName);
	filesystem::Stat stat;
	if (filesystem::stat(FileInfo(libPath), stat)) {
		auto targetPath = toString(liveReloadCache, "/", libName, ".1");
		filesystem::copy(FileInfo{libPath}, FileInfo(targetPath));

		init.liveReloadLibrary = Rc<LiveReloadLibrary>::create(targetPath, stat.mtime, 1, nullptr);

		if (init.liveReloadLibrary) {
			init.liveReloadPath = libPath;
			init.liveReloadCachePath = liveReloadCache;
			slog().debug("Context", "Run with Live reload library: ", targetPath);
		}
	}
#endif

	auto cfgSymbol = SharedModule::acquireTypedSymbol<SymbolParseConfigCmdSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, SymbolParseConfigCmdName);
	if (cfgSymbol) {
		return runWithConfig(cfgSymbol(argc, argv), sp::move(init));
	} else {
		return runWithConfig(ContextConfig(argc, argv), sp::move(init));
	}
}

int Context::run(NativeContextHandle *ctx) {
	auto cfgSymbol = SharedModule::acquireTypedSymbol<SymbolParseConfigNativeSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, SymbolParseConfigNativeName);
	if (cfgSymbol) {
		return Context_runWithConfig(cfgSymbol(ctx), ContentInitializer());
	} else {
		return Context_runWithConfig(ContextConfig(ctx), ContentInitializer());
	}
}

Context::Context() { }

Context::~Context() { _initializer.terminate(); }

bool Context::init(ContextConfig &&info, ContentInitializer &&init) {
	_initializer = sp::move(init);
	_initializer.initialize(0, nullptr);

	memory::context ctx(_initializer.pool);

	_info = info.context;

	_controller = platform::ContextController::create(this, move(info));

	if (!_controller) {
		log::source().error("Context", "Fail to create ContextController");
		return false;
	}
	_looper = _controller->getLooper();

#if MODULE_XENOLITH_FONT
	auto setLocale = SharedModule::acquireTypedSymbol<decltype(&locale::setLocale)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "locale::setLocale");
	if (setLocale) {
		if (_info->userLanguage.empty()) {
			setLocale(stappler::platform::getOsLocale());
		} else {
			setLocale(_info->userLanguage);
		}
	}
#endif

	if (!_initializer.liveReloadPath.empty()) {
		_actualLiveReloadLibrary = _initializer.liveReloadLibrary;
		// add timer-based watchdog
		// later we implement watchdog, based on event queue
		_liveReloadWatchdog = _looper->scheduleTimer(
				event::TimerInfo{
					.completion = event::TimerInfo::Completion::create<Context>(this,
							[](Context *ctx, event::TimerHandle *, uint32_t value, Status) {
			ctx->updateLiveReload();
		}),
					.interval = TimeInterval::milliseconds(250),
					.count = event::TimerInfo::Infinite,
				},
				this);
	}

	return true;
}

void Context::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) const {
	_looper->performOnThread(sp::move(func), target, immediate, tag);
}

bool Context::isCursorSupported(WindowCursor cursor, bool serverSide) const {
	return _controller->isCursorSupported(cursor, serverSide);
}

WindowCapabilities Context::getWindowCapabilities() const { return _controller->getCapabilities(); }

Status Context::readFromClipboard(Function<void(Status, BytesView, StringView)> &&cb,
		Function<StringView(SpanView<StringView>)> &&tcb, Ref *target) {
	auto request = Rc<platform::ClipboardRequest>::create();
	request->dataCallback = sp::move(cb);
	request->typeCallback = sp::move(tcb);
	request->target = target;

	return _controller->readFromClipboard(sp::move(request));
}

Status Context::probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb, Ref *ref) {
	auto probe = Rc<platform::ClipboardProbe>::create();
	probe->typeCallback = sp::move(cb);
	probe->target = ref;
	return _controller->probeClipboard(sp::move(probe));
}

Status Context::writeToClipboard(Function<Bytes(StringView)> &&cb, SpanView<String> types, Ref *ref,
		StringView label) {
	auto data = Rc<platform::ClipboardData>::create();
	data->label = label.str<Interface>();
	data->encodeCallback = sp::move(cb);
	data->types = types.vec<Interface>();
	data->owner = ref;

	return _controller->writeToClipboard(move(data));
}

void Context::handleConfigurationChanged(Rc<ContextInfo> &&info) { _info = move(info); }

void Context::handleGraphicsLoaded(NotNull<core::Loop> loop) {
	_loop = loop;
	_loop->run();
#if MODULE_XENOLITH_FONT
	auto createFontComponent =
			SharedModule::acquireTypedSymbol< decltype(&font::FontComponent::createFontComponent)>(
					buildconfig::MODULE_XENOLITH_FONT_NAME, "FontComponent::createFontComponent");
	if (createFontComponent) {
		if (auto comp = createFontComponent(this)) {
			addComponent(move(comp));
		}
	}
#endif
}

Value Context::saveState() { return Value(); }

void Context::handleAppThreadCreated(NotNull<AppThread>) {
	log::source().info("Context", "handleAppThreadCreated");
}
void Context::handleAppThreadDestroyed(NotNull<AppThread>) {
	log::source().info("Context", "handleAppThreadDestroyed");
}
void Context::handleAppThreadUpdate(NotNull<AppThread>, const UpdateTime &time) {
	// log::source().info("Context", "handleAppThreadUpdate");
}

core::SwapchainConfig Context::handleAppWindowSurfaceUpdate(NotNull<AppWindow> w,
		const SurfaceInfo &info, bool fastMode) {
	SwapchainConfig ret;
	ret.extent = info.currentExtent;
	ret.imageCount = std::max(uint32_t(3), info.minImageCount);

	ret.presentMode = core::PresentMode::Unsupported;

	auto windowInfo = w->getInfo();

	core::PresentMode preferredPresentMode =
			windowInfo ? windowInfo->preferredPresentMode : core::PresentMode::Mailbox;
	core::ImageFormat imageFormat =
			windowInfo ? windowInfo->imageFormat : core::ImageFormat::R8G8B8A8_UNORM;
	core::ColorSpace colorSpace =
			windowInfo ? windowInfo->colorSpace : core::ColorSpace::SRGB_NONLINEAR_KHR;

	if (preferredPresentMode != core::PresentMode::Unsupported) {
		for (auto &it : info.presentModes) {
			if (it == preferredPresentMode) {
				ret.presentMode = it;
				break;
			}
		}
	}

	if (ret.presentMode == core::PresentMode::Unsupported) {
		log::source().info("Context", "handleAppWindowSurfaceUpdate: fail to set up with ",
				core::getPresentModeName(preferredPresentMode), " PresentMode, fallback to ",
				core::getPresentModeName(info.presentModes.front()));
		ret.presentMode = info.presentModes.front();
	}

	// use Immediate mode as fastest for quick transitions (like on manual window resize)
	if (ret.presentMode != core::PresentMode::Mailbox) {
		if (std::find(info.presentModes.begin(), info.presentModes.end(),
					core::PresentMode::Immediate)
				!= info.presentModes.end()) {
			ret.presentModeFast = core::PresentMode::Immediate;
		}
	}

	auto it = info.formats.begin();
	while (it != info.formats.end()) {
		if (it->first == imageFormat && it->second == colorSpace) {
			break;
		} else {
			++it;
		}
	}

	if (it == info.formats.end()) {
		ret.imageFormat = info.formats.front().first;
		ret.colorSpace = info.formats.front().second;
		log::
				source()
						.info("Context",
								"handleAppWindowSurfaceUpdate: fail to find "
								"(imageFormat:colorspace) pair " "for a window, fallback to (",
								core::getImageFormatName(ret.imageFormat), ":",
								core::getColorSpaceName(ret.colorSpace), ")");
	} else {
		ret.imageFormat = it->first;
		ret.colorSpace = it->second;
	}

	if (hasFlag(w->getInfo()->flags, WindowCreationFlags::UserSpaceDecorations)
			&& hasFlag(info.supportedCompositeAlpha, core::CompositeAlphaFlags::Premultiplied)) {
		// For user-space decoration, compositor can provide Premultiplied alpha mode, use it
		ret.alpha = core::CompositeAlphaFlags::Premultiplied;
	} else if (hasFlag(w->getInfo()->flags, WindowCreationFlags::UserSpaceDecorations)
			&& hasFlag(info.supportedCompositeAlpha, core::CompositeAlphaFlags::Postmultiplied)) {
		ret.alpha = core::CompositeAlphaFlags::Postmultiplied;
	} else if (hasFlag(info.supportedCompositeAlpha, core::CompositeAlphaFlags::Opaque)) {
		ret.alpha = core::CompositeAlphaFlags::Opaque;
	} else if (hasFlag(info.supportedCompositeAlpha, core::CompositeAlphaFlags::Inherit)) {
		ret.alpha = core::CompositeAlphaFlags::Inherit;
	}

	ret.transfer =
			(info.supportedUsageFlags & core::ImageUsage::TransferDst) != core::ImageUsage::None;

	if (ret.presentMode == core::PresentMode::Mailbox) {
		ret.imageCount = std::max(uint32_t(3), ret.imageCount);
	}

	ret.transform = info.currentTransform;

	if (info.fullscreenHandle && info.fullscreenMode != core::FullScreenExclusiveMode::Default) {
		ret.fullscreenMode = info.fullscreenMode;
		ret.fullscreenHandle = info.fullscreenHandle;
	}

	return ret;
}

void Context::handleNativeWindowCreated(NotNull<NativeWindow> w) {
	log::source().info("Context", "handleNativeWindowCreated");

	if (auto appWindow = makeAppWindow(w)) {
		w->setAppWindow(move(appWindow));
	} else {
		log::source().error("Context", "Fail to create AppWindow for NativeWindow");
	}
}

void Context::handleNativeWindowDestroyed(NotNull<NativeWindow> w) {
	log::source().info("Context", "handleNativeWindowDestroyed");
	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->close(true);
	}
}

void Context::handleNativeWindowConstraintsChanged(NotNull<NativeWindow> w,
		core::UpdateConstraintsFlags flags) {
	log::source().info("Context", "handleNativeWindowConstraintsChanged ", toInt(flags));

	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->updateConstraints(flags);
	}
}

void Context::handleNativeWindowInputEvents(NotNull<NativeWindow> w,
		Vector<core::InputEventData> &&events) {
	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->handleInputEvents(sp::move(events));
	}
}

void Context::handleNativeWindowTextInput(NotNull<NativeWindow> w,
		const core::TextInputState &state) {
	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->handleTextInput(state);
	}
}

void Context::handleSystemNotification(SystemNotification note) {
	log::source().info("Context", "handleSystemNotification");
	for (auto &it : _components) { it.second->handleSystemNotification(this, note); }

	onSystemNotification(this, toInt(note));
}

void Context::handleWillDestroy() {
	log::source().info("Context", "handleWillDestroy");
	for (auto &it : _components) { it.second->handleDestroy(this); }
	_components.clear();

	_controller = nullptr;

	if (_loop) {
		_loop->stop();
		_loop = nullptr;
	}
}
void Context::handleDidDestroy() { log::source().info("Context", "handleDidDestroy"); }

void Context::handleWillStop() {
	log::source().info("Context", "handleWillStop");
	if (_running) {
		for (auto &it : _components) { it.second->handleStop(this); }

		_running = false;
	}
}

void Context::handleDidStop() {
	log::source().info("Context", "handleDidStop");
	if (_application) {
		_application->stop();
		_application->waitStopped();
		_application = nullptr;
	}
}

void Context::handleWillPause() {
	log::source().info("Context", "handleWillPause");
	for (auto &it : _components) { it.second->handlePause(this); }
}
void Context::handleDidPause() { log::source().info("Context", "handleDidPause"); }

void Context::handleWillResume() { log::source().info("Context", "handleWillResume"); }

void Context::handleDidResume() {
	log::source().info("Context", "handleDidResume");
	for (auto &it : _components) { it.second->handleResume(this); }
}

void Context::handleWillStart() {
	log::source().info("Context", "handleWillStart");
	_application = makeAppThread();
}

void Context::handleDidStart() {
	log::source().info("Context", "handleDidStart");
	if (!_running) {
		for (auto &it : _components) { it.second->handleStart(this); }

		_application->run();

		_running = true;
	}
}

void Context::handleNetworkStateChanged(NetworkFlags flags) {
	for (auto &it : _components) { it.second->handleNetworkStateChanged(flags); }

	if (_application) {
		_application->handleNetworkStateChanged(flags);
	}

	onNetworkStateChanged(this, toInt(flags));

	if (_application) {
		_application->wakeup();
	}
}

void Context::handleThemeInfoChanged(const ThemeInfo &info) {
	for (auto &it : _components) { it.second->handleThemeInfoChanged(info); }

	if (_application) {
		_application->handleThemeInfoChanged(info);
	}

	onThemeChanged(this, info.encode());

	if (_application) {
		_application->wakeup();
	}
}

bool Context::configureWindow(NotNull<WindowInfo> w) {
	auto caps = _controller->getCapabilities();

	for (auto flag : sp::flags(w->flags)) {
		switch (flag) {
		case WindowCreationFlags::UserSpaceDecorations:
			if (!hasFlag(caps, WindowCapabilities::UserSpaceDecorations)) {
				log::source().warn("Context",
						"WindowCreationFlags::UserSpaceDecorations is not supported");
				w->flags &= ~WindowCreationFlags::UserSpaceDecorations;
			}
			break;
		case WindowCreationFlags::DirectOutput:
			if (!hasFlag(caps, WindowCapabilities::DirectOutput)) {
				log::source().warn("Context", "WindowCreationFlags::DirectOutput is not supported");
				w->flags &= ~WindowCreationFlags::DirectOutput;
			}
			break;
		case WindowCreationFlags::PreferServerSideDecoration: break;
		case WindowCreationFlags::PreferNativeDecoration: break;
		case WindowCreationFlags::PreferServerSideCursors: break;
		default: break;
		}
	}

	return true;
}

void Context::updateMessageToken(BytesView tok) {
	if (tok != _messageToken) {
		onMessageToken(this, _messageToken);
	}
}

void Context::receiveRemoteNotification(Value &&val) { onRemoteNotification(this, move(val)); }

Rc<ScreenInfo> Context::getScreenInfo() const { return _controller->getScreenInfo(); }

void Context::openUrl(StringView str) { _controller->openUrl(str); }

Rc<AppThread> Context::makeAppThread() {
	auto makeAppThreadSymbol =
			SharedModule::acquireTypedSymbol<Context::SymbolMakeAppThreadSignature>(
					buildconfig::MODULE_APPCOMMON_NAME, Context::SymbolMakeAppThreadName);
	if (makeAppThreadSymbol) {
		auto thread = makeAppThreadSymbol(this);
		if (thread) {
			return thread;
		} else {
			slog().error("Context",
					"Fail to create thread with function provided, fallback to default AppThread");
		}
	}

	return Rc<AppThread>::create(this);
}

Rc<AppWindow> Context::makeAppWindow(NotNull<NativeWindow> w) {
	return _controller->makeAppWindow(_application, w);
}

void Context::initializeComponent(NotNull<ContextComponent> comp) {
	_controller->initializeComponent(comp);
}

void Context::updateLiveReload() {
	if (!_initializer.liveReloadPath.empty() && _actualLiveReloadLibrary) {
		auto mtime = _actualLiveReloadLibrary->mtime;

		filesystem::Stat stat;
		if (filesystem::stat(FileInfo{_initializer.liveReloadPath}, stat)) {
			if (stat.mtime != mtime) {
				performLiveReload(stat);
			}
		}
	}
}

void Context::performLiveReload(const filesystem::Stat &stat) {
	if (_initializer.liveReloadPath.empty() || !_initializer.liveReloadLibrary) {
		return;
	}

	uint32_t version = _actualLiveReloadLibrary->library.getVersion();

	++version;

	auto targetPath = toString(_initializer.liveReloadCachePath, "/",
			filepath::lastComponent(_initializer.liveReloadPath), ".", version);

	if (filesystem::copy(FileInfo(_initializer.liveReloadPath), FileInfo(targetPath))) {
		auto newLib = Rc<LiveReloadLibrary>::create(targetPath, stat.mtime, version, _looper);
		if (newLib) {
			_unloadedLiveReloadLibrary = sp::move(_actualLiveReloadLibrary);
			_actualLiveReloadLibrary = sp::move(newLib);
			onLiveReload(this, _actualLiveReloadLibrary.get());
		}
	}
}

} // namespace stappler::xenolith
