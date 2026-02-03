/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_WINDOW_INFO_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_WINDOW_INFO_H_

#include <sprt/runtime/geom.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/mode.h>
#include <sprt/runtime/stream.h>

namespace sprt::window {

/** View tiling constraints flags. If flag is set - window enge is constrained, if not set - resizable  */
enum class ViewConstraints : uint32_t {
	None,
	Top = 1 << 0,
	Left = 1 << 1,
	Bottom = 1 << 2,
	Right = 1 << 3,

	Vertical = Top | Bottom,
	Horizontal = Left | Right,
	All = Vertical | Horizontal
};

SPRT_DEFINE_ENUM_AS_MASK(ViewConstraints)

// Cross-OS window state flags
// Some OS/WN can support only some subset of this flags
enum class WindowState : uint64_t {
	None = 0,

	// Window is in modal state
	Modal = 1LLU << 0,

	// Window is sticked to stay in place between virtual desktops
	Sticky = 1LLU << 1,

	// Window is maximized vertically
	MaximizedVert = 1LLU << 2,

	// Window is maximized horizontally
	MaximizedHorz = 1LLU << 3,

	// Window is maximized (both flags should be set)
	Maximized = MaximizedVert | MaximizedHorz,

	// XCB: _NET_WM_STATE_SHADED
	Shaded = 1LLU << 4,

	// XCB: _NET_WM_STATE_SKIP_TASKBAR
	SkipTaskbar = 1LLU << 5,

	// XCB: _NET_WM_STATE_SKIP_PAGER
	SkipPager = 1LLU << 6,

	// Window was minimized or moved to background
	Minimized = 1LLU << 7,
	Background = Minimized,

	// Window in fullscreen state
	Fullscreen = 1LLU << 8,

	// Window is above most other windows
	Above = 1LLU << 9,

	// Window is below most other windows
	Below = 1LLU << 10,

	// Window demands user attention
	DemandsAttention = 1LLU << 11,

	// Window have input focus
	Focused = 1LLU << 12,

	// Window in live resize process
	Resizing = 1LLU << 13,

	// Pointer now within window
	Pointer = 1LLU << 14,

	// Block any close request for a window
	// CloseGuard will prevent OS WM to close window on it's side.
	// If CloseGuard is enabled, when WM or user tries to close window, it remains open and you will receive
	// WindowState notification with CloseRequest flag set.
	// When CloseRequest flag is set, ypu should commit this request with enableState(WindowState::CloseRequest)
	// to close window or discard it with disableState(WindowState::CloseRequest) to remove this flag and re-enable CloseGuard
	// Also, when CloseRequest flag is set, next `close` call or next WM close action will close this window
	//
	// Note that on Android, Back Gesture is not a subject for CloseGuard, use
	// WindowLayerFlag::BackButtonHandler and Predictive Back Gesture to achive similar user experience
	// (https://developer.android.com/guide/navigation/custom-back/predictive-back-gesture)
	CloseGuard = 1LLU << 15,

	// WM asked user to close this window
	CloseRequest = 1LLU << 16,

	// Window is available for input events
	Enabled = 1LLU << 17,

	// Android decoration handling
	// This flags mapped to WindowInsetController class methods
	DecorationStatusBarVisible = 1LLU << 18,
	DecorationNavigationVisible = 1LLU << 19,
	DecorationStatusBarLight = 1LLU << 20,
	DecorationNavigationLight = 1LLU << 21,
	DecorationShowBySwipe = 1LLU << 22,
	DecorationState = DecorationStatusBarVisible | DecorationNavigationVisible
			| DecorationStatusBarLight | DecorationNavigationLight | DecorationShowBySwipe,

	// Extra space here

	// Values for allowed window actions
	AllowedWindowMenu = 1LLU << 45,
	AllowedMove = 1LLU << 46,
	AllowedResize = 1LLU << 47,
	AllowedMinimize = 1LLU << 48,
	AllowedShade = 1LLU << 49,
	AllowedStick = 1LLU << 50,
	AllowedMaximizeVert = 1LLU << 51,
	AllowedMaximizeHorz = 1LLU << 52,
	AllowedClose = 1LLU << 53,
	AllowedFullscreen = 1LLU << 54,
	AllowedActionsMask = AllowedMove | AllowedResize | AllowedMinimize | AllowedShade | AllowedStick
			| AllowedMaximizeVert | AllowedMaximizeHorz | AllowedClose | AllowedFullscreen,

	// Window tiling constraints flag
	TiledLeft = 1LLU << 55,
	TiledRight = 1LLU << 56,
	TiledTop = 1LLU << 57,
	TiledBottom = 1LLU << 58,

	TiledTopLeft = TiledLeft | TiledTop,
	TiledTopRight = TiledRight | TiledTop,
	TiledBottomLeft = TiledLeft | TiledBottom,
	TiledBottomRight = TiledRight | TiledBottom,

	ConstrainedLeft = 1LLU << 59,
	ConstrainedRight = 1LLU << 60,
	ConstrainedTop = 1LLU << 61,
	ConstrainedBottom = 1LLU << 62,

	TilingMask = TiledLeft | TiledRight | TiledTop | TiledBottom | ConstrainedLeft
			| ConstrainedRight | ConstrainedTop | ConstrainedBottom,

	All = Modal | Sticky | Maximized | Shaded | SkipTaskbar | Minimized | Fullscreen | Above | Below
			| DemandsAttention | Focused | Resizing | Pointer | CloseGuard | CloseRequest | Enabled
			| DecorationState | AllowedActionsMask | TilingMask,
};

SPRT_DEFINE_ENUM_AS_MASK(WindowState)

enum class WindowCursor : uint8_t {
	Undefined,
	Default,
	ContextMenu,
	Help,
	Pointer,
	Progress,
	Wait,
	Cell,
	Crosshair,
	Text,
	VerticalText,
	Alias,
	Copy,
	Move,
	NoDrop,
	NotAllowed,
	Grab,
	Grabbing,

	AllScroll,
	ZoomIn,
	ZoomOut,
	DndAsk,

	RightPtr,
	Pencil,
	Target,

	ResizeRight,
	ResizeTop,
	ResizeTopRight,
	ResizeTopLeft,
	ResizeBottom,
	ResizeBottomRight,
	ResizeBottomLeft,
	ResizeLeft,
	ResizeLeftRight,
	ResizeTopBottom,
	ResizeTopRightBottomLeft,
	ResizeTopLeftBottomRight,
	ResizeCol,
	ResizeRow,
	ResizeAll,
	Max,
};

enum class WindowLayerFlags : uint32_t {
	None,
	MoveGrip,
	ResizeTopLeftGrip,
	ResizeTopGrip,
	ResizeTopRightGrip,
	ResizeRightGrip,
	ResizeBottomRightGrip,
	ResizeBottomGrip,
	ResizeBottomLeftGrip,
	ResizeLeftGrip,
	GripGuard, // to restrict grip for some layers

	GripMask = 0xF,

	// Grip-like flag to open window menu with left-click
	WindowMenuLeft = 1 << 4,

	// Grip-like flag to open window menu with right-click
	WindowMenuRight = 1 << 5,

	// Set for listeners, that handles Android system back action
	// Used for Android Predictive back gesture
	//
	// If at least one InputListener with this flag is enabled, back gesture will be captured by application,
	// and predictive gesture will be blocked.
	// If there are none, back gesture will be sent into system, and system animation will be shown
	// See more about Predictive back gesture in Android doc:
	// https://developer.android.com/guide/navigation/custom-back/predictive-back-gesture
	BackButtonHandler = 1 << 6,
};

SPRT_DEFINE_ENUM_AS_MASK(WindowLayerFlags)

struct WindowLayer {
	Rect rect;
	WindowCursor cursor = WindowCursor::Undefined;
	WindowLayerFlags flags = WindowLayerFlags::None;

	operator bool() const {
		return cursor != WindowCursor::Undefined || flags != WindowLayerFlags::None;
	}

	bool operator==(const WindowLayer &) const = default;
	bool operator!=(const WindowLayer &) const = default;
};

// This flags defined when window is created, and immutable
// Note that window manager can decide, what actual capabilities window will have,
// you can only ask for a some capabilities
//
// For actions (like move), you can ask to allow requests, but if you don't,
// it will not make window non-movable, it only can forbid to move it via server-size decorations
//
// To acquire actual actions flags, provided by WM, use WindowUpdate InputEvent,
// or AppWindow::getWindowActions()
enum class WindowCreationFlags : uint32_t {
	None = 0,

	// Ask window manager to allow move requests
	AllowMove = 1 << 0,

	// Ask window manager to allow resize requests
	AllowResize = 1 << 1,

	// Ask window manager to allow minimize requests
	AllowMinimize = 1 << 2,

	// Ask window manager to allow maximize requests
	AllowMaximize = 1 << 3,

	// Ask window manager to allow fullscreen requests
	AllowFullscreen = 1 << 4,

	// Ask window manager to allow close requests
	AllowClose = 1 << 5,

	// Flags for the regular, common OS window
	Regular =
			AllowMove | AllowResize | AllowMinimize | AllowMaximize | AllowFullscreen | AllowClose,

	// draw window without server-side decoration borders
	UserSpaceDecorations = 1 << 6,

	// On android, allows setPreferredFrameRate only if seamless
	OnlySeamlessFrameRateSwitch = 1 << 7,

	// Use direct output to display, bypassing whole WM stack
	// Check if it actually supported with WindowCapabilities::DirectOutput
	DirectOutput = 1 << 27,

	// Try to use server-side decoration where available
	PreferServerSideDecoration = 1 << 28,

	// Try to use system-native decoration where available
	// Used to enable client-side native decorations (libdecor on Wayland)
	PreferNativeDecoration = 1 << 29,

	// If possible, use server-defined cursors instead of client-side libraries
	// Note that server-side cursor theme may not contain all cursors
	PreferServerSideCursors = 1 << 30,
};

SPRT_DEFINE_ENUM_AS_MASK(WindowCreationFlags)

enum class WindowAttributes : uint32_t {
	None,
	Opaque = 1 << 0,
	Movable = 1 << 1,
	Resizeable = 1 << 2,
	Minimizable = 1 << 3,
	Maximizable = 1 << 4,
};

SPRT_DEFINE_ENUM_AS_MASK(WindowAttributes)

// Cababilities, prowided by OS Window Manager
enum class WindowCapabilities : uint32_t {
	None,

	// Switch between windowed and fullscreen modes
	// If not provided - window is only windowed or only fullscreen
	Fullscreen = 1 << 0,

	// Suupport for exculsive output to display, can be more performant then usual fullscreen.
	// Note that on some platforms (Wayland, MacOS, some X11 implementations), there is no need
	// in specific exculsive mode, it's automatic
	FullscreenExclusive = 1 << 1,

	// Support for different display modes in fullscreen mode
	// Without this capability, only ModeInfo::Current can be set as mmode for FullscreenInfo
	FullscreenWithMode = 1 << 2,

	// Systems with this capability can switch display mode without exit/enter cycle when
	// application already fullscreened
	FullscreenSeamlessModeSwitch = 1 << 3,

	// Window server can draw window decoration by itself
	// Without this capability, application is responsible for decoration drawing
	ServerSideDecorations = 1 << 4,

	// There is a client library for native client decoration drawing
	NativeDecorations = 1 << 5,

	// Window server can draw cursors by itself
	ServerSideCursors = 1 << 6,

	// Subwindows are allowed
	Subwindows = 1 << 7,

	// Direct output is available on platform
	DirectOutput = 1 << 8,

	// Full user-space decoration mode is supported
	UserSpaceDecorations = 1 << 9,

	// Above and below state supprted
	AboveBelowState = 1 << 10,

	// DemandsAttention state supprted
	DemandsAttentionState = 1 << 11,

	// SkipTaskbar and SkipPager state supported
	SkipTaskbarState = 1 << 12,

	// Enabled state supported
	EnabledState = 1 << 13,

	// Platform supports CloseGuard
	CloseGuard = 1 << 14,

	// Window supports separate MaximizeVert/MaximizeHorz in enableState/disableState
	SeparateMaximize = 1 << 15,

	// Window moving in maximized state is allowed
	AllowMoveFromMaximized = 1 << 16,

	// For buttons in grip layer to work, GripGuard flag required
	GripGuardsRequired = 1 << 17,

	// GL Scene should assist to draw user space shadows
	UserShadowsRequired = 1 << 18,

	// It's adiced to preserve AppWindow's Director instead of recreate it with a new window.
	// By default, Director will be preserved by WindowInfo::id parameter and will be connectoed
	// to next window with this id
	PreserveDirector = 1 << 19,

	// setPreferredModeSwitch is available
	PreferredFrameRate = 1 << 20,

	// Decoration state can be changed by application (mostly Android)
	DecorationState = 1 << 21,
};

SPRT_DEFINE_ENUM_AS_MASK(WindowCapabilities)

struct SPRT_API WindowInfo final : public Ref {
	memory::dynstring id;
	memory::dynstring title;
	IRect rect = IRect(0, 0, 1'024, 768);
	float density = 0.0f;
	WindowCreationFlags flags = WindowCreationFlags::None;

	// initial fullscreen mode
	FullscreenInfo fullscreen = FullscreenInfo::None;

	// TODO: extra window attributes go here

	PresentMode preferredPresentMode = PresentMode::Mailbox;
	ImageFormat imageFormat = ImageFormat::Undefined;
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;

	// provided by WM, no reason to set it diraectly
	WindowCapabilities capabilities = WindowCapabilities::None;

	// Current window state
	WindowState state = WindowState::None;

	// Insets for decorations, that appears above user-drawing space
	// Canvas inside this inset always be visible for user
	Padding decorationInsets;
};

SPRT_API void getWindowStateDescription(const callback<void(StringView)> &, WindowState);

} // namespace sprt::window

namespace sprt {

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::WindowState state) {
	getWindowStateDescription(cb, state);
	return cb;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_WINDOW_INFO_H_
