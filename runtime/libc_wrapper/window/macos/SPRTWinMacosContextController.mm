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

#define __SPRT_BUILD 1

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wavailability"

#import <AppKit/NSApplication.h>
#import <AppKit/NSPasteboardItem.h>
#import <Network/Network.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#pragma clang diagnostic pop

#include <sprt/runtime/log.h>

#include "private/window/macos/SPRTWinMacosContextController.h"
#include "private/window/macos/SPRTWinMacosDisplayConfigManager.h"
#include "private/window/macos/SPRTWinMacosWindow.h"

#if XL_MACOS_DEBUG
#define XL_MACOS_LOG(...) NSSP::log::source().debug(__VA_ARGS__)
#else
#define XL_MACOS_LOG(...)
#endif

@interface SPRTMacosAppDelegate : NSObject <NSApplicationDelegate> {
	NSApplication *_application;
	NSWindow *_window;
	NSSPWIN::MacosContextController *_controller;
	nw_path_monitor_t _pathMonitor;
	NSSPWIN::ThemeInfo _themeInfo;
	bool _terminated;
};

- (id _Nonnull)init:(NSSPWIN::MacosContextController *)controller;

- (void)run;
- (void)terminate;

- (void)doNothing:(id _Nonnull)object;

@end

@interface XLMacosPasteboardItem : NSPasteboardItem <NSPasteboardItemDataProvider> {
	NSSP::Rc<NSSPWIN::ClipboardData> _data;
	NSSPWIN::Map<NSString *, NSSP::StringView> _types;
	NSArray<NSString *> *_nstypes;
}

- (instancetype)initWithData:(NSSP::Rc<NSSPWIN::ClipboardData> &&)data;

- (NSArray<NSString *> *)dataTypes;

@end

namespace sprt::window {

void MacosContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *) {
	if (config.instance->api == gapi::InstanceApi::None) {
		config.instance->api = gapi::InstanceApi::Vulkan;
	}

	if (config.context) {
		config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;
	}

	if (config.loop) {
		config.loop->defaultFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window) {
		if (config.window->imageFormat == ImageFormat::Undefined) {
			config.window->imageFormat = ImageFormat::B8G8R8A8_UNORM;
		}
	}
}

Rc<MacosContextController> MacosContextController::create(NotNull<Context> ctx, ContextConfig &&cfg,
		NotNull<dispatch::Looper> looper) {
	return Rc<MacosContextController>::create(ctx, sprt::move(cfg), looper);
}

MacosContextController::~MacosContextController() { _appDelegate = nullptr; }

bool MacosContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<dispatch::Looper> looper) {
	if (!ContextController::init(ctx, looper)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	_appDelegate = [[SPRTMacosAppDelegate alloc] init:this];

	return true;
}

int MacosContextController::run(NotNull<ContextContainer> ctx) {
	_context->handleConfigurationChanged(move(_contextInfo));

	_container = ctx;

	[_appDelegate run];

	return ContextController::run(ctx);
}

void MacosContextController::handleContextDidStop() {
	ContextController::handleContextDidStop();

	if (_inTerminate) {
		_looper->poll();
	}
}

void MacosContextController::handleContextWillStart() {
	auto instance = _context->makeInstance(_instanceInfo);
	if (!instance) {
		oslog::vperror(__SPRT_LOCATION, "MacosContextController", "Fail to load gAPI instance");
		_resultCode = -1;
		[_appDelegate terminate];
		return;
	} else {
		if (auto loop = _context->makeLoop(instance, _loopInfo)) {
			_context->handleGraphicsLoaded(loop);
		}
	}

	ContextController::handleContextWillStart();
}

void MacosContextController::handleContextDidStart() {
	ContextController::handleContextDidStart();

	_displayConfigManager =
			Rc<MacosDisplayConfigManager>::create(this, [](NotNull<DisplayConfigManager> m) { });

	if (_windowInfo) {
		if (configureWindow(_windowInfo)) {
			Rc<MacosWindow>::create(this, sprt::move(_windowInfo));
		}
	}
}

void MacosContextController::handleContextDidDestroy() {
	ContextController::handleContextDidDestroy();
	if (!_inTerminate) {
		// Bypass looper with 0.1 sec timeout
		// This shoul be called outside of Looper an memory::Pool context
		[_appDelegate performSelector:@selector(terminate) withObject:nil afterDelay:0.1];
	}
}

void MacosContextController::applicationWillTerminate(bool terminated) {
	_inTerminate = true;
	auto container = sprt::move(_container);
	if (!terminated) {
		stop();
		handleContextWillDestroy();
		handleContextDidDestroy();
	}
	container->controller = nullptr;

	container->context = nullptr;
}


Status MacosContextController::readFromClipboard(Rc<ClipboardRequest> &&req) {
	auto pasteboard = [NSPasteboard generalPasteboard];
	auto types = [NSPasteboard generalPasteboard].types;

	Vector<StringView> targetTypes;
	Map<NSPasteboardType, String> mimeTypes;
	Map<String, NSPasteboardType> utTypes;

	auto addType = [&](NSPasteboardType v, StringView mime) {
		mimeTypes.emplace(v, mime.str<String>());
		auto it = utTypes.find(mime);
		if (it == utTypes.end()) {
			auto u = utTypes.emplace(mime.str<String>(), v).first;
			targetTypes.emplace_back(u->first);
		} else {
			oslog::vpwarn(__SPRT_LOCATION, "MacosContextController",
					"Pasteboard type dublicate: ", mime, " for ", v.UTF8String);
		}
	};

	for (NSPasteboardType v in types) {
		auto type = [UTType typeWithIdentifier:v];
		if (type) {
			auto mime = type.preferredMIMEType;
			if (mime) {
				addType(v, mime.UTF8String);
				continue;
			}
		} else if ([v isEqualToString:NSPasteboardTypeString]) {
			addType(v, "text/plain");
		} else if ([v isEqualToString:NSPasteboardTypeTabularText]) {
			addType(v, "text/x-tabular");
		} else if ([v isEqualToString:NSPasteboardTypeURL]) {
			addType(v, "text/x-uri");
		} else if ([v isEqualToString:NSPasteboardTypeFileURL]) {
			addType(v, "text/x-path");
		}
	}

	auto selectedType = req->typeCallback(targetTypes);
	if (sprt::find(targetTypes.begin(), targetTypes.end(), selectedType) == targetTypes.end()) {
		req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
		return Status::ErrorInvalidArguemnt;
	}

	auto tIt = utTypes.find(selectedType);
	if (tIt == utTypes.end()) {
		req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
		return Status::ErrorInvalidArguemnt;
	}

	auto typesArray = @[tIt->second];
	if (![pasteboard canReadItemWithDataConformingToTypes:typesArray]) {
		req->dataCallback(Status::ErrorInvalidArguemnt, BytesView(), StringView());
		return Status::ErrorInvalidArguemnt;
	}

	for (NSPasteboardItem *item in pasteboard.pasteboardItems) {
		auto type = [item availableTypeFromArray:typesArray];
		if (type) {
			auto data = [item dataForType:type];
			req->dataCallback(Status::Ok,
					BytesView(reinterpret_cast<const uint8_t *>(data.bytes), data.length),
					selectedType);
			return Status::Ok;
		}
	}

	req->dataCallback(Status::ErrorNotImplemented, BytesView(), StringView());
	return Status::ErrorNotImplemented;
}

Status MacosContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	auto pasteboard = [NSPasteboard generalPasteboard];

	[pasteboard prepareForNewContentsWithOptions:0];

	auto item = [[XLMacosPasteboardItem alloc] initWithData:sprt::move(data)];

	[pasteboard declareTypes:[item dataTypes] owner:item];
	[pasteboard writeObjects:@[item]];

	return Status::Ok;
}

bool MacosContextController::isCursorSupported(WindowCursor cursor, bool serverSide) const {
	switch (cursor) {
	case WindowCursor::Default:
	case WindowCursor::ContextMenu:
	case WindowCursor::Help:
	case WindowCursor::Pointer:
	case WindowCursor::Crosshair:
	case WindowCursor::Text:
	case WindowCursor::VerticalText:
	case WindowCursor::Alias:
	case WindowCursor::Copy:
	case WindowCursor::NoDrop:
	case WindowCursor::NotAllowed:
	case WindowCursor::Grab:
	case WindowCursor::Grabbing:
	case WindowCursor::ZoomIn:
	case WindowCursor::ZoomOut:

	case WindowCursor::ResizeRight:
	case WindowCursor::ResizeTop:
	case WindowCursor::ResizeBottom:
	case WindowCursor::ResizeLeft:
	case WindowCursor::ResizeLeftRight:
	case WindowCursor::ResizeTopBottom:
	case WindowCursor::ResizeCol:
	case WindowCursor::ResizeRow: return true; break;

	case WindowCursor::Progress:
	case WindowCursor::Wait:
	case WindowCursor::Cell:
	case WindowCursor::Move:

	case WindowCursor::AllScroll:
	case WindowCursor::DndAsk:

	case WindowCursor::RightPtr:
	case WindowCursor::Pencil:
	case WindowCursor::Target:

	case WindowCursor::ResizeTopRight:
	case WindowCursor::ResizeTopLeft:
	case WindowCursor::ResizeBottomRight:
	case WindowCursor::ResizeBottomLeft:
	case WindowCursor::ResizeTopRightBottomLeft:
	case WindowCursor::ResizeTopLeftBottomRight:
	case WindowCursor::ResizeAll:
	default: break;
	}
	return false;
}

WindowCapabilities MacosContextController::getCapabilities() const {
	WindowCapabilities caps = WindowCapabilities::FullscreenWithMode
			| WindowCapabilities::FullscreenSeamlessModeSwitch | WindowCapabilities::Fullscreen
			| WindowCapabilities::ServerSideDecorations | WindowCapabilities::UserSpaceDecorations
			| WindowCapabilities::CloseGuard | WindowCapabilities::AllowMoveFromMaximized;

	return caps;
}

void MacosContextController::openUrl(StringView url) {
	url.performWithTerminated([](const char *data, size_t) {
		NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:data]];
		if (url) {
			[[NSWorkspace sharedWorkspace] openURL:url];
		}
	});
}

SurfaceSupportInfo MacosContextController::getSupportInfo() const {
	SurfaceSupportInfo ret;
	ret.backendMask.set(toInt(SurfaceBackend::MacOS));
	ret.backendMask.set(toInt(SurfaceBackend::Metal));
	return ret;
}

} // namespace sprt::window


@implementation SPRTMacosAppDelegate

- (id _Nonnull)init:(NSSPWIN::MacosContextController *)controller {
	_controller = controller;
	_application = [NSApplication sharedApplication];

	if (!_application.delegate) {
		if (!NSSP::hasFlag(_controller->getContextInfo()->flags, NSSPWIN::ContextFlags::Headless)) {
			_application.activationPolicy = NSApplicationActivationPolicyRegular;
			_application.presentationOptions = NSApplicationPresentationDefault;
		}

		_application.delegate = self;
		_terminated = false;

		[NSThread detachNewThreadSelector:@selector(doNothing:) toTarget:self withObject:nil];
	}

	return self;
}

- (void)run {
	if (![[NSRunningApplication currentApplication] isFinishedLaunching]) {
		[NSApp run];
	}
}

- (void)terminate {
	_terminated = true;
	[NSApp terminate:self];
	[NSApp abortModal];
}

- (void)doNothing:(id _Nonnull)object {
	// do nothing
}

- (void)updateNetworkState:(nw_path_t)path {
	nw_path_status_t status = nw_path_get_status(path);

	NSSPWIN::NetworkFlags flags = NSSPWIN::NetworkFlags::None;

	if (status == nw_path_status_invalid) {
		_controller->handleNetworkStateChanged(flags);
	}

	if (nw_path_uses_interface_type(path, nw_interface_type_wired)) {
		flags |= NSSPWIN::NetworkFlags::Wired;
	} else if (nw_path_uses_interface_type(path, nw_interface_type_wifi)) {
		flags |= NSSPWIN::NetworkFlags::WLAN;
	} else if (nw_path_uses_interface_type(path, nw_interface_type_cellular)) {
		flags |= NSSPWIN::NetworkFlags::WWAN;
	}

	if (nw_path_is_expensive(path)) {
		flags |= NSSPWIN::NetworkFlags::Metered;
	}


	if (nw_path_is_constrained(path)) {
		flags |= NSSPWIN::NetworkFlags::Restricted;
	}

	switch (status) {
	case nw_path_status_invalid: break;
	case nw_path_status_unsatisfied: flags |= NSSPWIN::NetworkFlags::Suspended; break;
	case nw_path_status_satisfied:
		if ((nw_path_has_ipv4(path) || nw_path_has_ipv6(path)) && nw_path_has_dns(path)) {
			flags |= NSSPWIN::NetworkFlags::Internet;
		}
		flags |= NSSPWIN::NetworkFlags::Validated;
		break;
	case nw_path_status_satisfiable:
		if ((nw_path_has_ipv4(path) || nw_path_has_ipv6(path)) && nw_path_has_dns(path)) {
			flags |= NSSPWIN::NetworkFlags::Internet;
		}
		flags |= NSSPWIN::NetworkFlags::Suspended;
		break;
	}

	_controller->handleNetworkStateChanged(flags);
}

- (void)observeValueForKeyPath:(NSString *)keyPath
					  ofObject:(id)object
						change:(NSDictionary<NSString *, id> *)change
					   context:(void *)context {
	if (object == NSApp && [keyPath isEqualToString:@"effectiveAppearance"]) {
		NSSPWIN::String str = NSApp.effectiveAppearance.name.UTF8String;
		NSSP::StringView r(str);

		if (r.starts_with("NSAppearanceName")) {
			r += strlen("NSAppearanceName");
		}

		NSSPWIN::String color;
		if (r.starts_with("Dark")) {
			color = "prefer-dark";
		} else {
			color = "prefer-light";
		}

		_themeInfo.cursorSize = 24;
		_themeInfo.colorScheme = color;
		_themeInfo.systemTheme = r.str<NSSPWIN::String>();
		_themeInfo.doubleClickInterval = 1'000'000 * [NSEvent doubleClickInterval];

		_controller->handleThemeInfoChanged(NSSPWIN::ThemeInfo(_themeInfo));
	}
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	return NSTerminateNow;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationWillFinishLaunching");
	_controller->handleContextWillStart();
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationDidFinishLaunching");
	_controller->handleContextDidStart();

	_pathMonitor = nw_path_monitor_create();
	nw_path_monitor_set_queue(_pathMonitor, dispatch_get_main_queue());
	nw_path_monitor_set_update_handler(_pathMonitor,
			^(nw_path_t _Nonnull path) { [self updateNetworkState:path]; });
	nw_path_monitor_start(_pathMonitor);

	[NSApp addObserver:self
			forKeyPath:@"effectiveAppearance"
			   options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial
			   context:nullptr];
}

- (void)applicationWillHide:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationWillHide");
	_controller->handleContextWillStart();
}

- (void)applicationDidHide:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationDidHide");
}

- (void)applicationWillUnhide:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationWillUnhide");
}

- (void)applicationDidUnhide:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationDidUnhide");
}

- (void)applicationWillBecomeActive:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationWillBecomeActive");
	_controller->handleContextWillResume();
}
- (void)applicationDidBecomeActive:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationDidBecomeActive");
	_controller->handleContextDidResume();
}
- (void)applicationWillResignActive:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationWillResignActive");
	_controller->handleContextWillPause();
}

- (void)applicationDidResignActive:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationDidResignActive");
	_controller->handleContextDidPause();
}

- (void)applicationWillUpdate:(NSNotification *)notification {
}

- (void)applicationDidUpdate:(NSNotification *)notification {
}

- (void)applicationWillTerminate:(NSNotification *)notification {
	if (_pathMonitor) {
		nw_path_monitor_cancel(_pathMonitor);
		_pathMonitor = nullptr;
	}

	XL_MACOS_LOG("XLMacosAppDelegate", "applicationWillTerminate");
	_controller->applicationWillTerminate(_terminated);
	self->_controller = nullptr;
}

- (void)applicationDidChangeScreenParameters:(NSNotification *)notification {
	XL_MACOS_LOG("XLMacosAppDelegate", "applicationDidChangeScreenParameters");
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return NO;
}

@end

@implementation XLMacosPasteboardItem

- (instancetype)initWithData:(NSSP::Rc< NSSPWIN::ClipboardData> &&)data {
	self = [super init];

	_data = NSSP::move(data);

	auto types = [[NSMutableArray alloc] initWithCapacity:_data->types.size()];

	for (auto &it : _data->types) {
		auto sourceType = it;
		if (NSSP::StringView(it) == "text/plain") {
			sourceType = "text/plain;charset=utf-8";
		}
		auto type =
				[UTType typeWithMIMEType:[[NSString alloc] initWithUTF8String:sourceType.data()]];
		if (type) {
			auto id = type.identifier;
			[types addObject:id];
			_types.emplace(id, it);
		}
	}

	_nstypes = types;

	[self setDataProvider:self forTypes:_nstypes];

	return self;
}

- (NSArray<NSString *> *)dataTypes {
	return _nstypes;
}

- (void)pasteboard:(NSPasteboard *)pasteboard
					  item:(NSPasteboardItem *)item
		provideDataForType:(NSPasteboardType)type {
	if (!_data) {
		return;
	}

	auto tIt = _types.find(type);
	if (tIt == _types.end()) {
		return;
	}

	auto data = _data->encodeCallback(tIt->second);

	XL_MACOS_LOG("XLMacosPasteboardItem", "Write clipboard: ", tIt->second, " ", data.size());

	[pasteboard setData:[[NSData alloc] initWithBytes:data.data() length:data.size()] forType:type];
}

- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard {
	XL_MACOS_LOG("XLMacosPasteboardItem", "clear clipboard");
	_data = nullptr;
}

@end
