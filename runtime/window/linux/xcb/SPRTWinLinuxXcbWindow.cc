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

#include "SPRTWinLinuxXcbWindow.h"
#include "../SPRTWinLinuxController.h"
#include "../SPRTWinLinux.h"
#include "SPRTWinLinuxXcbDisplayConfigManager.h"
#include "SPRTWinLinuxXcbSoftwareSurface.h"

#include <sprt/runtime/enum.h>

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h" // IWYU pragma: keep
#endif

#ifndef XL_X11_DEBUG
#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::source().debug("XCB", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif
#endif

namespace sprt::window {

static InputModifier getModifiers(uint32_t mask) {
	InputModifier ret = InputModifier::None;
	InputModifier *mod,
			mods[] = {InputModifier::Shift, InputModifier::CapsLock, InputModifier::Ctrl,
				InputModifier::Alt, InputModifier::NumLock, InputModifier::Mod3,
				InputModifier::Mod4, InputModifier::Mod5, InputModifier::Button1,
				InputModifier::Button2, InputModifier::Button3, InputModifier::Button4,
				InputModifier::Button5, InputModifier::LayoutAlternative};
	for (mod = mods; mask; mask >>= 1, mod++) {
		if (mask & 1) {
			ret |= *mod;
		}
	}
	return ret;
}

static InputMouseButton getButton(xcb_button_t btn) { return InputMouseButton(btn); }

static XcbMoveResize getMoveResizeForGrip(WindowLayerFlags grip) {
	switch (grip) {
	case WindowLayerFlags::MoveGrip: return XcbMoveResize::Move; break;
	case WindowLayerFlags::ResizeTopLeftGrip: return XcbMoveResize::SizeTopLeft; break;
	case WindowLayerFlags::ResizeTopGrip: return XcbMoveResize::SizeTop; break;
	case WindowLayerFlags::ResizeTopRightGrip: return XcbMoveResize::SizeTopRight; break;
	case WindowLayerFlags::ResizeRightGrip: return XcbMoveResize::SizeRight; break;
	case WindowLayerFlags::ResizeBottomRightGrip: return XcbMoveResize::SizeBottomRight; break;
	case WindowLayerFlags::ResizeBottomGrip: return XcbMoveResize::SizeBottom; break;
	case WindowLayerFlags::ResizeBottomLeftGrip: return XcbMoveResize::SizeBottomLeft; break;
	case WindowLayerFlags::ResizeLeftGrip: return XcbMoveResize::SizeLeft; break;
	default: break;
	}
	return XcbMoveResize::Cancel;
}

static bool XcbWindow_updateState(XcbConnection *conn, WindowState state, xcb_window_t window,
		bool add) {
	xcb_client_message_event_t msg;
	msg.response_type = XCB_CLIENT_MESSAGE;
	msg.format = 32;
	msg.sequence = 0;
	msg.window = window;
	msg.data.data32[0] = 0;
	msg.data.data32[1] = 0;
	msg.data.data32[2] = 0;
	msg.data.data32[3] = 0;
	msg.data.data32[4] = 0;

	if (state == WindowState::Minimized) {
		msg.type = conn->getAtom(XcbAtomIndex::WM_CHANGE_STATE);
		msg.data.data32[0] = add ? XCB_ICCCM_WM_STATE_ICONIC : XCB_ICCCM_WM_STATE_NORMAL;
	} else {
		msg.type = conn->getAtom(XcbAtomIndex::_NET_WM_STATE);
		msg.data.data32[0] = add ? 1 : 0;
		msg.data.data32[3] = 1; // EWMH says 1 for normal applications

		switch (state) {
		case WindowState::Sticky:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_STICKY);
			break;
		case WindowState::MaximizedVert:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_VERT);
			break;
		case WindowState::MaximizedHorz:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_HORZ);
			break;
		case WindowState::Maximized:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_VERT);
			msg.data.data32[2] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_HORZ);
			break;
		case WindowState::Shaded:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_SHADED);
			break;
		case WindowState::SkipTaskbar:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_TASKBAR);
			break;
		case WindowState::SkipPager:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_PAGER);
			break;
		case WindowState::Fullscreen:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_FULLSCREEN);
			break;
		case WindowState::Above:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_ABOVE);
			break;
		case WindowState::Below:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_BELOW);
			break;
		case WindowState::DemandsAttention:
			msg.data.data32[1] = conn->getAtom(XcbAtomIndex::_NET_WM_STATE_DEMANDS_ATTENTION);
			break;
		default: return false; break;
		}
	}

	conn->getXcb()->xcb_send_event(conn->getConnection(), 0, conn->getDefaultScreen()->root,
			XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
			(const char *)&msg);
	return true;
}

XcbWindow::~XcbWindow() {
	if (_connection) {
		_defaultScreen = nullptr;
		if (_xinfo.syncCounter) {
			_xcb->xcb_sync_destroy_counter(_connection->getConnection(), _xinfo.syncCounter);
			_xinfo.syncCounter = 0;
		}

		if (_xinfo.outputWindow) {
			_xcb->xcb_destroy_window(_connection->getConnection(), _xinfo.outputWindow);
			_xinfo.outputWindow = 0;
		}

		if (_xinfo.window) {
			_xcb->xcb_destroy_window(_connection->getConnection(), _xinfo.window);
			_xinfo.window = 0;
		}

		if (_xinfo.colormap) {
			_xcb->xcb_free_colormap(_connection->getConnection(), _xinfo.colormap);
			_xinfo.colormap = 0;
		}
		_connection = nullptr;
	}
}

XcbWindow::XcbWindow() { }

bool XcbWindow::init(NotNull<XcbConnection> conn, Rc<WindowInfo> &&info,
		NotNull<LinuxContextController> c) {
	// The CONTROLLER's capabilities, not the connection's — see the same call in
	// SPRTWinLinuxWaylandWindow.cc. The connection answers for X11 alone; the dialog bits (and,
	// here, NativeDialogParenting) belong to the desktop session and are ORed on by
	// LinuxContextController::getCapabilities().
	if (!NativeWindow::init(c, move(info), c->getCapabilities())) {
		return false;
	}

	_connection = conn;

	if (_connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
		_info->state |= WindowState::AllowedWindowMenu;
	}

	// The device list is probed once when the connection comes up; windows created later read the
	// current answer here, and get subsequent changes through handleTouchscreenStateChanged.
	if (_connection->hasTouchscreen()) {
		_info->state |= WindowState::InputTouch;
	}

	_xcb = _connection->getXcb();

	if (_connection->hasErrors()) {
		return false;
	}

	StringView bundleName = _controller->getContext()->getInfo()->bundleName;

	_wmClass.resize(_info->title.size() + bundleName.size() + 1, char(0));
	__sprt_memcpy(_wmClass.data(), _info->title.data(), _info->title.size());
	__sprt_memcpy(_wmClass.data() + _info->title.size() + 1, bundleName.data(), bundleName.size());

	if (_info->icon) {
		// _NET_WM_ICON is CARDINAL[][2+n]/32: every image contributes its width, its height and
		// then width*height ARGB words, and they are all concatenated into the one property.
		size_t words = 0;
		for (auto &img : _info->icon->images) {
			if (img.isValid()) {
				words += 2 + size_t(img.extent.width) * size_t(img.extent.height);
			}
		}
		if (words) {
			_iconData.resize(words);
			auto out = _iconData.data();
			for (auto &img : _info->icon->images) {
				if (!img.isValid()) {
					continue;
				}
				*out++ = img.extent.width;
				*out++ = img.extent.height;
				packIconArgbPremultiplied(img, out);
				out += size_t(img.extent.width) * size_t(img.extent.height);
			}
		}
	}

	_defaultScreen = _connection->getDefaultScreen();

	_xinfo.parent = _defaultScreen->root;
	// `auxiliary` is the undecorated, WM-bypassing kind (menus and hints): it drives the visual,
	// the placement and the grab. `transient` is the wider "belongs to another window" set, which
	// additionally covers Dialog and Utility - those stay WM-managed and decorated, they just get a
	// parent, a type hint and no taskbar entry of their own.
	const bool auxiliary = _info->type == WindowType::Popup || _info->type == WindowType::Tooltip;
	const bool transient = _info->type != WindowType::Root;
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations) && !auxiliary) {
		_xinfo.depth = 32;
		_xinfo.visual = _connection->getVisualByDepth(32); //_defaultScreen->root_visual;
	} else {
		_xinfo.depth = XCB_COPY_FROM_PARENT;
		_xinfo.visual = _defaultScreen->root_visual;
	}

	_xinfo.eventMask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS
			| XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS
			| XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_VISIBILITY_CHANGE
			| XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
			| XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
			| XCB_EVENT_MASK_COLOR_MAP_CHANGE | XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	_xinfo.overrideRedirect = auxiliary ? 1 : 0;
	_xinfo.overrideClose = true;
	_xinfo.enableSync = !auxiliary;

	auto udpi = _connection->getUnscaledDpi();
	auto dpi = _connection->getDpi();

	_density = float(dpi) / float(udpi);

	auto *auxParent =
			transient ? dynamic_cast<XcbWindow *>(_controller->findWindow(_info->parent)) : nullptr;
	if (transient && !auxParent) {
		oslog::vperror(__SPRT_LOCATION, "XCB", "Auxiliary window parent is not available");
		return false;
	}

	if (auxiliary) {
		auto parentRect = auxParent->getContentScreenRect();

		// Constrain to the monitor the parent is on, not to the whole desktop: sliding a menu
		// across a monitor edge to keep it "on screen" would drop it onto the neighbour display.
		IRect workArea(0, 0, int32_t(_defaultScreen->width_in_pixels / _density),
				int32_t(_defaultScreen->height_in_pixels / _density));
		if (auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig()) {
			workArea = cfg->desktopRect;
			const auto center = IVec2(parentRect.x + parentRect.width / 2,
					parentRect.y + parentRect.height / 2);
			for (auto &logical : cfg->logical) {
				if (center.x >= logical.rect.x && center.y >= logical.rect.y
						&& center.x < logical.rect.x + logical.rect.width
						&& center.y < logical.rect.y + logical.rect.height) {
					workArea = logical.rect;
					break;
				}
			}
		}
		auto placed = computeWindowPlacement(_info->placement,
				Extent2(_info->rect.width, _info->rect.height), parentRect, workArea);
		_info->rect = placed;
		_xinfo.boundingRect = xcb_rectangle_t{static_cast<int16_t>(placed.x * _density),
			static_cast<int16_t>(placed.y * _density),
			static_cast<uint16_t>(placed.width * _density),
			static_cast<uint16_t>(placed.height * _density)};
	} else if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		auto &theme = _controller->getThemeInfo();
		_xinfo.boundingRect = xcb_rectangle_t{static_cast<int16_t>(_info->rect.x * _density),
			static_cast<int16_t>(_info->rect.y * _density),
			static_cast<uint16_t>(
					(_info->rect.width + theme.decorations.shadowWidth * 2) * _density),
			static_cast<uint16_t>(
					(_info->rect.height + theme.decorations.shadowWidth * 2) * _density)};
	} else {
		_xinfo.boundingRect = xcb_rectangle_t{static_cast<int16_t>(_info->rect.x * _density),
			static_cast<int16_t>(_info->rect.y * _density),
			static_cast<uint16_t>(_info->rect.width * _density),
			static_cast<uint16_t>(_info->rect.height * _density)};
	}

	_xinfo.contentRect = getContentRect(_xinfo.boundingRect);

	_xinfo.title = _info->title;
	_xinfo.icon = _info->title;
	_xinfo.wmClass = _wmClass;
	_xinfo.iconData = _iconData;

	if (!_connection->createWindow(_info, _xinfo)) {
		oslog::vperror(__SPRT_LOCATION, "XCB", "Fail to create window");
		return false;
	}

	_xcb->xcb_map_window(_connection->getConnection(), _xinfo.outputWindow);

	_frameRate = getCurrentFrameRate();

	// EWMH taxonomy: the WM applies its own decoration, stacking and taskbar policy from this.
	xcb_atom_t windowType = XCB_NONE;
	switch (_info->type) {
	case WindowType::Popup:
		windowType = _connection->getAtom(StringView("_NET_WM_WINDOW_TYPE_POPUP_MENU"));
		break;
	case WindowType::Tooltip:
		windowType = _connection->getAtom(StringView("_NET_WM_WINDOW_TYPE_TOOLTIP"));
		break;
	case WindowType::Dialog:
		windowType = _connection->getAtom(XcbAtomIndex::_NET_WM_WINDOW_TYPE_DIALOG);
		break;
	case WindowType::Utility:
		windowType = _connection->getAtom(XcbAtomIndex::_NET_WM_WINDOW_TYPE_UTILITY);
		break;
	case WindowType::Root:
		windowType = _connection->getAtom(XcbAtomIndex::_NET_WM_WINDOW_TYPE_NORMAL);
		break;
	}

	_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, _xinfo.window,
			_connection->getAtom(XcbAtomIndex::_NET_WM_WINDOW_TYPE), XCB_ATOM_ATOM, 32, 1,
			&windowType);

	if (auxParent) {
		auto parentWindow = auxParent->getWindow();
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
				_xinfo.window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1, &parentWindow);
	}

	// Initial _NET_WM_STATE, set as a property before the window is mapped (the client-message form
	// used by enableState only works on a mapped window).
	{
		Vector<xcb_atom_t> initialState;
		if (_info->type == WindowType::Dialog
				&& hasFlag(_info->flags, WindowCreationFlags::Modal)) {
			// A hint, nothing more: the WM may raise the dialog with its parent and dim the parent's
			// decorations, but the input block is enforced by ContextController::_modalBlocks.
			initialState.emplace_back(_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MODAL));
		}
		if (_info->type == WindowType::Utility) {
			// A palette belongs to its owner, not to the desktop. UTILITY alone is enough for
			// EWMH-strict WMs; these two cover the ones that only look at the state.
			initialState.emplace_back(
					_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_TASKBAR));
			initialState.emplace_back(_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_PAGER));
		}
		if (!initialState.empty()) {
			_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
					_xinfo.window, _connection->getAtom(XcbAtomIndex::_NET_WM_STATE), XCB_ATOM_ATOM,
					32, uint32_t(initialState.size()), initialState.data());
		}
	}

	struct MotifWmHints hints;
	hints.flags = MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
	hints.decorations = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU;
	hints.functions = 0;
	hints.inputMode = 0;
	hints.status = 0;

	for (auto f : flags(_info->flags)) {
		switch (f) {
		case WindowCreationFlags::AllowMove: hints.functions |= MWM_FUNC_MOVE; break;
		case WindowCreationFlags::AllowResize:
			hints.decorations |= MWM_DECOR_RESIZEH;
			hints.functions |= MWM_FUNC_RESIZE;
			break;
		case WindowCreationFlags::AllowMinimize:
			hints.decorations |= MWM_DECOR_MINIMIZE;
			hints.functions |= MWM_FUNC_MINIMIZE;
			break;
		case WindowCreationFlags::AllowMaximize:
			hints.decorations |= MWM_DECOR_MAXIMIZE;
			hints.functions |= MWM_FUNC_MAXIMIZE;
			break;
		case WindowCreationFlags::AllowClose: hints.functions |= MWM_FUNC_CLOSE; break;
		default: break;
		}
	}

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations) && !auxiliary) {
		hints.decorations = 0;
	}
	if (auxiliary) {
		hints.decorations = 0;
		hints.functions = 0;
	}

	_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, _xinfo.window,
			_connection->getAtom(XcbAtomIndex::_MOTIF_WM_HINTS), XCB_ATOM_CARDINAL, 32, 5, &hints);

	// Immutable min/max size constraints via WM_NORMAL_HINTS. libxcb-icccm is not loaded, so we
	// fill xcb_size_hints_t by hand and push it with the already-available xcb_change_property.
	// Values describe the outer (bounding) window in device pixels, matching how boundingRect is
	// built above: logical constraints are scaled by _density and, for user-space decorations,
	// grown by the shadow border.
	if (_info->minExtent != Extent2::ZERO || _info->maxExtent != Extent2::ZERO) {
		// largest window edge X can express (CARD16), used as "unbounded" for a single dimension
		constexpr int32_t Unbounded = 0x7FFF;

		int32_t frame = 0;
		if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
			frame = _controller->getThemeInfo().decorations.shadowWidth * 2;
		}

		xcb_size_hints_t sizeHints{};
		if (_info->minExtent.width != 0 || _info->minExtent.height != 0) {
			sizeHints.flags |= XCB_ICCCM_SIZE_HINT_P_MIN_SIZE;
			if (_info->minExtent.width != 0) {
				sizeHints.min_width = int32_t((_info->minExtent.width + frame) * _density);
			}
			if (_info->minExtent.height != 0) {
				sizeHints.min_height = int32_t((_info->minExtent.height + frame) * _density);
			}
		}
		if (_info->maxExtent.width != 0 || _info->maxExtent.height != 0) {
			sizeHints.flags |= XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
			sizeHints.max_width = _info->maxExtent.width != 0
					? int32_t((_info->maxExtent.width + frame) * _density)
					: Unbounded;
			sizeHints.max_height = _info->maxExtent.height != 0
					? int32_t((_info->maxExtent.height + frame) * _density)
					: Unbounded;
		}

		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
				_xinfo.window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32,
				XCB_ICCCM_NUM_WM_SIZE_HINTS_ELEMENTS, &sizeHints);
	}

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations) && !auxiliary) {
		auto &theme = _controller->getThemeInfo();
		generateShadowPixmaps(theme.decorations.shadowWidth * _density,
				theme.decorations.borderRadius * _density);
	}

	_xcb->xcb_flush(_connection->getConnection());

	return true;
}

void XcbWindow::handleExpose(xcb_expose_event_t *ev) {
	if (!hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		return;
	}

	auto &theme = _controller->getThemeInfo();

	auto writeCorner = [&](XcbShadowCornerContext &ctx, int16_t x, int16_t y) {
		_xcb->xcb_copy_area(_connection->getConnection(), ctx.pixmap, _xinfo.window,
				_xinfo.decorationGc, 0, 0, x, y, ctx.width, ctx.width);
	};

	auto shadowWidth = static_cast<uint32_t>(theme.decorations.shadowWidth * _density);

	// Fill internal rect before drawing shadow
	auto cornerSize = static_cast<uint32_t>(theme.decorations.borderRadius * _density);
	if (cornerSize) {
		uint32_t color = static_cast<uint32_t>(_shadowCurrentValue * 255.0f) << 24;
		uint32_t values[2] = {color, 0};
		_xcb->xcb_change_gc(_connection->getConnection(), _xinfo.decorationGc,
				XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, values);

		xcb_rectangle_t rects[] = {
			{int16_t(shadowWidth), int16_t(shadowWidth),
				uint16_t(_xinfo.boundingRect.width - shadowWidth * 2),
				uint16_t(_xinfo.boundingRect.height - shadowWidth * 2)},
		};

		_xcb->xcb_poly_fill_rectangle(_connection->getConnection(), _xinfo.window,
				_xinfo.decorationGc, 1, rects);
	}

	// draw rectangles with rounded corners
	uint32_t paddingLeft_Top = 0;
	uint32_t paddingLeft_Bottom = 0;
	uint32_t paddingRight_Top = 0;
	uint32_t paddingRight_Bottom = 0;
	uint32_t paddingTop_Left = 0;
	uint32_t paddingTop_Right = 0;
	uint32_t paddingBottom_Left = 0;
	uint32_t paddingBottom_Right = 0;

	if (!hasFlag(_info->state, WindowState::TiledTopLeft)) {
		writeCorner(_xinfo.shadowTopLeft, 0, 0);
		paddingLeft_Top += _xinfo.shadowTopLeft.width;
		paddingTop_Left += _xinfo.shadowTopLeft.width;
	}

	if (!hasFlag(_info->state, WindowState::TiledTopRight)) {
		writeCorner(_xinfo.shadowTopRight, _xinfo.boundingRect.width - _xinfo.shadowTopRight.width,
				0);
		paddingRight_Top += _xinfo.shadowTopRight.width;
		paddingTop_Right += _xinfo.shadowTopRight.width;
	}

	if (!hasFlag(_info->state, WindowState::TiledBottomLeft)) {
		writeCorner(_xinfo.shadowBottomLeft, 0,
				_xinfo.boundingRect.height - _xinfo.shadowTopRight.width);

		paddingLeft_Bottom += _xinfo.shadowBottomLeft.width;
		paddingBottom_Left += _xinfo.shadowBottomLeft.width;
	}

	if (!hasFlag(_info->state, WindowState::TiledBottomRight)) {
		writeCorner(_xinfo.shadowBottomRight,
				_xinfo.boundingRect.width - _xinfo.shadowTopRight.width,
				_xinfo.boundingRect.height - _xinfo.shadowTopRight.width);

		paddingRight_Bottom += _xinfo.shadowBottomRight.width;
		paddingBottom_Right += _xinfo.shadowBottomRight.width;
	}

	// Draw shadows on edges line by line
	makeShadowVector([&](uint32_t index, float value) {
		uint32_t color = static_cast<uint32_t>(value * _shadowCurrentValue * 255.0f) << 24;

		uint32_t values[2] = {color, 0};
		_xcb->xcb_change_gc(_connection->getConnection(), _xinfo.decorationGc,
				XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, values);

		uint32_t nrects = 0;
		xcb_rectangle_t rects[4];

		if (!hasFlag(_info->state, WindowState::TiledLeft)) {
			rects[nrects++] = {int16_t(shadowWidth - index - 1), int16_t(paddingLeft_Top), 1,
				uint16_t(_xinfo.boundingRect.height - paddingLeft_Top - paddingLeft_Bottom)};
		}

		if (!hasFlag(_info->state, WindowState::TiledRight)) {
			rects[nrects++] = {int16_t(_xinfo.boundingRect.width - shadowWidth + index),
				int16_t(paddingRight_Top), 1,
				uint16_t(_xinfo.boundingRect.height - paddingRight_Top - paddingRight_Bottom)};
		}

		if (!hasFlag(_info->state, WindowState::TiledTop)) {
			rects[nrects++] = {int16_t(paddingTop_Left), int16_t(shadowWidth - index - 1),
				uint16_t(_xinfo.boundingRect.width - paddingTop_Left - paddingTop_Right), 1};
		}

		if (!hasFlag(_info->state, WindowState::TiledBottom)) {
			rects[nrects++] = {int16_t(paddingBottom_Left),
				int16_t(_xinfo.boundingRect.height - shadowWidth + index),
				uint16_t(_xinfo.boundingRect.width - paddingBottom_Left - paddingBottom_Right), 1};
		}

		_xcb->xcb_poly_fill_rectangle(_connection->getConnection(), _xinfo.window,
				_xinfo.decorationGc, nrects, rects);
	}, static_cast<uint32_t>(theme.decorations.shadowWidth * _density));
}

void XcbWindow::handleConfigureNotify(xcb_configure_notify_event_t *ev) {
	auto mid = IVec2(ev->x + ev->width / 2, ev->y + ev->height / 2);
	auto mon = _connection->getDisplayConfigManager()->getMonitorForPosition(mid.x, mid.y);

	XL_X11_LOG("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d monitor:%s",
			ev->event, ev->window, ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width),
			uint32_t(ev->override_redirect), mon.data());
	// A click on the WM decoration never reaches us: under a compositor the title bar is the
	// compositor's own surface, so neither our pointer grab nor a focus change sees it. The
	// restack/move it produces on the owner does arrive here, and it is reason enough to take
	// the menu down — a popup must not outlive its owner being raised or moved.
	if (_info->type != WindowType::Popup && _info->type != WindowType::Tooltip) {
		_controller->dismissChildPopups(this, "owner-reconfigured");
	}

	_xinfo.boundingRect.x = ev->x;
	_xinfo.boundingRect.y = ev->y;
	_xinfo.outputName = mon;
	_borderWidth = ev->border_width;
	if (ev->width != _xinfo.boundingRect.width || ev->height != _xinfo.boundingRect.height) {
		_xinfo.boundingRect.width = ev->width;
		_xinfo.boundingRect.height = ev->height;

		auto newContentRect = getContentRect(_xinfo.boundingRect);
		if (!isEqual(_xinfo.contentRect, newContentRect)) {
			updateContentRect(newContentRect);
		}
	}

	_info->rect = IRect{
		_xinfo.boundingRect.x + _xinfo.contentRect.x,
		_xinfo.boundingRect.y + _xinfo.contentRect.y,
		_xinfo.contentRect.width,
		_xinfo.contentRect.height,
	};
}

void XcbWindow::handlePropertyNotify(xcb_property_notify_event_t *ev) {
	if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_WM_STATE)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, _connection->getAtom(XcbAtomIndex::_NET_WM_STATE), XCB_ATOM_ATOM, 0,
				sizeof(xcb_atom_t) * 16);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			auto values = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

			const auto stateMask = WindowState::Modal | WindowState::Sticky
					| WindowState::MaximizedVert | WindowState::MaximizedHorz | WindowState::Shaded
					| WindowState::SkipTaskbar | WindowState::SkipPager | WindowState::Minimized
					| WindowState::Fullscreen | WindowState::Above | WindowState::Below
					| WindowState::DemandsAttention | WindowState::Focused;
			auto state = WindowState::None;

			while (len > 0) {
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MODAL) == *values) {
					state |= WindowState::Modal;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_STICKY) == *values) {
					state |= WindowState::Sticky;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_VERT)
						== *values) {
					state |= WindowState::MaximizedVert;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_HORZ)
						== *values) {
					state |= WindowState::MaximizedHorz;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SHADED) == *values) {
					state |= WindowState::Shaded;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_TASKBAR)
						== *values) {
					state |= WindowState::SkipTaskbar;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_PAGER)
						== *values) {
					state |= WindowState::SkipPager;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_HIDDEN) == *values) {
					state |= WindowState::Minimized;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_FULLSCREEN)
						== *values) {
					state |= WindowState::Fullscreen;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_ABOVE) == *values) {
					state |= WindowState::Above;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_BELOW) == *values) {
					state |= WindowState::Below;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_DEMANDS_ATTENTION)
						== *values) {
					state |= WindowState::DemandsAttention;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_FOCUSED) == *values) {
					state |= WindowState::Focused;
				}
				++values;
				--len;
			}

			if (hasFlag(_info->state ^ state, WindowState::Focused)) {
				auto &theme = _controller->getThemeInfo();
				auto targetValue = theme.decorations.shadowMinValue;
				if (hasFlag(state, WindowState::Focused)) {
					targetValue = theme.decorations.shadowMaxValue;
				}
				if (_shadowCurrentValue != targetValue) {
					_shadowCurrentValue = targetValue;
					updateShadows();
				}
			}
			if ((_info->state & stateMask) != state) {
				updateState(ev->time, (_info->state & ~stateMask) | state);
			}
		}
	} else if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_WM_ALLOWED_ACTIONS)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, _connection->getAtom(XcbAtomIndex::_NET_WM_ALLOWED_ACTIONS),
				XCB_ATOM_ATOM, 0, sizeof(xcb_atom_t) * 16);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			auto values = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

			WindowState actions = WindowState::None;
			while (len > 0) {
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_MOVE) == *values) {
					actions |= WindowState::AllowedMove;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_RESIZE) == *values) {
					actions |= WindowState::AllowedResize;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_MINIMIZE) == *values) {
					actions |= WindowState::AllowedMinimize;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_MAXIMIZE_HORZ)
						== *values) {
					actions |= WindowState::AllowedMaximizeHorz;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_MAXIMIZE_VERT)
						== *values) {
					actions |= WindowState::AllowedMaximizeVert;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_FULLSCREEN)
						== *values) {
					actions |= WindowState::AllowedFullscreen;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_CLOSE) == *values) {
					actions |= WindowState::AllowedClose;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_SHADE) == *values) {
					actions |= WindowState::AllowedShade;
				} else if (_connection->getAtom(XcbAtomIndex::_NET_WM_ACTION_STICK) == *values) {
					actions |= WindowState::AllowedStick;
				} else {
					XL_X11_LOG("XcbWindow: Unknown action: %s",
							_connection->getAtomName(*values).data());
				}
				++values;
				--len;
			}
			auto current = _info->state & WindowState::AllowedActionsMask;
			if (current != actions) {
				updateState(ev->time, (_info->state & ~WindowState::AllowedActionsMask) | actions);
			}
		}
	} else if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_WM_DESKTOP)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, ev->atom, XCB_ATOM_CARDINAL, 0, 32);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply && _xcb->xcb_get_property_value_length(reply) >= int(sizeof(int32_t))) {
			XL_X11_LOG("XcbWindow: handlePropertyNotify _NET_WM_DESKTOP: %d %ld",
					*((int32_t *)_xcb->xcb_get_property_value(reply)),
					_xcb->xcb_get_property_value_length(reply) / sizeof(int32_t));
		}
	} else if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_FRAME_EXTENTS)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, ev->atom, XCB_ATOM_CARDINAL, 0, 32);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply && _xcb->xcb_get_property_value_length(reply) >= int(sizeof(uint32_t) * 4)) {
			SPRT_UNUSED auto values = (uint32_t *)_xcb->xcb_get_property_value(reply);

			XL_X11_LOG("XcbWindow: handlePropertyNotify: %s %d %d %d %d",
					_connection->getAtomName(ev->atom).data(), values[0], values[1], values[2],
					values[3]);
		}
	} else if (ev->atom == _connection->getAtom(XcbAtomIndex::_GTK_EDGE_CONSTRAINTS)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, ev->atom, XCB_ATOM_CARDINAL, 0, 32);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			auto values = (uint32_t *)_xcb->xcb_get_property_value(reply);
			auto len = _xcb->xcb_get_property_value_length(reply);

			auto state = _info->state;
			if (len > 0) {
				for (auto it : flags(XcbConstraints(values[0]))) {
					switch (it) {
					case XcbConstraints::TopTiled: state |= WindowState::TiledTop; break;
					case XcbConstraints::TopResizable: state &= ~WindowState::TiledTop; break;
					case XcbConstraints::RightTiled: state |= WindowState::TiledRight; break;
					case XcbConstraints::RightResizable: state &= ~WindowState::TiledRight; break;
					case XcbConstraints::BottomTiled: state |= WindowState::TiledBottom; break;
					case XcbConstraints::BottomResizable: state &= ~WindowState::TiledBottom; break;
					case XcbConstraints::LeftTiled: state |= WindowState::TiledLeft; break;
					case XcbConstraints::LeftResizable: state &= ~WindowState::TiledLeft; break;
					}
				}
			}
			if (state != _info->state) {
				updateState(ev->time, state);
				auto newContentRect = getContentRect(_xinfo.boundingRect);
				if (!isEqual(_xinfo.contentRect, newContentRect)) {
					updateContentRect(newContentRect);
				}
			}
		}
	} else if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_WM_USER_TIME)) {
		// do nothing
	} else {
		SPRT_UNUSED auto name = _connection->getAtomName(ev->atom);
		XL_X11_LOG("XcbWindow: handlePropertyNotify: %s", name.data());
	}
}

void XcbWindow::handleButtonPress(xcb_button_press_event_t *ev) {
	// With an active pointer grab the press is reported relative to the grab window, so
	// out-of-bounds coordinates mean "clicked outside the menu" — dismiss the whole chain.
	if (_popupGrabbed
			&& (ev->event_x < 0 || ev->event_y < 0
					|| ev->event_x >= int16_t(_xinfo.boundingRect.width)
					|| ev->event_y >= int16_t(_xinfo.boundingRect.height))) {
		_controller->dismissPopupChain(this);
		return;
	}

	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	_lastPointerRootX = ev->root_x;
	_lastPointerRootY = ev->root_y;
	_lastPointerButton = ev->detail;

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);
	auto btn = getButton(ev->detail);

	if (btn == InputMouseButton::MouseLeft) {
		// Capture current grip flags
		if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuLeft)
				&& _connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
			startGrip(XcbMoveResize::Menu, ev->root_x, ev->root_y, ev->detail);
			return;
		} else {
			_buttonGripFlags = _gripFlags;
		}
	} else if (btn == InputMouseButton::MouseRight) {
		if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuRight)
				&& _connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
			startGrip(XcbMoveResize::Menu, ev->root_x, ev->root_y, ev->detail);
			return;
		}
	}

	_buttons.set(ev->detail);

	InputEventData event({
		ev->detail,
		InputEventName::Begin,
		{{
			btn,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	switch (btn) {
	case InputMouseButton::MouseScrollUp:
		event.event = InputEventName::Scroll;
		event.point.valueX = 0.0f;
		event.point.valueY = 10.0f;
		break;
	case InputMouseButton::MouseScrollDown:
		event.event = InputEventName::Scroll;
		event.point.valueX = 0.0f;
		event.point.valueY = -10.0f;
		break;
	case InputMouseButton::MouseScrollLeft:
		event.event = InputEventName::Scroll;
		event.point.valueX = 10.0f;
		event.point.valueY = 0.0f;
		break;
	case InputMouseButton::MouseScrollRight:
		event.event = InputEventName::Scroll;
		event.point.valueX = -10.0f;
		event.point.valueY = 0.0f;
		break;
	default: break;
	}

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleButtonRelease(xcb_button_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	_lastPointerRootX = ev->root_x;
	_lastPointerRootY = ev->root_y;

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);
	auto btn = getButton(ev->detail);

	if (btn == InputMouseButton::MouseLeft) {
		// Release current grip flags
		_buttonGripFlags = WindowLayerFlags::None;
	}

	if (_buttons.test(ev->detail)) {
		_buttons.reset(ev->detail);

		InputEventData event({
			ev->detail,
			InputEventName::End,
			{{
				btn,
				mod,
				float(ev->event_x - _xinfo.contentRect.x),
				float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
			}},
		});

		switch (btn) {
		case InputMouseButton::MouseScrollUp:
		case InputMouseButton::MouseScrollDown:
		case InputMouseButton::MouseScrollLeft:
		case InputMouseButton::MouseScrollRight: break;
		default: _pendingEvents.emplace_back(event); break;
		}
	} else {
		startGrip(XcbMoveResize::Cancel, ev->root_x, ev->root_y,
				toInt(InputMouseButton::MouseLeft));
	}
}

void XcbWindow::handleMotionNotify(xcb_motion_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	_lastPointerRootX = ev->root_x;
	_lastPointerRootY = ev->root_y;

	if (_buttonGripFlags != WindowLayerFlags::None) {
		if (_buttons.test(toInt(InputMouseButton::MouseLeft)) && _buttons.count() == 1) {
			/* Only when the grip named a move or a resize the WM can perform.

			getMoveResizeForGrip answers Cancel for everything else - GripGuard included - and this
			used to send that Cancel to the WM and `return` anyway, so a guarded area consumed every
			motion of a pressed drag and the scene never saw one. Cancel is the absence of an
			action, not an action: drop the grip and let the motion through. */
			auto value = getMoveResizeForGrip(_buttonGripFlags);
			if (value != XcbMoveResize::Cancel) {
				startGrip(value, ev->root_x, ev->root_y, toInt(InputMouseButton::MouseLeft));
				return;
			}
			_buttonGripFlags = WindowLayerFlags::None;
		}
	}

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);

	InputEventData event({
		Max<uint32_t>,
		InputEventName::MouseMove,
		{{
			InputMouseButton::None,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleEnterNotify(xcb_enter_notify_event_t *ev) {
	XL_X11_LOG("handleEnterNotify");
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	_lastPointerRootX = ev->root_x;
	_lastPointerRootY = ev->root_y;

	updateState(ev->time, _info->state | WindowState::Pointer);

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);

	InputEventData event({
		Max<uint32_t>,
		InputEventName::MouseMove,
		{{
			InputMouseButton::None,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleLeaveNotify(xcb_leave_notify_event_t *ev) {
	XL_X11_LOG("handleLeaveNotify");
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	updateState(ev->time, _info->state & ~WindowState::Pointer);
}

void XcbWindow::handleTouchscreenStateChanged(bool value) {
	// XI hierarchy events carry a server timestamp, but it is not the user-interaction time this
	// window tracks, so pass 0 the way the other non-input state updates here do.
	updateState(0,
			value ? _info->state | WindowState::InputTouch
				  : _info->state & ~WindowState::InputTouch);
}

/* X11 delivers key events only to the focused window, so a modifier released elsewhere leaves
   _sideModifiers stale. QueryKeymap is the server's own answer to "which keys are physically
   down": a 32-byte bitmap indexed by keycode. One round trip, on focus-in only. */
void XcbWindow::resyncSideModifiers() {
	_sideModifiers = InputModifier::None;

	if (!_xcb->xcb_query_keymap || !_xcb->xcb_query_keymap_reply) {
		return;
	}

	auto cookie = _xcb->xcb_query_keymap(_connection->getConnection());
	auto reply = _xcb->xcb_query_keymap_reply(_connection->getConnection(), cookie, nullptr);
	if (!reply) {
		return;
	}

	for (uint32_t byte = 0; byte < 32; ++byte) {
		auto bits = reply->keys[byte];
		while (bits) {
			auto bit = __builtin_ctz(bits);
			bits &= bits - 1;
			_sideModifiers |=
					getKeySideModifier(_connection->getKeyCode(xcb_keycode_t(byte * 8 + bit)));
		}
	}

	::__sprt_free(reply);
}

void XcbWindow::handleFocusIn(xcb_focus_in_event_t *ev) {
	resyncSideModifiers();
	_forcedFrames += 2;
}

void XcbWindow::handleFocusOut(xcb_focus_out_event_t *ev) { _forcedFrames += 2; }

void XcbWindow::handleKeyPress(xcb_key_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	// The sided bits belong in the mask before the autorepeat check below compares it against the
	// pending event, which already carries them
	auto mod = getModifiers(ev->state) | _sideModifiers;
	auto ext = getExtent();

	// in case of key autorepeat, ev->time will match
	// just replace event name from previous InputEventName::KeyReleased to InputEventName::KeyRepeated
	if (!_pendingEvents.empty() && _pendingEvents.back().event == InputEventName::KeyReleased) {
		auto &iev = _pendingEvents.back();
		if (iev.id == ev->time && iev.getModifiers() == mod && iev.input.x == float(ev->event_x)
				&& iev.input.y == float(ext.height - ev->event_y)
				&& iev.key.keysym == _connection->getKeysym(ev->detail, ev->state, false)) {
			iev.event = InputEventName::KeyRepeated;
			return;
		}
	}

	InputEventData event({
		ev->time,
		InputEventName::KeyPressed,
		{{
			InputMouseButton::None,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	_connection->fillTextInputData(event, ev->detail, ev->state, isTextInputEnabled(), true);

	// A modifier key reports its own side on its own press, the way the Win32 backend does (it
	// queries the key state at event time, by which point the key is already down)
	if (auto side = getKeySideModifier(event.key.keycode); side != InputModifier::None) {
		_sideModifiers |= side;
		event.input.modifiers |= side;
	}

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleKeyRelease(xcb_key_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	auto mod = getModifiers(ev->state) | _sideModifiers;
	auto ext = getExtent();

	InputEventData event({
		ev->time,
		InputEventName::KeyReleased,
		{{
			InputMouseButton::None,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	_connection->fillTextInputData(event, ev->detail, ev->state, isTextInputEnabled(), false);

	// The key is up as of this event, so its side goes away with it - again matching Win32, which
	// would no longer see it in the key state
	if (auto side = getKeySideModifier(event.key.keycode); side != InputModifier::None) {
		_sideModifiers &= ~side;
		event.input.modifiers &= ~side;
	}

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleSyncRequest(xcb_timestamp_t syncTime, xcb_sync_int64_t value) {
	_lastSyncTime = syncTime;
	_xinfo.syncValue = value;
	_xinfo.syncFrameOrder = _frameOrder;
}

// WM_DELETE_WINDOW. Routed through close() rather than straight to the controller, so a guarded
// window raises WindowState::CloseRequest and the application gets to answer for it.
void XcbWindow::handleCloseRequest() { close(); }

void XcbWindow::notifyScreenChange() {
	auto newFrameRate = getCurrentFrameRate();
	if (newFrameRate != _frameRate) {
		_frameRate = newFrameRate;
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
	}
}

void XcbWindow::handleSettingsUpdated() {
	auto udpi = _connection->getUnscaledDpi();
	auto dpi = _connection->getDpi();

	if (udpi == 0) {
		udpi = 122'880;
	}

	auto d = float(dpi) / float(udpi);
	if (d != _density) {
		_density = float(dpi) / float(udpi);
		updateShadows();
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
	}
}

xcb_connection_t *XcbWindow::getConnection() const { return _connection->getConnection(); }

IRect XcbWindow::getContentScreenRect() const {
	// ConfigureNotify on a reparented toplevel reports coordinates relative to the WM frame, not
	// to the root, so boundingRect.x/y is not a screen position — ask the server for the real one.
	int16_t originX = _xinfo.boundingRect.x;
	int16_t originY = _xinfo.boundingRect.y;
	if (_xcb->xcb_translate_coordinates && _xcb->xcb_translate_coordinates_reply) {
		auto cookie = _xcb->xcb_translate_coordinates(_connection->getConnection(), _xinfo.window,
				_defaultScreen->root, 0, 0);
		if (auto reply = _connection->perform(_xcb->xcb_translate_coordinates_reply, cookie)) {
			originX = int16_t(reply->dst_x - _xinfo.contentRect.x);
			originY = int16_t(reply->dst_y - _xinfo.contentRect.y);
		}
	}
	return IRect(int32_t((originX + _xinfo.contentRect.x) / _density),
			int32_t((originY + _xinfo.contentRect.y) / _density),
			int32_t(_xinfo.contentRect.width / _density),
			int32_t(_xinfo.contentRect.height / _density));
}

void XcbWindow::mapWindow() {
	_connection->attachWindow(_xinfo.window, this);
	_xcb->xcb_map_window(_connection->getConnection(), _xinfo.window);
	_mapped = true;

	_xcb->xcb_flush(_connection->getConnection());

	// A mapped window is enabled; see the same line in WaylandWindow::mapWindow for why it belongs
	// here and not in init(). A modal Dialog is the only thing that clears the bit again.
	updateState(0, _info->state | WindowState::Enabled);

	configureOutputWindow();

	if (_info->type == WindowType::Popup) {
		auto cookie = _xcb->xcb_grab_pointer(_connection->getConnection(), 0, _xinfo.window,
				XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
						| XCB_EVENT_MASK_POINTER_MOTION,
				XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
		auto reply = _connection->perform(_xcb->xcb_grab_pointer_reply, cookie);
		_popupGrabbed = reply && reply->status == XCB_GRAB_STATUS_SUCCESS;
	}

	if (_info->fullscreen != FullscreenInfo::None) {
		setFullscreen(FullscreenInfo(_info->fullscreen), nullptr, this);
	}
}

void XcbWindow::unmapWindow() {
	_mapped = false;
	if (_popupGrabbed) {
		_xcb->xcb_ungrab_pointer(_connection->getConnection(), XCB_CURRENT_TIME);
		_popupGrabbed = false;
	}
	_xcb->xcb_unmap_window(_connection->getConnection(), _xinfo.window);
	_xcb->xcb_flush(_connection->getConnection());
	_connection->detachWindow(_xinfo.window);
}

bool XcbWindow::close() {
	if (!_xinfo.closed) {
		_xinfo.closed = true;
		if (!_controller->notifyWindowClosed(this)) {
			if (hasFlag(_info->state, WindowState::CloseGuard)) {
				updateState(0, _info->state | WindowState::CloseRequest);
			}
			_xinfo.closed = false;
			return false;
		}
		return true;
	}
	return true;
}

Rc<SoftwareSurface> XcbWindow::makeSoftwareSurface() {
	return Rc<XcbSoftwareSurface>::create(this);
}

void XcbWindow::handleFramePresented(const PresentationFrameInfo &frame) {
	_xcb->xcb_flush(_connection->getConnection());

	if (_pendingExpose) {
		handleExpose(nullptr);
		_pendingExpose = false;
	}

	if (_xinfo.syncCounter && (_xinfo.syncValue.lo != 0 || _xinfo.syncValue.hi != 0)
			&& frame.order > _xinfo.syncFrameOrder) {
		_xcb->xcb_sync_set_counter(_connection->getConnection(), _xinfo.syncCounter,
				_xinfo.syncValue);
		_xcb->xcb_flush(_connection->getConnection());

		_xinfo.syncValue.lo = 0;
		_xinfo.syncValue.hi = 0;
	}

	if (_forcedFrames > 0) {
		--_forcedFrames;
		emitAppFrame();
	}
}

FrameConstraints XcbWindow::exportConstraints(uint64_t &serial) const {
	auto ret = NativeWindow::exportConstraints(serial);

	ret.extent = Extent3(_xinfo.contentRect.width, _xinfo.contentRect.height, 1);
	if (ret.density == 0.0f) {
		ret.density = 1.0f;
	}
	if (_density != 0.0f) {
		ret.density *= _density;
		ret.surfaceDensity = _density;
	}

	ret.frameInterval = 1'000'000'000 / _frameRate;
	return move(ret);
}

Extent2 XcbWindow::getExtent() const {
	return Extent2(_xinfo.contentRect.width, _xinfo.contentRect.height);
}

SurfaceInterfaceInfo XcbWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo ret;
	ret.backend = SurfaceBackend::Xcb;
	ret.xcb.connection = _connection->getConnection();
	ret.xcb.window = _xinfo.outputWindow;
	return ret;
}

bool XcbWindow::enableState(WindowState state) {
	if (NativeWindow::enableState(state)) {
		return true;
	}

	return XcbWindow_updateState(_connection, state, _xinfo.window, true);
}

bool XcbWindow::disableState(WindowState state) {
	if (NativeWindow::disableState(state)) {
		return true;
	}

	return XcbWindow_updateState(_connection, state, _xinfo.window, false);
}

void XcbWindow::startGrip(XcbMoveResize value, int32_t x, int32_t y, int32_t button) {
	if (value != XcbMoveResize::Cancel) {
		cancelPointerEvents();
	}

	xcb_client_message_event_t message;
	message.response_type = XCB_CLIENT_MESSAGE;
	message.format = 32;
	message.sequence = 0;
	message.window = _xinfo.window;

	switch (value) {
	case XcbMoveResize::Menu:
		message.type = _connection->getAtom(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU);
		message.data.data32[0] = 0; // GtkDeviceId
		message.data.data32[1] = x;
		message.data.data32[2] = y;
		break;
	default:
		message.type = _connection->getAtom(XcbAtomIndex::_NET_WM_MOVERESIZE);
		message.data.data32[0] = x;
		message.data.data32[1] = y;
		message.data.data32[2] = toInt(value);
		message.data.data32[3] = button;
		message.data.data32[4] = 1; // EWMH says 1 for normal applications
		break;
	}

	_xcb->xcb_send_event(_connection->getConnection(), 0, _connection->getDefaultScreen()->root,
			XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
			(const char *)&message);
	_xcb->xcb_flush(_connection->getConnection());
}

void XcbWindow::openWindowMenu(Vec2 pos) {
	if (!_connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
		return;
	}

	int32_t rootX, rootY;
	if (pos.isValid()) {
		// pos: content-local, bottom-left origin -> root, top-left
		auto ext = getExtent();
		rootX = _xinfo.boundingRect.x + _xinfo.contentRect.x + int32_t(pos.x);
		rootY = _xinfo.boundingRect.y + _xinfo.contentRect.y
				+ int32_t(int32_t(ext.height) - int32_t(pos.y));
	} else {
		rootX = _lastPointerRootX;
		rootY = _lastPointerRootY;
	}

	startGrip(XcbMoveResize::Menu, rootX, rootY, _lastPointerButton);
}

// X11 has no IME connection here, so this window *is* the IME: the request is always accepted and
// the shared TextInputProcessor does the editing from raw key events. Report enablement, otherwise
// the processor never intercepts the keyboard (see TextInputProcessor::handleInputEnabled).
bool XcbWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) {
	if (_textInput) {
		_textInput->handleInputEnabled(true);
	}
	return true;
}

void XcbWindow::cancelTextInput() {
	if (_textInput) {
		_textInput->handleInputEnabled(false);
	}
}

void XcbWindow::updateWindowAttributes() {
	uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
	uint32_t values[2];
	values[0] = _xinfo.overrideRedirect;
	values[1] = _xinfo.eventMask;

	_xcb->xcb_change_window_attributes(_connection->getConnection(), _xinfo.window, mask, values);
}

void XcbWindow::configureWindow(xcb_rectangle_t r, uint16_t border_width) {
	const uint32_t values[] = {static_cast<uint32_t>(r.x), static_cast<uint32_t>(r.y),
		static_cast<uint32_t>(r.width), static_cast<uint32_t>(r.height), border_width};

	_xcb->xcb_configure_window(_connection->getConnection(), _xinfo.window,
			XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
					| XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
			values);
	_xcb->xcb_flush(_connection->getConnection());
}

uint32_t XcbWindow::getCurrentFrameRate() const {
	uint32_t rate = 0;
	auto currentConfig = _controller.get_cast<LinuxContextController>()
								 ->getDisplayConfigManager()
								 ->getCurrentConfig();
	if (currentConfig) {
		for (auto &it : currentConfig->monitors) {
			rate = sprt::max(rate, it.getCurrent().mode.rate);
		}
	}
	if (!rate) {
		rate = 60'000;
	}
	return rate;
}

Status XcbWindow::setFullscreenState(FullscreenInfo &&info) {
	auto submitMonitorIndex = [&](uint32_t index) {
		xcb_client_message_event_t monitors;
		monitors.response_type = XCB_CLIENT_MESSAGE;
		monitors.format = 32;
		monitors.sequence = 0;
		monitors.window = _xinfo.window;
		monitors.type = _connection->getAtom(XcbAtomIndex::_NET_WM_FULLSCREEN_MONITORS);
		monitors.data.data32[0] = index;
		monitors.data.data32[1] = index;
		monitors.data.data32[2] = index;
		monitors.data.data32[3] = index;
		monitors.data.data32[4] = 1; // EWMH says 1 for normal applications
		_xcb->xcb_send_event(_connection->getConnection(), 0, _connection->getDefaultScreen()->root,
				XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
				(const char *)&monitors);
	};

	auto submitFullscreen = [&](bool enable) {
		XcbWindow_updateState(_connection, WindowState::Fullscreen, _xinfo.window, enable);
	};

	if (info == FullscreenInfo::Current) {
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			return Status::Declined;
		}

		auto cfg = _connection->getDisplayConfigManager()->getCurrentConfig();
		for (auto &it : cfg->monitors) {
			if (it.id.name == _xinfo.outputName) {
				submitMonitorIndex(it.index);
				submitFullscreen(true);

				const unsigned long value = 1;
				if (hasFlag(_info->capabilities, WindowCapabilities::FullscreenExclusive)) {
					auto a = _connection->getAtom(XcbAtomIndex::_NET_WM_BYPASS_COMPOSITOR);
					if (a) {
						_xcb->xcb_change_property(_connection->getConnection(),
								XCB_PROP_MODE_REPLACE, _xinfo.window, a, XCB_ATOM_CARDINAL, 32, 1,
								&value);
						info.flags |= FullscreenFlags::Exclusive;
					}
				}
				_xcb->xcb_flush(_connection->getConnection());

				info.id = it.id;
				info.mode = it.getCurrent().mode;
				_info->fullscreen = move(info);
				return Status::Ok;
			}
		}

		return Status::ErrorInvalidArguemnt;
	}

	auto enable = info != FullscreenInfo::None;
	if (enable) {
		auto cfg = _connection->getDisplayConfigManager()->getCurrentConfig();
		if (!cfg) {
			return Status::ErrorInvalidArguemnt;
		}

		auto mon = cfg->getMonitor(info.id);
		if (!mon) {
			return Status::ErrorInvalidArguemnt;
		}
		submitMonitorIndex(mon->index);
	} else {
		_xcb->xcb_delete_property(_connection->getConnection(), _xinfo.window,
				_connection->getAtom(XcbAtomIndex::_NET_WM_FULLSCREEN_MONITORS));
	}

	submitFullscreen(enable);

	const unsigned long value = 1;
	if (hasFlag(info.flags, FullscreenFlags::Exclusive)
			&& hasFlag(_info->capabilities, WindowCapabilities::FullscreenExclusive)) {
		auto a = _connection->getAtom(XcbAtomIndex::_NET_WM_BYPASS_COMPOSITOR);
		if (a) {
			if (enable) {
				_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
						_xinfo.window, a, XCB_ATOM_CARDINAL, 32, 1, &value);
			} else {
				_xcb->xcb_delete_property(_connection->getConnection(), _xinfo.window, a);
			}
		}
	}

	_xcb->xcb_flush(_connection->getConnection());

	_info->fullscreen = move(info);
	return Status::Ok;
}

xcb_rectangle_t XcbWindow::getContentRect(xcb_rectangle_t boundingRect) const {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)
			&& !hasFlag(_info->state, WindowState::Fullscreen)) {
		auto &theme = _controller->getThemeInfo();
		auto offset = static_cast<uint32_t>(theme.decorations.shadowWidth * _density);

		auto rect = xcb_rectangle_t{
			static_cast<int16_t>(offset + theme.decorations.shadowOffset.x * _density),
			static_cast<int16_t>(offset - theme.decorations.shadowOffset.y * _density),
			static_cast<uint16_t>(boundingRect.width - offset * 2),
			static_cast<uint16_t>(boundingRect.height - offset * 2)};

		auto padding = FrameExtents::getExtents(boundingRect, rect);

		if (hasFlag(_info->state, WindowState::TiledLeft)) {
			rect.x -= static_cast<int16_t>(padding.left);
			rect.width += static_cast<int16_t>(padding.left);
		}
		if (hasFlag(_info->state, WindowState::TiledTop)) {
			rect.y -= static_cast<int16_t>(padding.top);
			rect.height += static_cast<int16_t>(padding.top);
		}
		if (hasFlag(_info->state, WindowState::TiledRight)) {
			rect.width += static_cast<int16_t>(padding.right);
		}
		if (hasFlag(_info->state, WindowState::TiledBottom)) {
			rect.height += static_cast<int16_t>(padding.bottom);
		}
		return rect;
	} else {
		return xcb_rectangle_t{0, 0, boundingRect.width, boundingRect.height};
	}
}

void XcbWindow::updateContentRect(xcb_rectangle_t rect) {
	if (!isEqual(_xinfo.contentRect, rect)) {
		_xinfo.contentRect = rect;
		if (_connection->hasCapability(XcbAtomIndex::_GTK_FRAME_EXTENTS)) {
			auto padding = FrameExtents::getExtents(_xinfo.boundingRect, _xinfo.contentRect);
			_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
					_xinfo.window, _connection->getAtom(XcbAtomIndex::_GTK_FRAME_EXTENTS),
					XCB_ATOM_CARDINAL, 32, 4, &padding);
		}
		configureOutputWindow();
	}
}

void XcbWindow::configureOutputWindow() {
	uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
			| XCB_CONFIG_WINDOW_HEIGHT;
	uint32_t values[8];
	uint32_t idx = 0;

	values[idx++] = _xinfo.contentRect.x;
	values[idx++] = _xinfo.contentRect.y;
	values[idx++] = _xinfo.contentRect.width;
	values[idx++] = _xinfo.contentRect.height;

	_xcb->xcb_configure_window(_connection->getConnection(), _xinfo.outputWindow, mask, values);
	_xcb->xcb_flush(_connection->getConnection());

	_controller->notifyWindowConstraintsChanged(this,
			UpdateConstraintsFlags::DeprecateSwapchain | UpdateConstraintsFlags::SwitchToFastMode);

	if (_xcb->hasShape()) {
		_xcb->xcb_shape_rectangles(_connection->getConnection(), XCB_SHAPE_SO_SET,
				XCB_SHAPE_SK_INPUT, 0, _xinfo.outputWindow, 0, 0, 0, nullptr);
	}
}

void XcbWindow::updateShadows() {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		auto &theme = _controller->getThemeInfo();
		_controller->notifyWindowConstraintsChanged(this, UpdateConstraintsFlags::None);
		generateShadowPixmaps(theme.decorations.shadowWidth * _density,
				theme.decorations.borderRadius * _density);
		emitAppFrame();
		_pendingExpose = true;
	}
}

void XcbWindow::generateShadowPixmaps(uint32_t size, uint32_t inset) {
	if (_xinfo.depth != 32) {
		oslog::vperror(__SPRT_LOCATION, "XcbWindow", "Shadows can be generated only with depth 32");
		return;
	}

	auto width = size + inset;

	auto updateContext = [&](XcbShadowCornerContext &ctx) {
		if (ctx.gc) {
			_xcb->xcb_free_gc(_connection->getConnection(), ctx.gc);
		} else {
			ctx.gc = _xcb->xcb_generate_id(_connection->getConnection());
		}
		if (ctx.pixmap) {
			_xcb->xcb_free_pixmap(_connection->getConnection(), ctx.pixmap);
		} else {
			ctx.pixmap = _xcb->xcb_generate_id(_connection->getConnection());
		}

		ctx.width = width;
		ctx.inset = inset;

		_xcb->xcb_create_pixmap(_connection->getConnection(), _xinfo.depth, ctx.pixmap,
				_xinfo.window, ctx.width, ctx.width);
		_xcb->xcb_create_gc(_connection->getConnection(), ctx.gc, ctx.pixmap, 0, nullptr);
	};

	auto putImage = [&](XcbShadowCornerContext &ctx, const uint8_t *c) {
		_xcb->xcb_put_image(_connection->getConnection(), XCB_IMAGE_FORMAT_Z_PIXMAP, ctx.pixmap,
				ctx.gc, ctx.width, ctx.width, 0, 0, 0, _xinfo.depth, ctx.width * ctx.width * 4, c);
	};

	updateContext(_xinfo.shadowTopLeft);
	updateContext(_xinfo.shadowTopRight);
	updateContext(_xinfo.shadowBottomLeft);
	updateContext(_xinfo.shadowBottomRight);

	Bytes data;
	data.resize(width * width // shadow rect
			* _xinfo.depth / 8 // color components, should be 4
			* 4 // four shadows
	);

	auto ptr = reinterpret_cast<Color4B *>(data.data());

	auto targetA = ptr;
	auto targetB = ptr + width * width;
	auto targetC = ptr + width * width * 2;
	auto targetD = ptr + width * width * 3;

	makeShadowCorner([&](uint32_t i, uint32_t j, float value) {
		auto valueA = (uint8_t)(_shadowCurrentValue * 255.0f * value);
		targetA[i * width + j].a = valueA;
		targetB[(width - i - 1) * width + (width - j - 1)].a = valueA;
		targetC[(i)*width + (width - j - 1)].a = valueA;
		targetD[(width - i - 1) * width + (j)].a = valueA;
	}, width, inset);

	putImage(_xinfo.shadowTopLeft, reinterpret_cast<const uint8_t *>(targetB));
	putImage(_xinfo.shadowTopRight, reinterpret_cast<const uint8_t *>(targetD));
	putImage(_xinfo.shadowBottomLeft, reinterpret_cast<const uint8_t *>(targetC));
	putImage(_xinfo.shadowBottomRight, reinterpret_cast<const uint8_t *>(targetA));
}

void XcbWindow::setCursor(WindowCursor cursor) {
	uint32_t cursorId = _connection->loadCursor(cursor);
	if (_xinfo.cursorId != cursorId) {
		_connection->setCursorId(_xinfo.window, cursorId);
		_xinfo.cursorId = cursorId;
	}
}

void XcbWindow::updateUserTime(uint32_t t) {
	_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, _xinfo.window,
			_connection->getAtom(XcbAtomIndex::_NET_WM_USER_TIME), XCB_ATOM_CARDINAL, 32, 1, &t);
	_lastInputTime = t;
}

void XcbWindow::cancelPointerEvents() {
	for (uint32_t i = 0; i < 64; ++i) {
		if (_buttons.test(i)) {
			InputEventData event({
				i,
				InputEventName::Cancel,
				{{
					InputMouseButton(i),
					InputModifier::None,
					NaN<float>,
					NaN<float>,
				}},
			});
			_pendingEvents.emplace_back(sprt::move(event));
		}
	}
	_buttons.reset();
	_xcb->xcb_ungrab_pointer(_connection->getConnection(), XCB_CURRENT_TIME);
	_buttonGripFlags = WindowLayerFlags::None;
}

} // namespace sprt::window
