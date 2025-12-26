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

#include "XLWindowInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Value WindowInfo::encode() const {
	Value ret;
	ret.setString(id, "id");
	ret.setString(title, "title");
	ret.setValue(
			Value{
				Value(rect.x),
				Value(rect.y),
				Value(rect.width),
				Value(rect.height),
			},
			"rect");

	ret.setValue(
			Value{
				Value(decorationInsets.top),
				Value(decorationInsets.left),
				Value(decorationInsets.bottom),
				Value(decorationInsets.right),
			},
			"decoration");

	if (density) {
		ret.setDouble(density, "density");
	}

	ret.setString(core::getImageFormatName(imageFormat), "imageFormat");
	ret.setString(core::getColorSpaceName(colorSpace), "colorSpace");
	ret.setString(core::getPresentModeName(preferredPresentMode), "preferredPresentMode");

	Value f;
	if (hasFlag(flags, WindowCreationFlags::DirectOutput)) {
		f.addString("DirectOutput");
	}

	if (hasFlag(flags, WindowCreationFlags::PreferNativeDecoration)) {
		f.addString("PreferNativeDecoration");
	}

	if (hasFlag(flags, WindowCreationFlags::PreferServerSideDecoration)) {
		f.addString("PreferServerSideDecoration");
	}

	if (hasFlag(flags, WindowCreationFlags::PreferServerSideCursors)) {
		f.addString("PreferServerSideCursors");
	}

	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

StringView getWindowCursorName(WindowCursor cursor) {
	switch (cursor) {
	case WindowCursor::Undefined: return StringView("Undefined"); break;
	case WindowCursor::Default: return StringView("Default"); break;
	case WindowCursor::ContextMenu: return StringView("ContextMenu"); break;
	case WindowCursor::Help: return StringView("Help"); break;
	case WindowCursor::Pointer: return StringView("Pointer"); break;
	case WindowCursor::Progress: return StringView("Progress"); break;
	case WindowCursor::Wait: return StringView("Wait"); break;
	case WindowCursor::Cell: return StringView("Cell"); break;
	case WindowCursor::Crosshair: return StringView("Crosshair"); break;
	case WindowCursor::Text: return StringView("Text"); break;
	case WindowCursor::VerticalText: return StringView("VerticalText"); break;
	case WindowCursor::Alias: return StringView("Alias"); break;
	case WindowCursor::Copy: return StringView("Copy"); break;
	case WindowCursor::Move: return StringView("Move"); break;
	case WindowCursor::NoDrop: return StringView("NoDrop"); break;
	case WindowCursor::NotAllowed: return StringView("NotAllowed"); break;
	case WindowCursor::Grab: return StringView("Grab"); break;
	case WindowCursor::Grabbing: return StringView("Grabbing"); break;
	case WindowCursor::AllScroll: return StringView("AllScroll"); break;
	case WindowCursor::ZoomIn: return StringView("ZoomIn"); break;
	case WindowCursor::ZoomOut: return StringView("ZoomOut"); break;
	case WindowCursor::DndAsk: return StringView("DndAsk"); break;
	case WindowCursor::RightPtr: return StringView("RightPtr"); break;
	case WindowCursor::Pencil: return StringView("Pencil"); break;
	case WindowCursor::Target: return StringView("Target"); break;
	case WindowCursor::ResizeRight: return StringView("ResizeRight"); break;
	case WindowCursor::ResizeTop: return StringView("ResizeTop"); break;
	case WindowCursor::ResizeTopRight: return StringView("ResizeTopRight"); break;
	case WindowCursor::ResizeTopLeft: return StringView("ResizeTopLeft"); break;
	case WindowCursor::ResizeBottom: return StringView("ResizeBottom"); break;
	case WindowCursor::ResizeBottomRight: return StringView("ResizeBottomRight"); break;
	case WindowCursor::ResizeBottomLeft: return StringView("ResizeBottomLeft"); break;
	case WindowCursor::ResizeLeft: return StringView("ResizeLeft"); break;
	case WindowCursor::ResizeLeftRight: return StringView("ResizeLeftRight"); break;
	case WindowCursor::ResizeTopBottom: return StringView("ResizeTopBottom"); break;
	case WindowCursor::ResizeTopRightBottomLeft:
		return StringView("ResizeTopRightBottomLeft");
		break;
	case WindowCursor::ResizeTopLeftBottomRight:
		return StringView("ResizeTopLeftBottomRight");
		break;
	case WindowCursor::ResizeCol: return StringView("ResizeCol"); break;
	case WindowCursor::ResizeRow: return StringView("ResizeRow"); break;
	case WindowCursor::ResizeAll: return StringView("ResizeAll"); break;
	case WindowCursor::Max: break;
	}
	return StringView();
}

} // namespace stappler::xenolith
