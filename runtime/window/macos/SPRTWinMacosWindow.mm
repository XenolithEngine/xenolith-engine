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

// These sources pull in AppKit / CoreGraphics / IOKit, which only exist on macOS.
// Guard the whole unit on SPRT_MACOS so it compiles to nothing on iOS (the window
// module is built for all Apple targets). __sprt_def.h is included first so that
// SPRT_MACOS is defined before the guard is evaluated.
#include <sprt/c/bits/__sprt_def.h>

#if SPRT_MACOS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wavailability"

#import <AppKit/AppKit.h>
//#import <MetalKit/MetalKit.h>

#pragma clang diagnostic pop

#include "SPRTWinMacosContextController.h"
#include "SPRTWinMacosDisplayConfigManager.h"
#include "SPRTWinMacosWindow.h"
#include "SPRTWinMacosView.h"

#include <sprt/runtime/log.h>

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkSwapchain.h"
#endif

// `zoomInCursor` / `zoomOutCursor` were introduced in the macOS 15.0 SDK. When
// building against an older SDK they are undeclared, so forward-declare them to
// keep the call sites typed; availability is still verified at runtime through
// `-[NSObject respondsToSelector:]` before the methods are invoked. On the 15.0+
// SDK the real declarations are used and this category is skipped.
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 150'000
@interface NSCursor (SPRTLegacyCursors)
+ (NSCursor *)zoomInCursor;
+ (NSCursor *)zoomOutCursor;
+ (NSCursor *)columnResizeCursor;
+ (NSCursor *)rowResizeCursor;
@end
#endif

#if XL_MACOS_DEBUG
#define XL_MACOS_LOG(...) NSSP::log::source().debug(__VA_ARGS__)
#else
#define XL_MACOS_LOG(...)
#endif

@interface SPRTMacosViewController : NSViewController <NSWindowDelegate> {
	CADisplayLink *_displayLink;
	CGPoint _currentPointerLocation;
	NSSPWIN::InputModifier _currentModifiers;
	NSSPWIN::InputKeyCode _keycodes[256];
	uint16_t _scancodes[NSSP::toInt(NSSPWIN::InputKeyCode::Max)];
	NSSPWIN::MacosWindow *_engineWindow;
	NSSPWIN::WindowLayerFlags _buttonGripFlags;
	sprt::bitset<64> _buttons;
	CGSize _lastNotifiedDrawableSize;
	CFAbsoluteTime _lastConstraintsNotifyTime;
};

@property(nonatomic, assign) BOOL displayLinkPaused;

- (instancetype _Nonnull)init:(NSSP::NotNull<NSSPWIN::MacosWindow>)constroller
					   window:(NSWindow *_Nonnull)window;

- (void)invalidate;

- (SPRTMacosView *)targetView;

- (void)setEngineLiveResize:(BOOL)value;

@end

@interface SPRTMacosWindow : NSWindow {
	NSWindowStyleMask _defaultStyle;
	BOOL _allowKey;
	BOOL _allowMain;
}

- (void)configureRole:(BOOL)allowKey allowMain:(BOOL)allowMain;

- (void)setFrame:(NSRect)frameRect
				  display:(BOOL)displayFlag
				 duration:(NSTimeInterval)duration
		completionHandler:(nullable void (^)(void))completionHandler;

- (void)toggleFullScreen:(id)sender withScreen:(NSScreen *)screen;

- (NSWindowStyleMask)defaultStyle;

@end

namespace sprt::window {

MacosWindow::~MacosWindow() {
	removePopupDismissMonitor();
	detachFromParentWindow();
	_startupHold = nullptr;
	_rootViewController = nullptr;
	if (_window) {
		_window = nullptr;
	}
}

bool MacosWindow::init(NotNull<ContextController> controller, Rc<WindowInfo> &&info) {
	if (!NativeWindow::init(controller, sprt::move(info), controller->getCapabilities())) {
		return false;
	}

	const bool auxiliary = isAuxiliary();
	NSWindowStyleMask style = 0;
	if (auxiliary) {
		// Borderless surface: no title bar, no fullscreen chrome.
		style = NSWindowStyleMaskBorderless;
	} else if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		style = NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
				| NSWindowStyleMaskClosable;

		updateState(0,
				WindowState::AllowedClose | WindowState::AllowedMinimize
						| WindowState::AllowedMaximizeHorz | WindowState::AllowedMaximizeVert
						| WindowState::AllowedMove | WindowState::AllowedFullscreen);
	} else {
		style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
				| NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
	}

	auto rect = NSRect{
		{static_cast<CGFloat>(_info->rect.x), static_cast<CGFloat>(_info->rect.y)},
		{static_cast<CGFloat>(_info->rect.width), static_cast<CGFloat>(_info->rect.height)},
	};

	// A borderless NSWindow covers the auxiliary roles; no separate NSPanel subclass is needed
	// for the Metal content-view path. Auxiliary windows must not defer creation: a MoltenVK
	// present into a never-shown deferred window returns DEVICE_LOST on the shared VkDevice.
	_window = [[SPRTMacosWindow alloc] initWithContentRect:rect
												 styleMask:style
												   backing:NSBackingStoreBuffered
													 defer:auxiliary ? NO : YES];

	if (auxiliary) {
		[_window setOpaque:YES];
		[_window setBackgroundColor:[NSColor colorWithCalibratedRed:0x2A / 255.0
															  green:0x2A / 255.0
															   blue:0x2E / 255.0
															  alpha:1.0]];
	} else if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		[_window setOpaque:NO];
		[_window setBackgroundColor:[NSColor clearColor]];
	}

	if (auxiliary) {
		[_window setLevel:NSPopUpMenuWindowLevel];
		[_window setHasShadow:YES];
		[_window setHidesOnDeactivate:NO];
		// Never become key/main — Root keeps focus. Esc is handled by a local key monitor.
		[_window configureRole:NO allowMain:NO];
		// Never a fullscreen primary space citizen.
		_window.collectionBehavior = NSWindowCollectionBehaviorMoveToActiveSpace
				| NSWindowCollectionBehaviorTransient
				| NSWindowCollectionBehaviorFullScreenAuxiliary;
	} else {
		[_window configureRole:YES allowMain:YES];
		_window.collectionBehavior |=
				_window.collectionBehavior | NSWindowCollectionBehaviorFullScreenPrimary;
	}

	// Apply immutable min/max content constraints. `_info->rect` is content-space in points,
	// so the constraints map directly onto contentMin/MaxSize (Cocoa enforces them on resize).
	// A 0 dimension means "unconstrained": 0 for the minimum, CGFLOAT_MAX for the maximum.
	if (!auxiliary) {
		if (_info->minExtent != Extent2::ZERO) {
			_window.contentMinSize = NSMakeSize(static_cast<CGFloat>(_info->minExtent.width),
					static_cast<CGFloat>(_info->minExtent.height));
		}
		if (_info->maxExtent != Extent2::ZERO) {
			_window.contentMaxSize = NSMakeSize(_info->maxExtent.width != 0
							? static_cast<CGFloat>(_info->maxExtent.width)
							: CGFLOAT_MAX,
					_info->maxExtent.height != 0 ? static_cast<CGFloat>(_info->maxExtent.height)
												 : CGFLOAT_MAX);
		}
	}

	[_window setReleasedWhenClosed:false];
	_rootViewController = [[SPRTMacosViewController alloc] init:this window:_window];
	_window.contentViewController = _rootViewController;
	_window.contentView = _window.contentViewController.view;
	_window.title = [NSString stringWithCString:_info->title.data() encoding:NSUTF8StringEncoding];

	_initialized = true;

	// Hold a self-ref until notifyWindowCreated retains us: otherwise loadWindow's temporary Rc
	// dies and takes the window with it before the view finishes loading.
	_startupHold = this;

	if (_windowLoaded) {
		handleWindowLoaded();
	}
	return true;
}

void MacosWindow::attachToParentWindow() {
	if (!isAuxiliary() || _info->parent.empty()) {
		return;
	}
	auto *parent = dynamic_cast<MacosWindow *>(_controller->findWindow(_info->parent));
	if (!parent || !parent->getWindow() || !_window) {
		return;
	}
	NSWindow *parentWin = parent->getWindow();
	if ([parentWin.childWindows indexOfObjectIdenticalTo:_window] == NSNotFound) {
		[parentWin addChildWindow:_window ordered:NSWindowAbove];
	}
}

void MacosWindow::detachFromParentWindow() {
	if (!_window) {
		return;
	}
	NSWindow *parent = _window.parentWindow;
	if (parent) {
		[parent removeChildWindow:_window];
	}
}

void MacosWindow::applyAuxiliaryPlacement() {
	if (!isAuxiliary() || !_window) {
		return;
	}
	auto *parent = dynamic_cast<MacosWindow *>(_controller->findWindow(_info->parent));
	if (!parent || !parent->getWindow()) {
		return;
	}

	NSWindow *parentWin = parent->getWindow();
	NSRect parentFrame = parentWin.frame;
	// Engine placement space is Y-down from the parent content top-left.
	NSRect parentContent = [parentWin contentRectForFrameRect:parentFrame];
	IRect parentContentRect(0, 0, int32_t(parentContent.size.width),
			int32_t(parentContent.size.height));

	NSScreen *screen = parentWin.screen ?: [NSScreen mainScreen];
	NSRect visible = screen.visibleFrame;
	// Work area in parent-content Y-down space: convert screen visible rect.
	const CGFloat parentTop = parentContent.origin.y + parentContent.size.height;
	IRect workArea(int32_t(visible.origin.x - parentContent.origin.x),
			int32_t(parentTop - (visible.origin.y + visible.size.height)),
			int32_t(visible.size.width), int32_t(visible.size.height));

	Extent2 size(_info->rect.width, _info->rect.height);
	IRect placed = computeWindowPlacement(_info->placement, size, parentContentRect, workArea);

	// Convert Y-down parent-content coords back to Cocoa screen (Y-up).
	const CGFloat cocoaX = parentContent.origin.x + CGFloat(placed.x);
	const CGFloat cocoaY =
			parentTop - CGFloat(placed.y) - CGFloat(placed.height);
	NSRect frame = NSMakeRect(cocoaX, cocoaY, CGFloat(placed.width), CGFloat(placed.height));
	[_window setFrame:[_window frameRectForContentRect:frame] display:NO];

	_info->rect = IRect(placed.x, placed.y, placed.width, placed.height);
}

void MacosWindow::mapWindow() {
	// A late first-frame callback must not resurrect a window that is already winding down.
	if (_mapped || _dismissScheduled) {
		return;
	}

	if (isAuxiliary()) {
		applyAuxiliaryPlacement();
		attachToParentWindow();
		// Never steal key focus from Root — that greys its title bar and churns Context
		// pause/resume. Esc arrives through the local key monitor instead.
		[_window orderFront:nil];
		if (_info->type == WindowType::Popup || _info->type == WindowType::Tooltip) {
			// Arm outside-click / Esc dismiss after a short delay so the opening click cannot
			// also dismiss in the same gesture.
			MacosWindow *self = this;
			dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)),
					dispatch_get_main_queue(), ^{
				if (self && self->_mapped && self->_window && !self->_dismissScheduled) {
					self->installPopupDismissMonitor();
				}
			});
		}
		_mapped = true;
		if (_rootViewController) {
			_rootViewController.displayLinkPaused = NO;
		}
		return;
	}

	// Root (and future Dialog/Utility): show in front and activate.
	[_window makeKeyAndOrderFront:nil];
	[_window orderFrontRegardless];
	[_window orderWindow:NSWindowAbove relativeTo:0];
	[NSApp activateIgnoringOtherApps:YES];
	_mapped = true;

	if (_rootViewController) {
		_rootViewController.displayLinkPaused = NO;
	}
}

void MacosWindow::unmapWindow() {
	removePopupDismissMonitor();
	detachFromParentWindow();
	[_rootViewController invalidate];
	_rootViewController = nullptr;
}

// Whole subtree, not just direct children: a click in a third-level submenu is still "inside"
// for the top-level menu. A plain function, not a self-referencing block — that would retain
// itself and leak on every event.
static BOOL pointInsideWindowTree(NSWindow *root, NSPoint pt) {
	if (!root) {
		return NO;
	}
	if (NSPointInRect(pt, root.frame)) {
		return YES;
	}
	for (NSWindow *child in root.childWindows) {
		if (pointInsideWindowTree(child, pt)) {
			return YES;
		}
	}
	return NO;
}

void MacosWindow::installPopupDismissMonitor() {
	if (_popupDismissMonitor || !isAuxiliary() || !_window) {
		return;
	}
	// A tooltip has no dismiss affordance, so any click takes it away — and it must never swallow
	// that click. For a popup, outside clicks dismiss and are swallowed so the button that opened
	// it cannot reopen in the same gesture. The popup is never key, so this monitor is also the
	// only path Esc can arrive by.
	const bool isTooltip = _info->type == WindowType::Tooltip;
	MacosWindow *self = this;
	auto scheduleDismiss = ^(const char * /*reason*/) {
		if (!self || self->_dismissScheduled) {
			return;
		}
		// Tooltip and Popup share hide()/EndOfLife dismiss.
		self->_dismissScheduled = true;
		self->removePopupDismissMonitor();
		Rc<ContextController> ctrl = self->_controller;
		ctrl->getLooper()->performOnThread([self] {
			if (!self) {
				return;
			}
			if (auto *aw = self->getAppWindow()) {
				aw->hide();
			} else {
				self->close();
			}
		}, ctrl);
	};
	_popupDismissMonitor = [NSEvent
			addLocalMonitorForEventsMatchingMask:(NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown
														 | NSEventMaskKeyDown)
										 handler:^NSEvent *(NSEvent *event) {
		if (!self || !self->_window || self->_dismissScheduled) {
			return event;
		}
		if (event.type == NSEventTypeKeyDown) {
			if (event.keyCode == 53 /* kVK_Escape */) {
				scheduleDismiss("escape");
				return nil;
			}
			return event;
		}
		if (isTooltip) {
			scheduleDismiss("click-any");
			return event;
		}
		const NSPoint screenPt = [NSEvent mouseLocation];
		auto pointInsideTree = [&](NSWindow *root) { return pointInsideWindowTree(root, screenPt); };
		if (pointInsideTree(self->_window)) {
			return event;
		}
		// Climb to the root-level Popup (parent is Root). Nested + root monitors must not
		// both hide on an outside click — that double-parks Metal and races DeviceLost.
		MacosWindow *treeRoot = self;
		while (treeRoot && treeRoot->_info) {
			auto parentId = treeRoot->_info->parent;
			if (parentId.empty() || !self->_controller) {
				break;
			}
			auto *pw = self->_controller->findWindow(parentId);
			auto *pi = pw ? pw->getInfo() : nullptr;
			if (!pi || pi->type != WindowType::Popup) {
				break;
			}
			treeRoot = static_cast<MacosWindow *>(pw);
		}
		if (treeRoot != self) {
			// Outside submenu: dismiss only when the click is still inside the parent menu tree.
			// Clicks outside the whole tree are handled solely by the root popup's monitor.
			if (treeRoot && pointInsideTree(treeRoot->_window)) {
				scheduleDismiss("click-in-parent-menu");
			}
			return event;
		}
		scheduleDismiss("click-outside-tree");
		// Swallow so parent widgets (Open Popup) do not fire on the dismiss click.
		return nil;
	}];
}

void MacosWindow::removePopupDismissMonitor() {
	if (_popupDismissMonitor) {
		[NSEvent removeMonitor:_popupDismissMonitor];
		_popupDismissMonitor = nil;
	}
}

void MacosWindow::prepareClose() {
	removePopupDismissMonitor();
	_dismissScheduled = true;
	_mapped = false;
	if (_rootViewController) {
		_rootViewController.displayLinkPaused = YES;
	}
}

bool MacosWindow::setContentExtent(Extent2 extent) {
	if (!_window || extent.width == 0 || extent.height == 0) {
		return false;
	}
	_info->rect.width = int32_t(extent.width);
	_info->rect.height = int32_t(extent.height);

	NSRect content = [_window contentRectForFrameRect:_window.frame];
	const CGFloat top = content.origin.y + content.size.height;
	NSRect next = NSMakeRect(content.origin.x, top - CGFloat(extent.height),
			CGFloat(extent.width), CGFloat(extent.height));
	[_window setFrame:[_window frameRectForContentRect:next] display:YES];
	_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
	return true;
}

bool MacosWindow::close() {
	prepareClose();

	// Use None (not CloseInPlace): let AppKit drive windowWillClose → CloseInPlace so the
	// presentation engine can wind down instead of being ripped out mid-acquire.
	if (!_controller->notifyWindowClosed(this, WindowCloseOptions::None)) {
		if (hasFlag(_info->state, WindowState::CloseGuard)) {
			updateState(0, _info->state | WindowState::CloseRequest);
		}
		return false;
	}

	detachFromParentWindow();
	if (_window) {
		[_window close];
	}
	return true;
}

void MacosWindow::handleFramePresented(const PresentationFrameInfo &) { }

SurfaceInterfaceInfo MacosWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo info;
	info.backend = SurfaceBackend::Metal;
	info.metal.layer = (__bridge void *)_rootViewController.targetView.layer;
	return info;
}

FrameConstraints MacosWindow::exportConstraints(uint64_t &serial) const {
	FrameConstraints constraints = NativeWindow::exportConstraints(serial);

	if (constraints.density == 0.0f) {
		constraints.density = 1.0f;
	}

	constraints.density *= _window.backingScaleFactor;
	constraints.surfaceDensity = _window.backingScaleFactor;

	return constraints;
}

Extent2 MacosWindow::getExtent() const {
	CGSize size;
	if (_rootViewController && _rootViewController.targetView) {
		size = _rootViewController.targetView.layer.frame.size;
	} else {
		size = _window.frame.size;
	}

	size.width *= _window.backingScaleFactor;
	size.height *= _window.backingScaleFactor;
	return Extent2(static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height));
}

PresentationOptions MacosWindow::getPreferredOptions() const {
	PresentationOptions opts;
	opts.followDisplayLinkBarrier = true;
	return opts;
}

void MacosWindow::handleWindowLoaded() {
	_windowLoaded = true;
	if (!_initialized) {
		return;
	}

	if (!_createdNotified) {
		_createdNotified = true;
		_controller->notifyWindowCreated(this);
		_startupHold = nullptr;
		[_window display];
	}

	// Do not makeKeyAndOrderFront here — mapWindow owns first show, and for auxiliary
	// windows an early key steal from Root is exactly the hang/focus bug we are fixing.
}

void MacosWindow::handleDisplayLink() {
	if (_appWindow) {
		_appWindow->update(PresentationUpdateFlags::DisplayLink);
	}
}

void MacosWindow::handleFullscreenTransitionComplete(MacosFullscreenRequest req) {
	if (_hasPendingFullscreenOp) {
		if (_fullscreenRequest == req) {
			_hasPendingFullscreenOp = false;
			_nextScreen = nullptr;
		} else if (req == MacosFullscreenRequest::ExitFullscreen
				&& _fullscreenRequest == MacosFullscreenRequest::ToggleFullscreen) {
			_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
		}
	}
}

bool MacosWindow::enableState(WindowState state) {
	if (NativeWindow::enableState(state)) {
		return true;
	}

	switch (state) {
	case WindowState::Maximized:
		[_window zoom:nullptr];
		return true;
		break;
	case WindowState::Minimized:
		[_window miniaturize:nullptr];
		return true;
		break;
	default: break;
	}

	return false;
}

bool MacosWindow::disableState(WindowState state) {
	if (NativeWindow::disableState(state)) {
		return true;
	}

	switch (state) {
	case WindowState::Maximized:
		[_window zoom:nullptr];
		return true;
		break;
	case WindowState::Minimized:
		[_window deminiaturize:nullptr];
		return true;
		break;
	default: break;
	}

	return false;
}

Status MacosWindow::setFullscreenState(FullscreenInfo &&info) {
	if (_hasPendingFullscreenOp) {
		return Status::ErrorAgain;
	}

	auto enable = info != FullscreenInfo::None;
	if (!enable) {
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			_hasPendingFullscreenOp = true;
			_fullscreenRequest = MacosFullscreenRequest::ExitFullscreen;
			[_window toggleFullScreen:nil];
			_info->fullscreen = move(info);
			_hasOriginalFrame = false;
			return Status::Ok;
		} else {
			return Status::Declined;
		}
	} else {
		auto frame = _window.frame;

		if (!hasFlag(_info->state, WindowState::Fullscreen)) {
			_originalFrame = frame;
			_hasOriginalFrame = true;
		}

		if (hasFlag(info.flags, FullscreenFlags::Current)) {
			if (!hasFlag(_info->state, WindowState::Fullscreen)) {
				auto current = _controller->getDisplayConfigManager()->getCurrentConfig();

				auto screen = _window.screen;
				NSNumber *screenNumber = [screen deviceDescription][@"NSScreenNumber"];

				for (auto &it : current->monitors) {
					if (CGDisplayUnitNumber([screenNumber unsignedIntValue]) == it.index) {
						info.id = it.id;
						info.mode = it.getCurrent().mode;
						_hasPendingFullscreenOp = true;
						_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
						[_window toggleFullScreen:nil];
						_info->fullscreen = move(info);
						return Status::Ok;
						break;
					}
				}
				return Status::Ok;
			}
			return Status::Declined;
		}

		auto current = _controller->getDisplayConfigManager()->getCurrentConfig();
		auto mon = current->getMonitor(info.id);
		if (!mon) {
			return Status::ErrorInvalidArguemnt;
		}

		NSScreen *screen = nil;
		for (screen in [NSScreen screens]) {
			NSNumber *screenNumber = [screen deviceDescription][@"NSScreenNumber"];
			if (CGDisplayUnitNumber([screenNumber unsignedIntValue]) == mon->index) {
				break;
			}
		}

		if (!screen) {
			return Status::ErrorInvalidArguemnt;
		}

		info.id = mon->id;
		info.mode = mon->getCurrent().mode;

		// find target output
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			if (screen != _window.screen) {
				_hasPendingFullscreenOp = true;
				_fullscreenRequest = MacosFullscreenRequest::ToggleFullscreen;
				_nextScreen = screen;
				[_window toggleFullScreen:nil];
				_info->fullscreen = move(info);
				return Status::Ok;
			}
			return Status::Declined;
		}

		if (screen == _window.screen) {
			_hasPendingFullscreenOp = true;
			_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
			[_window toggleFullScreen:nil];
			_info->fullscreen = move(info);
			return Status::Ok;
		}

		_hasPendingFullscreenOp = true;
		_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
		[_window toggleFullScreen:nil withScreen:screen];
		_info->fullscreen = move(info);
		return Status::Ok;
	}
}

bool MacosWindow::updateTextInput(const TextInputRequest &req, TextInputFlags flags) {
	return false;
}

void MacosWindow::cancelTextInput() { }

void MacosWindow::setCursor(WindowCursor cursor) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
	if (cursor != _currentCursor) {
		NSCursor *targetCursor = nullptr;
		switch (cursor) {
		case NSSPWIN::WindowCursor::Default: targetCursor = [NSCursor arrowCursor]; break;
		case NSSPWIN::WindowCursor::ContextMenu:
			targetCursor = [NSCursor contextualMenuCursor];
			break;
		case NSSPWIN::WindowCursor::Help: targetCursor = [NSCursor arrowCursor]; break;
		case NSSPWIN::WindowCursor::Pointer: targetCursor = [NSCursor pointingHandCursor]; break;
		case NSSPWIN::WindowCursor::Progress: break;
		case NSSPWIN::WindowCursor::Wait: break;
		case NSSPWIN::WindowCursor::Cell: break;
		case NSSPWIN::WindowCursor::Crosshair: targetCursor = [NSCursor crosshairCursor]; break;
		case NSSPWIN::WindowCursor::Text: targetCursor = [NSCursor IBeamCursor]; break;
		case NSSPWIN::WindowCursor::VerticalText:
			targetCursor = [NSCursor IBeamCursorForVerticalLayout];
			break;
		case NSSPWIN::WindowCursor::Alias: targetCursor = [NSCursor dragLinkCursor]; break;
		case NSSPWIN::WindowCursor::Copy: targetCursor = [NSCursor dragCopyCursor]; break;
		case NSSPWIN::WindowCursor::Move: break;
		case NSSPWIN::WindowCursor::NoDrop:
			targetCursor = [NSCursor operationNotAllowedCursor];
			break;
		case NSSPWIN::WindowCursor::NotAllowed:
			targetCursor = [NSCursor operationNotAllowedCursor];
			break;
		case NSSPWIN::WindowCursor::Grab: targetCursor = [NSCursor openHandCursor]; break;
		case NSSPWIN::WindowCursor::Grabbing: targetCursor = [NSCursor closedHandCursor]; break;

		case NSSPWIN::WindowCursor::AllScroll: break;
		case NSSPWIN::WindowCursor::ZoomIn:
			// +[NSCursor zoomInCursor] is only available on macOS 15.0+; probe
			// for it at runtime so older systems fall back to the default cursor.
			if ([NSCursor respondsToSelector:@selector(zoomInCursor)]) {
				targetCursor = [NSCursor zoomInCursor];
			}
			break;
		case NSSPWIN::WindowCursor::ZoomOut:
			if ([NSCursor respondsToSelector:@selector(zoomOutCursor)]) {
				targetCursor = [NSCursor zoomOutCursor];
			}
			break;
		case NSSPWIN::WindowCursor::DndAsk: break;

		case NSSPWIN::WindowCursor::RightPtr: break;
		case NSSPWIN::WindowCursor::Pencil: break;
		case NSSPWIN::WindowCursor::Target: break;

		case NSSPWIN::WindowCursor::ResizeRight: targetCursor = [NSCursor resizeRightCursor]; break;
		case NSSPWIN::WindowCursor::ResizeTop: targetCursor = [NSCursor resizeUpCursor]; break;
		case NSSPWIN::WindowCursor::ResizeTopRight: break;
		case NSSPWIN::WindowCursor::ResizeTopLeft: break;
		case NSSPWIN::WindowCursor::ResizeBottom: targetCursor = [NSCursor resizeDownCursor]; break;
		case NSSPWIN::WindowCursor::ResizeBottomRight: break;
		case NSSPWIN::WindowCursor::ResizeBottomLeft: break;
		case NSSPWIN::WindowCursor::ResizeLeft: targetCursor = [NSCursor resizeLeftCursor]; break;
		case NSSPWIN::WindowCursor::ResizeLeftRight:
			targetCursor = [NSCursor resizeLeftRightCursor];
			break;
		case NSSPWIN::WindowCursor::ResizeTopBottom:
			targetCursor = [NSCursor resizeUpDownCursor];
			break;
		case NSSPWIN::WindowCursor::ResizeTopRightBottomLeft: break;
		case NSSPWIN::WindowCursor::ResizeTopLeftBottomRight: break;
		case NSSPWIN::WindowCursor::ResizeCol:
			if ([NSCursor respondsToSelector:@selector(columnResizeCursor)]) {
				targetCursor = [NSCursor columnResizeCursor];
			}
			break;
		case NSSPWIN::WindowCursor::ResizeRow:
			if ([NSCursor respondsToSelector:@selector(rowResizeCursor)]) {
				targetCursor = [NSCursor rowResizeCursor];
			}
			break;
		case NSSPWIN::WindowCursor::ResizeAll: break;
		default: break;
		}

		if (targetCursor) {
			[targetCursor set];
			_currentCursor = cursor;
		} else {
			if (_currentCursor != WindowCursor::Undefined) {
				[[NSCursor arrowCursor] set];
				_currentCursor = WindowCursor::Undefined;
			}
		}
	}
#pragma clang diagnostic pop
}

} // namespace sprt::window

@implementation SPRTMacosViewController

- (void)handleDisplayLink:(CADisplayLink *)obj {
	if (_engineWindow) {
		_engineWindow->handleDisplayLink();
	}
	[self.view setNeedsDisplay:YES];
	[self.targetView setNeedsDisplay:YES];
}

- (instancetype _Nonnull)init:(NSSP::NotNull<NSSPWIN::MacosWindow>)w
					   window:(NSWindow *_Nonnull)window {
	self = [super init];
	_engineWindow = w;

	_currentPointerLocation = CGPoint{0, 0};
	_currentModifiers = NSSPWIN::InputModifier::None;

	_buttonGripFlags = NSSPWIN::WindowLayerFlags::None;
	_buttons.reset();
	// Note: a literal, not CGSizeZero — that is an imported data symbol from
	// CoreGraphics, which the SDK-free "+open" link stubs do not export
	_lastNotifiedDrawableSize = CGSize{0, 0};
	_lastConstraintsNotifyTime = 0;

	window.delegate = self;

	_displayLink = [window displayLinkWithTarget:self selector:@selector(handleDisplayLink:)];
	[_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
	_displayLink.paused = YES;

	NSSPWIN::createKeyTables(_keycodes, _scancodes);

	return self;
}

- (void)invalidate {
	_engineWindow = nullptr;
}

- (BOOL)displayLinkPaused {
	return _displayLink.paused;
}

- (void)setDisplayLinkPaused:(BOOL)value {
	_displayLink.paused = value;
}

- (SPRTMacosView *)targetView {
	return (SPRTMacosView *)self.view;
}

- (void)setEngineLiveResize:(BOOL)value {
	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			value ? NSSPWIN::UpdateConstraintsFlags::EnableLiveResize
				  : NSSPWIN::UpdateConstraintsFlags::DisableLiveResize);
}

- (void)viewDidLoad {
	[super viewDidLoad];

	_engineWindow->handleWindowLoaded();

	[_engineWindow->getWindow() setHidesOnDeactivate:NO];
	[_engineWindow->getWindow() setCanHide:NO];
	_engineWindow->getWindow().displaysWhenScreenProfileChanges = YES;
}

- (void)viewDidAppear {
	if (!_engineWindow) {
		return;
	}

	[super viewDidAppear];
	_displayLink.paused = NO;
	_engineWindow->updateState(0,
			_engineWindow->getInfo()->state & ~NSSPWIN::WindowState::Background);
	// Barrier mode may have raised _waitForDisplayLink before the link was running.
	_engineWindow->emitAppFrame();
}

- (void)viewWillDisappear {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0,
			_engineWindow->getInfo()->state | NSSPWIN::WindowState::Background);
	_displayLink.paused = YES;
	[super viewWillDisappear];
}

- (void)viewDidDisappear {
	[super viewWillDisappear];
}

- (void)loadView {
	auto extent = _engineWindow->getWindow().contentLayoutRect.size;

	auto view = [[SPRTMacosView alloc] initWithFrame:NSRect{{0.0f, 0.0f}, extent}
											  window:_engineWindow];

	view.wantsLayer = YES;
	view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

	CAMetalLayer *metalLayer = (CAMetalLayer *)view.layer;
	metalLayer.drawableSize = [view convertSizeToBacking:view.frame.size];

	if (NSSP::hasFlag(_engineWindow->getInfo()->flags,
				NSSPWIN::WindowCreationFlags::UserSpaceDecorations)) {
		metalLayer.cornerRadius = 10.0;
		metalLayer.masksToBounds = YES;
		// Opaque fill under the drawable: with a clear window the rounded-rect AA fringe
		// composites against the desktop and reads as a dark hairline along the top edge.
		metalLayer.backgroundColor = [NSColor colorWithCalibratedRed:0.059 green:0.067 blue:0.055
																alpha:1.0]
											 .CGColor;
		// thin light hairline hugging the rounded edge, like native macOS windows
		metalLayer.borderWidth = 0.5;
		metalLayer.borderColor = [NSColor colorWithWhite:0.85 alpha:0.7].CGColor;
		[_engineWindow->getWindow() setHasShadow:YES];
	}

	self.view = view;
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
	return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification {
	if (!_engineWindow) {
		[self.targetView setFrame:self.view.window.contentLayoutRect];
		return;
	}

	XL_MACOS_LOG("MacosWindow", "windowDidResize: ", self.view.window.contentLayoutRect.size.width,
			" ", self.view.window.contentLayoutRect.size.height);

	CAMetalLayer *metalLayer = (CAMetalLayer *)self.view.layer;
	CGSize drawableSize = [self.view convertSizeToBacking:self.view.frame.size];
	metalLayer.drawableSize = drawableSize;

	const bool live = self.view.inLiveResize;
	const CGFloat dx = fabs(drawableSize.width - _lastNotifiedDrawableSize.width);
	const CGFloat dy = fabs(drawableSize.height - _lastNotifiedDrawableSize.height);
	const bool sizeChanged = dx >= 1.0 || dy >= 1.0;

	// During live resize, MoltenVK cannot keep up with a swapchain recreate on every
	// AppKit callback — frames get invalidated, FPS collapses, the UI goes white.
	// EnableLiveResize is armed once in windowWillStartLiveResize; here we only
	// deprecate when the drawable actually moved, and at most ~20 Hz while dragging.
	bool shouldNotify = sizeChanged;
	if (live && sizeChanged) {
		const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
		constexpr CFAbsoluteTime kMinInterval = 0.05; // 20 Hz
		constexpr CGFloat kMinDelta = 8.0; // backing pixels
		if ((dx < kMinDelta && dy < kMinDelta)
				&& (now - _lastConstraintsNotifyTime) < kMinInterval) {
			shouldNotify = false;
		}
	}

	if (shouldNotify) {
		_lastNotifiedDrawableSize = drawableSize;
		_lastConstraintsNotifyTime = CFAbsoluteTimeGetCurrent();
		_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
				NSSPWIN::UpdateConstraintsFlags::WindowResized);
	}
	_engineWindow->emitAppFrame();

	auto isZoomed = _engineWindow->getWindow().zoomed;
	if (isZoomed) {
		_engineWindow->updateState(0,
				_engineWindow->getInfo()->state | NSSPWIN::WindowState::Maximized);
	} else {
		_engineWindow->updateState(0,
				_engineWindow->getInfo()->state & ~NSSPWIN::WindowState::Maximized);
	}
}

- (void)windowWillMove:(NSNotification *)notification {
}

- (void)windowDidMove:(NSNotification *)notification {
	//_engineWindow->emitAppFrame();
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0, _engineWindow->getInfo()->state | NSSPWIN::WindowState::Resizing);
	// Switch to Immediate present mode once for the drag; per-frame recreates happen
	// via WindowResized in windowDidResize (throttled).
	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			NSSPWIN::UpdateConstraintsFlags::EnableLiveResize);
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0,
			_engineWindow->getInfo()->state & ~NSSPWIN::WindowState::Resizing);

	CAMetalLayer *metalLayer = (CAMetalLayer *)self.view.layer;
	_lastNotifiedDrawableSize = metalLayer.drawableSize;
	_lastConstraintsNotifyTime = CFAbsoluteTimeGetCurrent();

	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			NSSPWIN::UpdateConstraintsFlags::DisableLiveResize);
	_engineWindow->emitAppFrame();
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
	if (!_engineWindow) {
		return YES;
	}

	if (!_engineWindow->getController()->notifyWindowClosed(_engineWindow,
				NSSPWIN::WindowCloseOptions::None)) {
		if (NSSP::hasFlag(_engineWindow->getInfo()->state, NSSPWIN::WindowState::CloseGuard)) {
			_engineWindow->updateState(0,
					_engineWindow->getInfo()->state | NSSPWIN::WindowState::CloseRequest);
		}
		return NO;
	}
	return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	XL_MACOS_LOG("SPRTMacosViewController", "windowWillClose");
	_engineWindow->getWindow().delegate = nullptr;
	_engineWindow->getController()->notifyWindowClosed(_engineWindow,
			NSSPWIN::WindowCloseOptions::CloseInPlace
					| NSSPWIN::WindowCloseOptions::IgnoreExitGuard);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0, _engineWindow->getInfo()->state | NSSPWIN::WindowState::Focused);
	if (auto *macosWin = dynamic_cast<NSSPWIN::MacosWindow *>(_engineWindow)) {
		macosWin->setHasBeenKey(true);
	}
}

- (void)windowDidResignKey:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0, _engineWindow->getInfo()->state & ~NSSPWIN::WindowState::Focused);
	// Popup dismiss lives in the mapWindow NSEvent monitor, not here: resignKey also fires on
	// ordinary key transfers and would self-close the popup immediately.
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window defaultFrame:(NSRect)newFrame {
	return newFrame;
}

- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame {
	return YES;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0,
			_engineWindow->getInfo()->state | NSSPWIN::WindowState::Fullscreen);

	_engineWindow->handleFullscreenTransitionComplete(
			NSSPWIN::MacosFullscreenRequest::EnterFullscreen);

	[_engineWindow->getWindow() setOpaque:YES];
	[self.view.layer setOpaque:YES];
}

- (void)windowWillExitFullScreen:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->updateState(0,
			_engineWindow->getInfo()->state & ~NSSPWIN::WindowState::Fullscreen);

	[_engineWindow->getWindow() setOpaque:NO];
	[self.view.layer setOpaque:NO];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	if (_engineWindow->getFullscreenRequest()
			== NSSPWIN::MacosFullscreenRequest::ToggleFullscreen) {
		_engineWindow->handleFullscreenTransitionComplete(
				NSSPWIN::MacosFullscreenRequest::ExitFullscreen);
		[_engineWindow->getWindow() toggleFullScreen:nil withScreen:_engineWindow->getNextScreen()];
	} else {
		_engineWindow->handleFullscreenTransitionComplete(
				NSSPWIN::MacosFullscreenRequest::ExitFullscreen);
	}
}

- (NSArray<NSWindow *> *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
														onScreen:(NSScreen *)screen;
{ return @[_engineWindow->getWindow()]; }

- (void)window:(NSWindow *)window
		startCustomAnimationToEnterFullScreenOnScreen:(NSScreen *)screen
										 withDuration:(NSTimeInterval)duration {
	XL_MACOS_LOG("MacosWindow", "startCustomAnimationToEnterFullScreenOnScreen");

	auto frame = window.frame;
	[window setStyleMask:([window styleMask] | NSWindowStyleMaskFullScreen)];
	[window setFrame:frame display:YES];

	[(SPRTMacosWindow *)window setFrame:screen.frame
								display:YES
							   duration:duration
					  completionHandler:nil];
}

- (NSArray<NSWindow *> *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
	return @[_engineWindow->getWindow()];
}

- (void)window:(NSWindow *)window
		startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
	XL_MACOS_LOG("MacosWindow", "startCustomAnimationToExitFullScreenWithDuration");

	__weak SPRTMacosWindow *w = _engineWindow->getWindow();

	auto frame = w.screen.frame;
	[w setStyleMask:w.defaultStyle];
	[w setFrame:frame display:YES];

	auto origFrame = _engineWindow->getOriginalFrame();

	auto center = CGPoint{origFrame.origin.x + origFrame.size.width / 2.0,
		origFrame.origin.y + origFrame.size.height / 2.0};

	NSScreen *screen = nil;

	for (NSScreen *s in NSScreen.screens) {
		if (NSPointInRect(center, s.frame)) {
			screen = s;
			break;
		}
	}

	if (_engineWindow->getFullscreenRequest()
			== NSSPWIN::MacosFullscreenRequest::ToggleFullscreen) {
		auto screenFrame = window.screen.frame;
		auto x = (screenFrame.size.width - origFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - origFrame.size.height) / 2.0;
		auto targetRect =
				NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y}, origFrame.size};

		[w setFrame:targetRect
						  display:YES
						 duration:duration
				completionHandler:^() {
				  XL_MACOS_LOG("SPRTMacosWindow",
						  "startCustomAnimationToExitFullScreenWithDuration complete");
				}];

	} else if (screen != w.screen) {
		// this is bypass for the layout bug, when window unfullscreened to other screen
		// first, we unfullscreen window to screen center, then move to final position.
		// If we unfullsceen it into another screen - it will disappear from desktop

		auto screenFrame = window.screen.frame;
		auto x = (screenFrame.size.width - origFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - origFrame.size.height) / 2.0;
		auto targetRect =
				NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y}, origFrame.size};

		[w setFrame:targetRect
						  display:YES
						 duration:duration
				completionHandler:^() {
				  [w setFrame:origFrame
									display:YES
								   duration:[w animationResizeTime:origFrame]
						  completionHandler:^() {
							XL_MACOS_LOG("SPRTMacosWindow",
									"startCustomAnimationToExitFullScreenWithDuration complete");
						  }];
				}];
	} else {
		[w setFrame:origFrame
						  display:YES
						 duration:duration
				completionHandler:^() {
				  XL_MACOS_LOG("SPRTMacosWindow",
						  "startCustomAnimationToExitFullScreenWithDuration complete");
				}];
	}
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window {
	XL_MACOS_LOG("SPRTMacosWindow", "windowDidFailToExitFullScreen");
}

- (void)mouseDown:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	_buttonGripFlags = _engineWindow->getGripFlags();
	_buttons.set(NSSP::toInt(NSSPWIN::InputMouseButton::MouseLeft));

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSSPWIN::InputEventName::Begin,
		{{
			NSSPWIN::InputMouseButton::MouseLeft,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	};

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)rightMouseDown:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	_buttons.set(NSSP::toInt(NSSPWIN::InputMouseButton::MouseRight));

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSSPWIN::InputEventName::Begin,
		{{
			NSSPWIN::InputMouseButton::MouseRight,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	};

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)otherMouseDown:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	_buttons.set(NSSP::toInt(NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber))));

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSSPWIN::InputEventName::Begin,
		{{
			NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
			mods,
			float(loc.x),
			float(loc.y),
		}},
	};

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)mouseUp:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	if (_buttons.test(NSSP::toInt(NSSPWIN::InputMouseButton::MouseLeft))) {
		_buttons.reset(NSSP::toInt(NSSPWIN::InputMouseButton::MouseLeft));

		auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
		CGPoint loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
		auto mods =
				NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

		NSSPWIN::InputEventData event{
			static_cast<uint32_t>(theEvent.buttonNumber),
			NSSPWIN::InputEventName::End,
			{{
				NSSPWIN::InputMouseButton::MouseLeft,
				mods,
				float(loc.x),
				float(loc.y),
			}},
		};

		_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
		_currentPointerLocation = loc;
	}
}

- (void)rightMouseUp:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	if (_buttons.test(NSSP::toInt(NSSPWIN::InputMouseButton::MouseRight))) {
		_buttons.reset(NSSP::toInt(NSSPWIN::InputMouseButton::MouseRight));

		auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
		auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
		auto mods =
				NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

		NSSPWIN::InputEventData event{
			static_cast<uint32_t>(theEvent.buttonNumber),
			NSSPWIN::InputEventName::End,
			{{
				NSSPWIN::InputMouseButton::MouseRight,
				mods,
				float(loc.x),
				float(loc.y),
			}},
		};

		_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
		_currentPointerLocation = loc;
	}
}

- (void)otherMouseUp:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	if (_buttons.test(NSSP::toInt(NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber))))) {
		_buttons.reset(NSSP::toInt(NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber))));

		auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
		auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
		auto mods =
				NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

		NSSPWIN::InputEventData event{
			static_cast<uint32_t>(theEvent.buttonNumber),
			NSSPWIN::InputEventName::End,
			{{
				NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
				mods,
				float(loc.x),
				float(loc.y),
			}},
		};

		_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
		_currentPointerLocation = loc;
	}
}

- (void)mouseMoved:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::InputEventData event{
		sprt::Max<uint32_t>,
		NSSPWIN::InputEventName::MouseMove,
		{{
			NSSPWIN::InputMouseButton::None,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	};

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)clearButtonEvents {
	for (uint32_t i = 0; i < 64; ++i) {
		if (_buttons.test(i)) {
			NSSPWIN::Vector<NSSPWIN::InputEventData> events;
			events.emplace_back(NSSPWIN::InputEventData({
				NSSPWIN::getMacosButtonNumber(NSSPWIN::InputMouseButton(i)),
				NSSPWIN::InputEventName::Cancel,
				{{
					NSSPWIN::InputMouseButton(i),
					NSSPWIN::InputModifier::None,
					NSSP::NaN<float>,
					NSSP::NaN<float>,
				}},
			}));
			_engineWindow->handleInputEvents(move(events));
		}
	}
	_buttons.reset();
	_buttonGripFlags = NSSPWIN::WindowLayerFlags::None;
}

- (void)mouseDragged:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	if (_buttonGripFlags != NSSPWIN::WindowLayerFlags::None) {
		if (_buttons.test(NSSP::toInt(NSSPWIN::InputMouseButton::MouseLeft))
				&& _buttons.count() == 1) {
			if (_buttonGripFlags == NSSPWIN::WindowLayerFlags::MoveGrip) {
				[self clearButtonEvents];
				[_engineWindow->getWindow() performWindowDragWithEvent:theEvent];
				return;
			}
		}
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::Vector<NSSPWIN::InputEventData> events;

	events.emplace_back(NSSPWIN::InputEventData{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSSPWIN::InputEventName::Move,
		{{
			NSSPWIN::InputMouseButton::MouseLeft,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	});
	events.emplace_back(NSSPWIN::InputEventData{
		sprt::Max<uint32_t>,
		NSSPWIN::InputEventName::MouseMove,
		{{
			NSSPWIN::InputMouseButton::None,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	});

	_engineWindow->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)scrollWheel:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::Vector<NSSPWIN::InputEventData> events;

	uint32_t buttonId = 0;
	NSSPWIN::InputMouseButton buttonName;

	if (theEvent.scrollingDeltaY != 0) {
		if (theEvent.scrollingDeltaY > 0) {
			buttonName = NSSPWIN::InputMouseButton::MouseScrollUp;
		} else {
			buttonName = NSSPWIN::InputMouseButton::MouseScrollDown;
		}
	} else {
		if (theEvent.scrollingDeltaX > 0) {
			buttonName = NSSPWIN::InputMouseButton::MouseScrollRight;
		} else {
			buttonName = NSSPWIN::InputMouseButton::MouseScrollLeft;
		}
	}

	buttonId = sprt::Max<uint32_t> - sprt::toInt(buttonName);

	events.emplace_back(NSSPWIN::InputEventData{
		buttonId,
		NSSPWIN::InputEventName::Begin,
		{{
			buttonName,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	});
	events.emplace_back(NSSPWIN::InputEventData{
		buttonId,
		NSSPWIN::InputEventName::Scroll,
		{{
			buttonName,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	});
	events.emplace_back(NSSPWIN::InputEventData{
		buttonId,
		NSSPWIN::InputEventName::End,
		{{
			buttonName,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	});

	events.at(1).point.valueX = theEvent.scrollingDeltaX;
	events.at(1).point.valueY = theEvent.scrollingDeltaY;
	events.at(1).point.density = _engineWindow->getWindow().backingScaleFactor;

	_engineWindow->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSSPWIN::InputEventName::Move,
		{{
			NSSPWIN::InputMouseButton::MouseRight,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	};

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSSPWIN::InputEventName::Move,
		{{
			NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
			mods,
			float(loc.x),
			float(loc.y),
		}},
	};

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)mouseEntered:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSSPWIN::Vector<NSSPWIN::InputEventData> events;

	_engineWindow->updateState(0, _engineWindow->getInfo()->state | NSSPWIN::WindowState::Pointer);

	events.emplace_back(NSSPWIN::InputEventData{
		sprt::Max<uint32_t>,
		NSSPWIN::InputEventName::MouseMove,
		{{
			NSSPWIN::InputMouseButton::None,
			mods,
			float(loc.x),
			float(loc.y),
		}},
	});

	_engineWindow->handleInputEvents(sprt::move(events));
	_currentPointerLocation = loc;
}

- (void)mouseExited:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);

	_engineWindow->updateState(0, _engineWindow->getInfo()->state & ~NSSPWIN::WindowState::Pointer);
	_currentPointerLocation = loc;
}

- (BOOL)becomeFirstResponder {
	return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
	return [super resignFirstResponder];
}

- (void)viewDidChangeBackingProperties {
}

- (void)keyDown:(NSEvent *)theEvent {
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;
	auto code = [theEvent keyCode];

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(code),
		theEvent.isARepeat ? NSSPWIN::InputEventName::KeyRepeated
						   : NSSPWIN::InputEventName::KeyPressed,
		{{
			NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
			mods,
			float(_currentPointerLocation.x),
			float(_currentPointerLocation.y),
		}},
	};

	NSSPWIN::String chars = theEvent.characters.UTF8String;
	NSSPWIN::String ichars = theEvent.charactersIgnoringModifiers.UTF8String;

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = NSSPWIN::InputKeyComposeState::Disabled;
	event.key.keysym = NSSP::StringViewUtf8(ichars).getChar();
	event.key.keychar = NSSP::StringViewUtf8(chars).getChar();

	// Esc dismisses Popup windows via a deferred close on the controller looper —
	// never call close() synchronously from inside keyDown (AppKit deadlock).
	if (code == 53 /* kVK_Escape */ && _engineWindow
			&& _engineWindow->getInfo()->type == NSSPWIN::WindowType::Popup) {
		auto *ctrl = _engineWindow->getController();
		auto *win = _engineWindow;
		ctrl->getLooper()->performOnThread([win] {
			if (!win) {
				return;
			}
			if (auto *aw = win->getAppWindow()) {
				aw->hide();
			} else {
				win->close();
			}
		}, ctrl);
		return;
	}

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
}

- (void)keyUp:(NSEvent *)theEvent {
	auto mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;
	auto code = SPRT_OBJC_CALL([theEvent keyCode]);

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(code),
		NSSPWIN::InputEventName::KeyReleased,
		{{
			NSSPWIN::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
			mods,
			float(_currentPointerLocation.x),
			float(_currentPointerLocation.y),
		}},
	};

	NSSPWIN::String chars = theEvent.characters.UTF8String;
	NSSPWIN::String ichars = theEvent.charactersIgnoringModifiers.UTF8String;

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = NSSPWIN::InputKeyComposeState::Disabled;
	event.key.keysym = NSSP::StringViewUtf8(ichars).getChar();
	event.key.keychar = NSSP::StringViewUtf8(chars).getChar();

	_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
}

- (void)flagsChanged:(NSEvent *)theEvent {
	static sprt::pair<NSSPWIN::InputModifier, NSSPWIN::InputKeyCode> testmask[] = {
		sprt::make_pair(NSSPWIN::InputModifier::ShiftL, NSSPWIN::InputKeyCode::LEFT_SHIFT),
		sprt::make_pair(NSSPWIN::InputModifier::ShiftR, NSSPWIN::InputKeyCode::RIGHT_SHIFT),
		sprt::make_pair(NSSPWIN::InputModifier::CtrlL, NSSPWIN::InputKeyCode::LEFT_CONTROL),
		sprt::make_pair(NSSPWIN::InputModifier::CtrlR, NSSPWIN::InputKeyCode::RIGHT_CONTROL),
		sprt::make_pair(NSSPWIN::InputModifier::AltL, NSSPWIN::InputKeyCode::LEFT_ALT),
		sprt::make_pair(NSSPWIN::InputModifier::AltR, NSSPWIN::InputKeyCode::RIGHT_ALT),
		sprt::make_pair(NSSPWIN::InputModifier::WinL, NSSPWIN::InputKeyCode::LEFT_SUPER),
		sprt::make_pair(NSSPWIN::InputModifier::WinR, NSSPWIN::InputKeyCode::RIGHT_SUPER),
		sprt::make_pair(NSSPWIN::InputModifier::CapsLock, NSSPWIN::InputKeyCode::CAPS_LOCK),
		sprt::make_pair(NSSPWIN::InputModifier::NumLock, NSSPWIN::InputKeyCode::NUM_LOCK),
		sprt::make_pair(NSSPWIN::InputModifier::Mod5, NSSPWIN::InputKeyCode::WORLD_1),
		sprt::make_pair(NSSPWIN::InputModifier::Mod4, NSSPWIN::InputKeyCode::WORLD_2),
	};

	NSSPWIN::InputModifier mods = NSSPWIN::getInputModifiers(uint32_t(theEvent.modifierFlags));
	if (mods == _currentModifiers) {
		return;
	}

	auto diff = mods ^ _currentModifiers;

	NSSPWIN::InputEventData event{
		static_cast<uint32_t>(0),
		NSSPWIN::InputEventName::KeyReleased,
		{{
			NSSPWIN::InputMouseButton::None,
			mods,
			float(_currentPointerLocation.x),
			float(_currentPointerLocation.y),
		}},
	};

	for (auto &it : testmask) {
		if ((diff & it.first) != 0) {
			event.id = NSSP::toInt(it.first);
			if ((mods & it.first) != 0) {
				event.event = NSSPWIN::InputEventName::KeyPressed;
			}
			event.key.keycode = it.second;
			event.key.compose = NSSPWIN::InputKeyComposeState::Disabled;
			event.key.keysym = 0;
			event.key.keychar = 0;
			break;
		}
	}

	if (event.id) {
		_engineWindow->handleInputEvents(NSSPWIN::Vector<NSSPWIN::InputEventData>{event});
	}

	_currentModifiers = mods;
}

@end

@implementation SPRTMacosWindow

- (instancetype)initWithContentRect:(NSRect)contentRect
						  styleMask:(NSWindowStyleMask)style
							backing:(NSBackingStoreType)backingStoreType
							  defer:(BOOL)flag {
	self = [super initWithContentRect:contentRect
							styleMask:style
							  backing:backingStoreType
								defer:flag];
	_defaultStyle = style;
	_allowKey = YES;
	_allowMain = YES;
	return self;
}

- (void)configureRole:(BOOL)allowKey allowMain:(BOOL)allowMain {
	_allowKey = allowKey;
	_allowMain = allowMain;
}

- (BOOL)canBecomeKeyWindow {
	return _allowKey;
}

- (BOOL)canBecomeMainWindow {
	return _allowMain;
}

- (NSWindowStyleMask)defaultStyle {
	return _defaultStyle;
}

- (void)setFrame:(NSRect)frameRect
				  display:(BOOL)displayFlag
				 duration:(NSTimeInterval)duration
		completionHandler:(nullable void (^)(void))completionHandler {
	[(SPRTMacosViewController *)self.contentViewController setEngineLiveResize:YES];
	[NSAnimationContext
			runAnimationGroup:^(NSAnimationContext *_Nonnull context) {
			  context.duration = duration;
			  [self.animator setFrame:frameRect display:displayFlag];
			}
			completionHandler:^(void) {
			  [(SPRTMacosViewController *)self.contentViewController setEngineLiveResize:NO];
			  if (completionHandler) {
				  completionHandler();
			  }
			}];
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag {
	XL_MACOS_LOG("SPRTMacosWindow", "setFrame: ", frameRect.origin.x, " ", frameRect.origin.y, " ",
			frameRect.size.width, " ", frameRect.size.height);
	[super setFrame:frameRect display:displayFlag];
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag animate:(BOOL)animateFlag {
	XL_MACOS_LOG("SPRTMacosWindow", "setFrame: ", frameRect.origin.x, " ", frameRect.origin.y, " ",
			frameRect.size.width, " ", frameRect.size.height);
	if (!animateFlag) {
		[super setFrame:frameRect display:displayFlag animate:NO];
	} else {
		[self setFrame:frameRect
						  display:displayFlag
						 duration:[self animationResizeTime:frameRect]
				completionHandler:nil];
	}
}

- (void)toggleFullScreen:(id)sender withScreen:(NSScreen *)screen {
	if (screen == self.screen) {
		[self toggleFullScreen:sender];
	} else {
		auto screenFrame = screen.frame;
		auto windowFrame = self.frame;
		auto x = (screenFrame.size.width - windowFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - windowFrame.size.height) / 2.0;
		auto targetRect = NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y},
			windowFrame.size};

		__weak NSWindow *ref = self;

		[self setFrame:targetRect
						  display:YES
						 duration:[self animationResizeTime:targetRect]
				completionHandler:^() { [ref toggleFullScreen:sender]; }];
	}
}

- (NSTimeInterval)animationResizeTime:(NSRect)newFrame {
	return [super animationResizeTime:newFrame];
}

- (instancetype)animator {
	return [super animator];
}

- (id)animationForKey:(NSAnimatablePropertyKey)key {
	return [super animationForKey:key];
}

@end

#endif // SPRT_MACOS
