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

#include "SPRTWinHeadlessWindow.h"

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#include "SPRTWinHeadlessController.h"
#include <sprt/runtime/log.h>

namespace sprt::window {

HeadlessWindow::~HeadlessWindow() { }

HeadlessWindow::HeadlessWindow() { }

HeadlessContextController *HeadlessWindow::getHeadlessController() const {
	return static_cast<HeadlessContextController *>(_controller.get());
}

bool HeadlessWindow::init(NotNull<HeadlessContextController> c, Rc<WindowInfo> &&info) {
	if (info->type != WindowType::Root) {
		// An auxiliary window is positioned relative to a parent that must already be there;
		// ContextController::createWindow has checked that, but not that it is one of ours.
		auto *parent = dynamic_cast<HeadlessWindow *>(c->findWindow(info->parent));
		if (!parent) {
			oslog::vperror(__SPRT_LOCATION, "HeadlessWindow",
					"Auxiliary window parent is not available");
			return false;
		}

		const auto size = clampWindowExtent(Extent2(info->rect.width, info->rect.height),
				info->minExtent, info->maxExtent);
		const auto workArea = c->getVirtualScreenRect();
		const auto parentRect = parent->getContentScreenRect();

		if (info->type == WindowType::Popup || info->type == WindowType::Tooltip) {
			// Same positioner every other backend feeds, with the whole virtual desktop as the
			// work area - there is only ever one "monitor" here.
			info->rect = computeWindowPlacement(info->placement, size, parentRect, workArea);
		} else {
			// Dialog and Utility are placed by the window manager everywhere else. This one
			// centers them over their parent and keeps them on the virtual screen.
			const int32_t w = int32_t(size.width);
			const int32_t h = int32_t(size.height);
			int32_t x = parentRect.x + (int32_t(parentRect.width) - w) / 2;
			int32_t y = parentRect.y + (int32_t(parentRect.height) - h) / 2;

			// A window wider than the screen keeps its left/top edge visible rather than being
			// pushed off the other way, so max() is applied last.
			x = sprt::max(workArea.x, sprt::min(x, workArea.x + int32_t(workArea.width) - w));
			y = sprt::max(workArea.y, sprt::min(y, workArea.y + int32_t(workArea.height) - h));

			info->rect = IRect(x, y, size.width, size.height);
		}
	}

	// The pseudo-screen is whatever WindowInfo asks for: there is no WM to negotiate with.
	_extent = Extent2(info->rect.width, info->rect.height);

	// The requested actions are all granted: the emulated WM has no policy of its own, and this is
	// what tells an application which window buttons to draw.
	if (hasFlag(info->flags, WindowCreationFlags::AllowClose)) {
		info->state |= WindowState::AllowedClose;
	}
	if (hasFlag(info->flags, WindowCreationFlags::AllowResize)) {
		info->state |= WindowState::AllowedResize;
	}
	if (hasFlag(info->flags, WindowCreationFlags::AllowMove)) {
		info->state |= WindowState::AllowedMove;
	}

	// There is no device to probe, so the emulated WM declares one: this backend injects mouse
	// events and nothing else, and a widget that adapts to the input devices available should see
	// what it would see on a desktop. ContextFlags::HeadlessNoPointer takes it away, which is the
	// only way to reach the touch-shaped branch of such a widget under test.
	if (c->hasPointerDevice()) {
		info->state |= WindowState::InputPointer;
	}

	// Subwindows and UserSpaceDecorations are the capabilities this backend really has: the
	// controller creates auxiliary windows itself, so a menu can open a submenu off this window
	// like anywhere else, and a window with no window system around it draws its own frame by
	// definition. Everything else - server-side decorations, cursors, fullscreen, an OS icon -
	// needs a window system.
	return NativeWindow::init(c, move(info), c->getCapabilities());
}

void HeadlessWindow::mapWindow() {
	if (_mapped) {
		return;
	}
	_mapped = true;

	// Every window must emit its state exactly once as it maps: the app-thread mirror
	// (RenderServerChannel::getWindowState) starts empty and this event is the only thing that ever
	// fills it. Enabled is raised here rather than in init() precisely so that the transition
	// happens now - a menu takes neither focus nor the pointer, so nothing else below would emit
	// anything for it at all.
	//
	// Nothing here is blocked to begin with; a modal Dialog clears Enabled on its parent through
	// ContextController::retainModalBlock, exactly as it does with a window system in play.
	updateState(0, _info->state | WindowState::Enabled);

	getHeadlessController()->handleWindowMapped(this);
}

void HeadlessWindow::unmapWindow() {
	if (!_mapped) {
		return;
	}
	_mapped = false;
	getHeadlessController()->handleWindowUnmapped(this);
}

bool HeadlessWindow::close() {
	if (_closed) {
		return true;
	}

	// There is no window manager to route the close back to us, so the notification the WM-backed
	// windows get for free has to be raised here. Without it the controller never drops the window
	// and the process hangs after the presentation engine is torn down.
	_closed = true;
	if (!_controller->notifyWindowClosed(this)) {
		_closed = false;
		return false;
	}
	return true;
}

Extent2 HeadlessWindow::getExtent() const { return _extent; }

IRect HeadlessWindow::getContentScreenRect() const {
	return IRect(_info->rect.x, _info->rect.y, _extent.width, _extent.height);
}

void HeadlessWindow::updateFocusState(bool focused) {
	updateState(0,
			focused ? (_info->state | WindowState::Focused)
					: (_info->state & ~WindowState::Focused));
}

void HeadlessWindow::updatePointerState(bool within) {
	updateState(0,
			within ? (_info->state | WindowState::Pointer)
				   : (_info->state & ~WindowState::Pointer));
}

SurfaceInterfaceInfo HeadlessWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Headless;
	return ret;
}

SurfaceInfo HeadlessWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	// The window - not the surface - is the authority on the pseudo-screen size here, and it is
	// what setExtent() moves. Without this the surface would keep reporting the extent it was
	// built with and a resize would recreate the swapchain at the old size.
	info.currentExtent = _extent;
	info.minImageExtent = Extent2(1, 1);
	info.maxImageExtent = _extent;
	return sprt::move(info);
}

PresentationOptions HeadlessWindow::getPreferredOptions() const {
	PresentationOptions opts;

	// Selects the pseudo-swapchain presentation engine in the gAPI backend.
	opts.headless = true;

	// There is no display link and no WM tick, so frames are driven entirely by
	// setReadyForNextFrame (the `frame` command on the inspector socket). Without this a headless
	// process would burn a GPU and a core rendering frames nobody will ever look at.
	opts.renderOnDemand = true;
	opts.followDisplayLink = false;
	opts.followDisplayLinkBarrier = false;

	// No vsync to pace against: present is a memcpy-less no-op, so a present window would only add
	// latency between "render this" and "the image is readable".
	opts.usePresentWindow = false;

	// The pseudo-swapchain hands out images synchronously and can not signal a VkFence from the
	// host, so the engine must take the fence-less acquisition path.
	opts.acquireImageWithoutFence = true;

	return opts;
}

bool HeadlessWindow::applyExtent(Extent2 extent) {
	auto clamped = clampWindowExtent(extent, _info->minExtent, _info->maxExtent);
	if (clamped == _extent) {
		return false;
	}

	_extent = clamped;
	_info->rect.width = clamped.width;
	_info->rect.height = clamped.height;

	// A menu can not survive its owner being resized under it - the anchor it was placed against
	// has moved. Every backend does this; here the resize is the only way geometry ever changes.
	_controller->dismissChildPopups(this, "owner-resized");

	// ...which is also why this is the only place the application's view of the geometry can go
	// stale here: there is no move to report.
	_controller->notifyWindowGeometryChanged(this);
	return true;
}

bool HeadlessWindow::setContentExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return false;
	}

	// The caller (AppWindow::setContentExtent) updates the presentation constraints itself once
	// this returns true, so - unlike setExtent - nothing is notified from here.
	return applyExtent(extent);
}

Status HeadlessWindow::setExtent(Extent2 extent) {
	if (extent.width == 0 || extent.height == 0) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!applyExtent(extent)) {
		return Status::Done; // already at that size, nothing to deprecate
	}

	// Same path a WM-driven resize takes: deprecate the swapchain so the next frame is rendered
	// (and captured) at the new extent.
	_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::WindowResized);
	return Status::Ok;
}

/* THE EMULATED WINDOW MANAGER'S HALF OF USER-SPACE DECORATIONS.

The capability says the window system draws no frame and the application draws its own, which
leaves the application with a title bar and eight resize edges that nothing acts on unless the
backend acts on them. Every other backend hands the press to the WM - xcb _NET_WM_MOVERESIZE,
Wayland xdg_toplevel::move, Win32 HTCAPTION - and there is no WM here, so this window moves and
resizes ITSELF on the virtual screen it already owns. Without this the capability would be a
courtesy bit: the flag would survive Context::configureWindow, the decorations would be built, and
a drag on them would do nothing at all.

A grip press is SWALLOWED, exactly as it is everywhere else: the window system takes the drag, so
the scene never sees the press or anything after it. WindowLayerFlags::GripGuard has already
resolved to nothing by the time _gripFlags is read (see NativeWindow::updateLayerState), so a
button drawn on a title bar keeps its click.

WHAT AN INJECTED COORDINATE MEANS WHILE A DRAG RUNS: it is read against the window rect AS IT WAS
WHEN THE PRESS LANDED. A real pointer reports root coordinates and the window slides out from under
it; a synthetic one has no root to report, so the drag is defined in the frame the caller was
aiming in - which is what makes a drag from (x0, y0) to (x1, y1) move the window by exactly
(x1 - x0, y0 - y1), whatever happened in between. */
void HeadlessWindow::handleInputEvents(Vector<InputEventData> &&events) {
	// The flag is INHERITED by every auxiliary window of a decorated one (see SubWindow::open), and
	// a menu or a hint must not be draggable by its own edges - it is placed by its owner and
	// dismissed by a press, not resized. Same exclusion every other backend makes, spelled the same
	// way: `auxiliary` is Popup and Tooltip, and Dialog and Utility are ordinary windows.
	const bool auxiliary = _info->type == WindowType::Popup || _info->type == WindowType::Tooltip;
	if (auxiliary || !hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		NativeWindow::handleInputEvents(sprt::move(events));
		return;
	}

	Vector<InputEventData> forwarded;
	forwarded.reserve(events.size());

	for (auto &event : events) {
		if (_gripDrag != WindowLayerFlags::None) {
			switch (event.event) {
			case InputEventName::Move:
			case InputEventName::MouseMove: updateGripDrag(event.getLocation()); continue;
			case InputEventName::End:
			case InputEventName::Cancel: _gripDrag = WindowLayerFlags::None; continue;
			default: break;
			}
		} else if (event.event == InputEventName::Begin
				&& event.getButton() == InputMouseButton::MouseLeft) {
			// The layer under the press has to be resolved BEFORE the grip is read. A real pointer
			// could not have pressed without travelling there first, so every other backend reads
			// a grip its own motion handler already computed; an injected stream is routinely a
			// Begin with no move in front of it, and would engage whatever the last hover left.
			handleMotionEvent(event);

			auto grip = _gripFlags & WindowLayerFlags::GripMask;
			if (grip != WindowLayerFlags::None && startGripDrag(grip, event.getLocation())) {
				continue;
			}
		}
		forwarded.emplace_back(event);
	}

	NativeWindow::handleInputEvents(sprt::move(forwarded));
}

bool HeadlessWindow::startGripDrag(WindowLayerFlags grip, Vec2 local) {
	// The emulated WM has no policy of its own beyond the one init() derived from the creation
	// flags - so what an application asked not to be allowed, it is not allowed here either.
	const bool move = grip == WindowLayerFlags::MoveGrip;
	if (!hasFlag(_info->state, move ? WindowState::AllowedMove : WindowState::AllowedResize)) {
		return false;
	}

	_gripDrag = grip;
	_gripAnchor = local;
	_gripRect = IRect(_info->rect.x, _info->rect.y, _extent.width, _extent.height);

	// A menu can not outlive its owner being grabbed - the anchor it was placed against is about to
	// move. Every backend takes them down on a reconfigure; here the reconfigure starts now.
	_controller->dismissChildPopups(this, "owner-grabbed");
	return true;
}

void HeadlessWindow::updateGripDrag(Vec2 local) {
	// The window's own space is Y-UP and in logical points; the virtual screen is Y-DOWN. Both
	// deltas are taken against the anchor frozen at the press - see the note above.
	const int32_t dx = int32_t(roundf(local.x - _gripAnchor.x));
	const int32_t dy = -int32_t(roundf(local.y - _gripAnchor.y));

	if (_gripDrag == WindowLayerFlags::MoveGrip) {
		applyGripGeometry(
				IRect(_gripRect.x + dx, _gripRect.y + dy, _gripRect.width, _gripRect.height));
		return;
	}

	int32_t left = _gripRect.x;
	int32_t top = _gripRect.y;
	int32_t right = left + int32_t(_gripRect.width);
	int32_t bottom = top + int32_t(_gripRect.height);

	switch (_gripDrag) {
	case WindowLayerFlags::ResizeTopLeftGrip:
		left += dx;
		top += dy;
		break;
	case WindowLayerFlags::ResizeTopGrip: top += dy; break;
	case WindowLayerFlags::ResizeTopRightGrip:
		right += dx;
		top += dy;
		break;
	case WindowLayerFlags::ResizeRightGrip: right += dx; break;
	case WindowLayerFlags::ResizeBottomRightGrip:
		right += dx;
		bottom += dy;
		break;
	case WindowLayerFlags::ResizeBottomGrip: bottom += dy; break;
	case WindowLayerFlags::ResizeBottomLeftGrip:
		left += dx;
		bottom += dy;
		break;
	case WindowLayerFlags::ResizeLeftGrip: left += dx; break;
	default: return;
	}

	auto clamped = clampWindowExtent(
			Extent2(uint32_t(sprt::max(1, right - left)), uint32_t(sprt::max(1, bottom - top))),
			_info->minExtent, _info->maxExtent);

	// The edge the grip does NOT hold stays put, so a window dragged past its minimum stops growing
	// on the side under the pointer instead of sliding away from it.
	if (left != _gripRect.x) {
		left = right - int32_t(clamped.width);
	}
	if (top != _gripRect.y) {
		top = bottom - int32_t(clamped.height);
	}

	applyGripGeometry(IRect(left, top, clamped.width, clamped.height));
}

void HeadlessWindow::applyGripGeometry(const IRect &rect) {
	const bool moved = rect.x != _info->rect.x || rect.y != _info->rect.y;

	_info->rect.x = rect.x;
	_info->rect.y = rect.y;

	if (applyExtent(Extent2(rect.width, rect.height))) {
		// Same path a WM-driven resize takes; applyExtent has already reported the geometry and
		// taken the child popups down.
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::WindowResized);
	} else if (moved) {
		// A move changes nothing about the swapchain, but everything about where a popup would be
		// placed - and it is the one geometry change this backend could not make before.
		_controller->notifyWindowGeometryChanged(this);
	}
}

// There is no on-screen keyboard to raise, but the window still stands in for the IME so that a
// headless run can be driven through the real text-input path: events injected into
// handleInputEvents are intercepted by the shared TextInputProcessor, and performTextInput() can
// reproduce composition. Declining here would leave isTextInputEnabled() false and make text input
// untestable without a display.
bool HeadlessWindow::updateTextInput(const TextInputRequest &, TextInputFlags) {
	if (_textInput) {
		_textInput->handleInputEnabled(true);
	}
	return true;
}

void HeadlessWindow::cancelTextInput() {
	if (_textInput) {
		_textInput->handleInputEnabled(false);
	}
}

} // namespace sprt::window

#endif // __SPRT_RUNTIME_CONFIG_HAVE_WINDOW
