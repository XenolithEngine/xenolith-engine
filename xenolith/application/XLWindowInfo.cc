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
#include "SPBitmap.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Value encodeWindowInfo(const WindowInfo &info) {
	Value ret;
	ret.setString(info.id, "id");
	ret.setString(info.title, "title");
	ret.setString(getWindowTypeName(info.type), "type");
	if (!info.parent.empty()) {
		ret.setString(info.parent, "parent");
	}
	ret.setValue(
			Value{
				Value(info.rect.x),
				Value(info.rect.y),
				Value(info.rect.width),
				Value(info.rect.height),
			},
			"rect");

	ret.setValue(
			Value{
				Value(info.decorationInsets.top),
				Value(info.decorationInsets.left),
				Value(info.decorationInsets.bottom),
				Value(info.decorationInsets.right),
			},
			"decoration");

	if (info.density) {
		ret.setDouble(info.density, "density");
	}

	ret.setString(core::getImageFormatName(info.imageFormat), "imageFormat");
	ret.setString(core::getColorSpaceName(info.colorSpace), "colorSpace");
	ret.setString(core::getPresentModeName(info.preferredPresentMode), "preferredPresentMode");

	Value f;
	if (hasFlag(info.flags, WindowCreationFlags::Modal)) {
		f.addString("Modal");
	}

	if (hasFlag(info.flags, WindowCreationFlags::DirectOutput)) {
		f.addString("DirectOutput");
	}

	if (hasFlag(info.flags, WindowCreationFlags::PreferNativeDecoration)) {
		f.addString("PreferNativeDecoration");
	}

	if (hasFlag(info.flags, WindowCreationFlags::PreferServerSideDecoration)) {
		f.addString("PreferServerSideDecoration");
	}

	if (hasFlag(info.flags, WindowCreationFlags::PreferServerSideCursors)) {
		f.addString("PreferServerSideCursors");
	}

	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}

	if (info.icon) {
		// Sizes and name only: the pixels have no business in a data::Value the inspector will
		// serialize, and the sizes are what actually tells you whether the icon is usable.
		Value icon;
		if (!info.icon->name.empty()) {
			icon.setString(info.icon->name, "name");
		}
		Value sizes;
		for (auto &it : info.icon->images) { sizes.addInteger(it.extent.width); }
		if (!sizes.empty()) {
			icon.setValue(move(sizes), "sizes");
		}
		ret.setValue(move(icon), "icon");
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

SpanView<uint32_t> getDefaultWindowIconSizes() {
	static constexpr uint32_t s_sizes[] = {16, 24, 32, 48, 64, 128, 256};
	return SpanView<uint32_t>(s_sizes, sizeof(s_sizes) / sizeof(s_sizes[0]));
}

// Copy `bmp` (already RGBA8888, straight alpha) into a tightly packed WindowIconImage.
// The Bitmap's rows can be padded, so this can not be a single memcpy of data().
static WindowIconImage WindowIcon_makeImage(const mem_std::Bitmap &bmp) {
	WindowIconImage img;
	img.extent = Extent2(bmp.width(), bmp.height());
	img.data.resize(img.getDataSize());

	auto rowSize = size_t(bmp.width()) * 4;
	for (uint32_t y = 0; y < bmp.height(); ++y) {
		memcpy(img.data.data() + y * rowSize, bmp.dataPtr() + y * bmp.stride(), rowSize);
	}
	return img;
}

Rc<WindowIcon> makeWindowIcon(BytesView imageData, SpanView<uint32_t> sizes) {
	if (imageData.empty()) {
		return nullptr;
	}

	mem_std::Bitmap bmp(imageData);
	if (!bmp) {
		log::error("WindowIcon", "Fail to decode icon image");
		return nullptr;
	}

	if (!bmp.convert(bitmap::PixelFormat::RGBA8888)) {
		log::error("WindowIcon", "Fail to convert icon image to RGBA8888");
		return nullptr;
	}

	// WindowIconImage is defined as straight alpha. A decoder normally hands us exactly that, but
	// normalize rather than trust it: premultiplied data taken as straight darkens every
	// semi-transparent pixel a second time when the backend premultiplies again.
	if (bmp.alpha() == bitmap::AlphaFormat::Premultiplied) {
		auto ptr = bmp.dataPtr();
		for (uint32_t y = 0; y < bmp.height(); ++y) {
			auto row = ptr + y * bmp.stride();
			for (uint32_t x = 0; x < bmp.width(); ++x, row += 4) {
				auto a = row[3];
				if (a == 0 || a == 255) {
					continue;
				}
				for (uint32_t c = 0; c < 3; ++c) {
					row[c] = uint8_t(sprt::min(uint32_t(255), (uint32_t(row[c]) * 255 + a / 2) / a));
				}
			}
		}
	}

	// A non-square source is center-cropped to its shorter side: every consumer of this type
	// requires square rasters, and cropping beats the letterbox a resample to square would give.
	if (bmp.width() != bmp.height()) {
		auto side = sprt::min(bmp.width(), bmp.height());
		auto offX = (bmp.width() - side) / 2;
		auto offY = (bmp.height() - side) / 2;

		mem_std::Bitmap cropped;
		cropped.alloc(side, side, bitmap::PixelFormat::RGBA8888,
				bitmap::AlphaFormat::Unpremultiplied);
		for (uint32_t y = 0; y < side; ++y) {
			memcpy(cropped.dataPtr() + y * cropped.stride(),
					bmp.dataPtr() + (y + offY) * bmp.stride() + size_t(offX) * 4,
					size_t(side) * 4);
		}
		bmp = sp::move(cropped);
	}

	if (sizes.empty()) {
		sizes = getDefaultWindowIconSizes();
	}

	auto ret = Rc<WindowIcon>::alloc();
	auto source = bmp.width();

	for (auto &size : sizes) {
		if (size > source) {
			// Skip rather than upscale: the window system chooses from what it is given, and a
			// blurry raster it might pick is worse than one size fewer to choose from.
			continue;
		}
		if (size == source) {
			continue; // emitted below, from the source itself
		}
		auto scaled = bmp.resample(size, size);
		if (scaled) {
			ret->images.emplace_back(WindowIcon_makeImage(scaled));
		}
	}

	// Always emit the source's own size, so a source smaller than every requested size still
	// produces a usable icon instead of an empty one.
	ret->images.emplace_back(WindowIcon_makeImage(bmp));

	if (ret->images.empty()) {
		return nullptr;
	}
	return ret;
}

Rc<WindowIcon> makeWindowIcon(const FileInfo &path, SpanView<uint32_t> sizes) {
	auto data = filesystem::readIntoMemory<mem_std::Interface>(path);
	if (data.empty()) {
		log::error("WindowIcon", "Fail to read icon file: ", path);
		return nullptr;
	}
	return makeWindowIcon(BytesView(data), sizes);
}

} // namespace stappler::xenolith
