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

#include "XLLinuxXcbWindow.h"
#include "linux/XLLinuxContextController.h"
#include "XLLinuxXcbConnection.h"
#include "XLLinuxXcbDisplayConfigManager.h"
#include "platform/XLContextNativeWindow.h"

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

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static core::InputModifier getModifiers(uint32_t mask) {
	core::InputModifier ret = core::InputModifier::None;
	core::InputModifier *mod,
			mods[] = {core::InputModifier::Shift, core::InputModifier::CapsLock,
				core::InputModifier::Ctrl, core::InputModifier::Alt, core::InputModifier::NumLock,
				core::InputModifier::Mod3, core::InputModifier::Mod4, core::InputModifier::Mod5,
				core::InputModifier::Button1, core::InputModifier::Button2,
				core::InputModifier::Button3, core::InputModifier::Button4,
				core::InputModifier::Button5, core::InputModifier::LayoutAlternative};
	for (mod = mods; mask; mask >>= 1, mod++) {
		if (mask & 1) {
			ret |= *mod;
		}
	}
	return ret;
}

static core::InputMouseButton getButton(xcb_button_t btn) { return core::InputMouseButton(btn); }

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
	if (!NativeWindow::init(c, move(info), conn->getCapabilities())) {
		return false;
	}

	_connection = conn;

	if (_connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
		_info->state |= WindowState::AllowedWindowMenu;
	}

	_xcb = _connection->getXcb();

	if (_connection->hasErrors()) {
		return false;
	}

	StringView bundleName = _controller->getContext()->getInfo()->bundleName;

	_wmClass.resize(_info->title.size() + bundleName.size() + 1, char(0));
	memcpy(_wmClass.data(), _info->title.data(), _info->title.size());
	memcpy(_wmClass.data() + _info->title.size() + 1, bundleName.data(), bundleName.size());

	_defaultScreen = _connection->getDefaultScreen();

	_xinfo.parent = _defaultScreen->root;
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
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

	_xinfo.overrideRedirect = 0;
	_xinfo.overrideClose = true;
	_xinfo.enableSync = true;

	auto udpi = _connection->getUnscaledDpi();
	auto dpi = _connection->getDpi();

	_density = float(dpi) / float(udpi);

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
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

	if (!_connection->createWindow(_info, _xinfo)) {
		log::source().error("XCB", "Fail to create window");
		return false;
	}

	_xcb->xcb_map_window(_connection->getConnection(), _xinfo.outputWindow);

	_frameRate = getCurrentFrameRate();

	auto windowType = _connection->getAtom(XcbAtomIndex::_NET_WM_WINDOW_TYPE_NORMAL);

	_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, _xinfo.window,
			_connection->getAtom(XcbAtomIndex::_NET_WM_WINDOW_TYPE), XCB_ATOM_ATOM, 32, 1,
			&windowType);

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

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		hints.decorations = 0;
	}

	_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, _xinfo.window,
			_connection->getAtom(XcbAtomIndex::_MOTIF_WM_HINTS), XCB_ATOM_CARDINAL, 32, 5, &hints);

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
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
		if (reply) {
			XL_X11_LOG("XcbWindow: handlePropertyNotify _NET_WM_DESKTOP: %d %ld",
					*((int32_t *)_xcb->xcb_get_property_value(reply)),
					_xcb->xcb_get_property_value_length(reply) / sizeof(int32_t));
		}
	} else if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_FRAME_EXTENTS)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, ev->atom, XCB_ATOM_CARDINAL, 0, 32);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			SP_UNUSED auto values = (uint32_t *)_xcb->xcb_get_property_value(reply);

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
		SP_UNUSED auto name = _connection->getAtomName(ev->atom);
		XL_X11_LOG("XcbWindow: handlePropertyNotify: %s", name.data());
	}
}

void XcbWindow::handleButtonPress(xcb_button_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);
	auto btn = getButton(ev->detail);

	if (btn == core::InputMouseButton::MouseLeft) {
		// Capture current grip flags
		if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuLeft)
				&& _connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
			startGrip(XcbMoveResize::Menu, ev->root_x, ev->root_y, ev->detail);
			return;
		} else {
			_buttonGripFlags = _gripFlags;
		}
	} else if (btn == core::InputMouseButton::MouseRight) {
		if (hasFlag(_currentLayerFlags, WindowLayerFlags::WindowMenuRight)
				&& _connection->hasCapability(XcbAtomIndex::_GTK_SHOW_WINDOW_MENU)) {
			startGrip(XcbMoveResize::Menu, ev->root_x, ev->root_y, ev->detail);
			return;
		}
	}

	_buttons.set(ev->detail);

	core::InputEventData event({
		ev->detail,
		core::InputEventName::Begin,
		{{
			btn,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	switch (btn) {
	case core::InputMouseButton::MouseScrollUp:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = 0.0f;
		event.point.valueY = 10.0f;
		break;
	case core::InputMouseButton::MouseScrollDown:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = 0.0f;
		event.point.valueY = -10.0f;
		break;
	case core::InputMouseButton::MouseScrollLeft:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = 10.0f;
		event.point.valueY = 0.0f;
		break;
	case core::InputMouseButton::MouseScrollRight:
		event.event = core::InputEventName::Scroll;
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

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);
	auto btn = getButton(ev->detail);

	if (btn == core::InputMouseButton::MouseLeft) {
		// Release current grip flags
		_buttonGripFlags = WindowLayerFlags::None;
	}

	if (_buttons.test(ev->detail)) {
		_buttons.reset(ev->detail);

		core::InputEventData event({
			ev->detail,
			core::InputEventName::End,
			{{
				btn,
				mod,
				float(ev->event_x - _xinfo.contentRect.x),
				float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
			}},
		});

		switch (btn) {
		case core::InputMouseButton::MouseScrollUp:
		case core::InputMouseButton::MouseScrollDown:
		case core::InputMouseButton::MouseScrollLeft:
		case core::InputMouseButton::MouseScrollRight: break;
		default: _pendingEvents.emplace_back(event); break;
		}
	} else {
		startGrip(XcbMoveResize::Cancel, ev->root_x, ev->root_y,
				toInt(core::InputMouseButton::MouseLeft));
	}
}

void XcbWindow::handleMotionNotify(xcb_motion_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	if (_buttonGripFlags != WindowLayerFlags::None) {
		if (_buttons.test(toInt(core::InputMouseButton::MouseLeft)) && _buttons.count() == 1) {
			auto grip = _buttonGripFlags;
			startGrip(getMoveResizeForGrip(grip), ev->root_x, ev->root_y,
					toInt(core::InputMouseButton::MouseLeft));
			return;
		}
	}

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);

	core::InputEventData event({
		maxOf<uint32_t>(),
		core::InputEventName::MouseMove,
		{{
			core::InputMouseButton::None,
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

	updateState(ev->time, _info->state | WindowState::Pointer);

	auto ext = getExtent();
	auto mod = getModifiers(ev->state);

	core::InputEventData event({
		maxOf<uint32_t>(),
		core::InputEventName::MouseMove,
		{{
			core::InputMouseButton::None,
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

void XcbWindow::handleFocusIn(xcb_focus_in_event_t *ev) { _forcedFrames += 2; }

void XcbWindow::handleFocusOut(xcb_focus_out_event_t *ev) { _forcedFrames += 2; }

void XcbWindow::handleKeyPress(xcb_key_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	auto mod = getModifiers(ev->state);
	auto ext = getExtent();

	// in case of key autorepeat, ev->time will match
	// just replace event name from previous InputEventName::KeyReleased to InputEventName::KeyRepeated
	if (!_pendingEvents.empty()
			&& _pendingEvents.back().event == core::InputEventName::KeyReleased) {
		auto &iev = _pendingEvents.back();
		if (iev.id == ev->time && iev.getModifiers() == mod && iev.input.x == float(ev->event_x)
				&& iev.input.y == float(ext.height - ev->event_y)
				&& iev.key.keysym == _connection->getKeysym(ev->detail, ev->state, false)) {
			iev.event = core::InputEventName::KeyRepeated;
			return;
		}
	}

	core::InputEventData event({
		ev->time,
		core::InputEventName::KeyPressed,
		{{
			core::InputMouseButton::None,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	_connection->fillTextInputData(event, ev->detail, ev->state, isTextInputEnabled(), true);

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleKeyRelease(xcb_key_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		updateUserTime(ev->time);
	}

	auto mod = getModifiers(ev->state);
	auto ext = getExtent();

	core::InputEventData event({
		ev->time,
		core::InputEventName::KeyReleased,
		{{
			core::InputMouseButton::None,
			mod,
			float(ev->event_x - _xinfo.contentRect.x),
			float(int32_t(ext.height) - (ev->event_y - _xinfo.contentRect.y)),
		}},
	});

	_connection->fillTextInputData(event, ev->detail, ev->state, isTextInputEnabled(), false);

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleSyncRequest(xcb_timestamp_t syncTime, xcb_sync_int64_t value) {
	_lastSyncTime = syncTime;
	_xinfo.syncValue = value;
	_xinfo.syncFrameOrder = _frameOrder;
}

void XcbWindow::handleCloseRequest() { _controller->notifyWindowClosed(this); }

void XcbWindow::notifyScreenChange() {
	auto newFrameRate = getCurrentFrameRate();
	if (newFrameRate != _frameRate) {
		_frameRate = newFrameRate;
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
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
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
	}
}

xcb_connection_t *XcbWindow::getConnection() const { return _connection->getConnection(); }

void XcbWindow::mapWindow() {
	_connection->attachWindow(_xinfo.window, this);
	_xcb->xcb_map_window(_connection->getConnection(), _xinfo.window);

	_xcb->xcb_flush(_connection->getConnection());

	configureOutputWindow();

	if (_info->fullscreen != FullscreenInfo::None) {
		setFullscreen(FullscreenInfo(_info->fullscreen), nullptr, this);
	}
}

void XcbWindow::unmapWindow() {
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

void XcbWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	_xcb->xcb_flush(_connection->getConnection());

	if (_pendingExpose) {
		handleExpose(nullptr);
		_pendingExpose = false;
	}

	if (_xinfo.syncCounter && (_xinfo.syncValue.lo != 0 || _xinfo.syncValue.hi != 0)
			&& frame->getFrameOrder() > _xinfo.syncFrameOrder) {
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

core::FrameConstraints XcbWindow::exportConstraints() const {
	auto ret = NativeWindow::exportConstraints();

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

Rc<core::Surface> XcbWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());
	auto connection = getConnection();

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkXcbSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr, 0,
		connection, _xinfo.outputWindow};
	if (instance->vkCreateXcbSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::source().error("XcbWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
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

bool XcbWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) { return true; }

void XcbWindow::cancelTextInput() { }

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
			rate = std::max(rate, it.getCurrent().mode.rate);
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
						info.flags |= core::FullscreenFlags::Exclusive;
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
			core::UpdateConstraintsFlags::DeprecateSwapchain
					| core::UpdateConstraintsFlags::SwitchToFastMode);

	if (_xcb->hasShape()) {
		_xcb->xcb_shape_rectangles(_connection->getConnection(), XCB_SHAPE_SO_SET,
				XCB_SHAPE_SK_INPUT, 0, _xinfo.outputWindow, 0, 0, 0, nullptr);
	}
}

void XcbWindow::updateShadows() {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		auto &theme = _controller->getThemeInfo();
		_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);
		generateShadowPixmaps(theme.decorations.shadowWidth * _density,
				theme.decorations.borderRadius * _density);
		emitAppFrame();
		_pendingExpose = true;
	}
}

void XcbWindow::generateShadowPixmaps(uint32_t size, uint32_t inset) {
	if (_xinfo.depth != 32) {
		log::source().error("XcbWindow", "Shadows can be generated only with depth 32");
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
			core::InputEventData event({
				i,
				core::InputEventName::Cancel,
				{{
					core::InputMouseButton(i),
					core::InputModifier::None,
					nan(),
					nan(),
				}},
			});
			_pendingEvents.emplace_back(sp::move(event));
		}
	}
	_buttons.reset();
	_xcb->xcb_ungrab_pointer(_connection->getConnection(), XCB_CURRENT_TIME);
	_buttonGripFlags = WindowLayerFlags::None;
}

} // namespace stappler::xenolith::platform
