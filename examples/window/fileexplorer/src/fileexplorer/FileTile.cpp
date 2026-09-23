/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "fileexplorer/FileTile.h"
#include "fileexplorer/IconGridView.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// Where the pictures of a directory tile sit on the folder's face, as fractions of the icon box.
struct PreviewSlot {
	float x;
	float y;
	float side;
};

static constexpr PreviewSlot s_previewSlots[] = {
	{0.10f, 0.12f, 0.34f},
	{0.36f, 0.12f, 0.34f},
	{0.62f, 0.12f, 0.34f},
};

} // namespace

bool FileTile::init(IconGridView *view, ThumbnailCache *cache) {
	if (!Panel::init()) {
		return false;
	}

	_view = view;
	_cache = cache;

	setType("file-tile");
	removeStyleClass("xl-ui-panel");
	// Without this a `background-color` on the new type falls through to the node tint.
	ui::Panel::registerStyleAppliers("file-tile");
	setAnchorPoint(Anchor::BottomLeft);

	_icon = addChild(Rc<basic2d::IconSprite>::create(basic2d::IconName::Empty), ZOrder(1));
	_icon->setAnchorPoint(Anchor::BottomLeft);
	_icon->addStyleClass("fe-tile-icon");

	_thumb = addChild(Rc<basic2d::Sprite>::create(), ZOrder(2));
	_thumb->setAnchorPoint(Anchor::BottomLeft);
	_thumb->setTextureAutofit(basic2d::Autofit::Contain);
	_thumb->setVisible(false);

	for (size_t i = 0; i < ThumbnailCache::FolderPreviewCount; ++i) {
		auto preview = addChild(Rc<basic2d::Sprite>::create(), ZOrder(3 + int16_t(i)));
		preview->setAnchorPoint(Anchor::BottomLeft);
		preview->setTextureAutofit(basic2d::Autofit::Cover);
		preview->setVisible(false);
		_previews.emplace_back(preview);
	}

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(9));
	_label->setAnchorPoint(Anchor::TopLeft);
	_label->setAlignment(font::TextAlign::Center);
	_label->setMaxLines(2);
	_label->addStyleClass("fe-tile-label");

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			setOrUpdateComponent<InteractiveComponent>(
					[](NotNull<InteractiveComponent> state) { return state->handleHover(1); });
			break;
		case GestureEvent::Activated: break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			setOrUpdateComponent<InteractiveComponent>(
					[](NotNull<InteractiveComponent> state) { return state->handleHover(-1); });
			break;
		}
		return true;
	}, false);

	// Up to two taps, so a double tap can reach the activate path; Immediate reports the first
	// one without waiting out the double-tap interval.
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated && _view) {
			_view->handleTileTap(_index, tap.count);
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 2, InputTapFlags::Immediate});

	return true;
}

void FileTile::update(ui::FilesystemModel::Node *node, size_t index, float iconSize) {
	_index = index;

	auto ref = ui::FilesystemModel::getFileRef(node);
	auto path = ref ? StringView(ref->path) : StringView();
	auto mtime = ref ? int64_t(ref->stat.mtime.toMicros()) : int64_t(0);

	const auto sameEntry = (_node == node && _path == path && _mtime == mtime);
	if (sameEntry && _iconSize == iconSize) {
		return;
	}

	_node = node;
	_iconSize = iconSize;

	if (!sameEntry) {
		++_epoch;
		_path = path.str<Interface>();
		_name = ref ? ref->name : String();
		_mtime = mtime;
		_dir = ref ? ref->dir : false;
		applyEntry();
	}

	markContentSizeDirty();
}

void FileTile::applyEntry() {
	_label->setString(_name);
	_label->setLocaleEnabled(false); // a file name is content, whatever it happens to start with

	_icon->setIconName(ui::FilesystemModel::getIcon(_node));
	_icon->setVisible(true);
	// A folder and a file are told apart by colour, which is the sheet's to decide.
	if (_dir) {
		_icon->addStyleClass("dir");
	} else {
		_icon->removeStyleClass("dir");
	}

	_thumb->setVisible(false);
	_thumb->setTexture(Rc<Texture>(nullptr));

	for (auto &it : _previews) {
		it->setVisible(false);
		it->setTexture(Rc<Texture>(nullptr));
	}

	if (!_cache || _path.empty()) {
		return;
	}

	if (_dir) {
		requestFolderPreview();
	} else {
		requestThumbnail();
	}
}

void FileTile::requestThumbnail() {
	Rc<FileTile> self(this);
	const auto epoch = _epoch;
	_cache->requestThumbnail(_path, _mtime, [self, epoch](Rc<Texture> &&texture) {
		if (self->_epoch == epoch) {
			self->showThumbnail(sp::move(texture));
		}
	});
}

void FileTile::requestFolderPreview() {
	Rc<FileTile> self(this);
	const auto epoch = _epoch;
	_cache->requestFolderPreview(_path, _mtime,
			[self, epoch](Vector<ThumbnailCache::PreviewRef> &&refs) {
		if (self->_epoch != epoch) {
			return;
		}
		for (size_t i = 0; i < refs.size() && i < self->_previews.size(); ++i) {
			// The preview pictures go through the same cache as the file tiles, so a directory
			// and the images inside it share one texture each.
			self->_cache->requestThumbnail(refs[i].path, refs[i].mtime,
					[self, epoch, i](Rc<Texture> &&texture) {
				if (self->_epoch == epoch) {
					self->showPreview(i, sp::move(texture));
				}
			});
		}
	});
}

void FileTile::showThumbnail(Rc<Texture> &&texture) {
	if (!texture) {
		return; // the icon stays: a file that does not decode is still a file
	}

	// The callback is installed before setTexture: a cached texture is already loaded, and then
	// setTexture fires it inside itself.
	_thumb->setTextureLoadedCallback([this] {
		_thumb->setVisible(true);
		_icon->setVisible(false);
	});
	_thumb->setTexture(sp::move(texture));
	markContentSizeDirty();
}

void FileTile::showPreview(size_t slot, Rc<Texture> &&texture) {
	if (!texture || slot >= _previews.size()) {
		return;
	}

	auto preview = _previews[slot];
	preview->setTextureLoadedCallback([preview] { preview->setVisible(true); });
	preview->setTexture(sp::move(texture));
	markContentSizeDirty();
}

void FileTile::setSelected(bool value) {
	if (_selected == value) {
		return;
	}
	_selected = value;
	if (value) {
		addStyleClass("selected");
	} else {
		removeStyleClass("selected");
	}
}

void FileTile::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	auto size = getContentSize();
	if (size.width <= 0.0f || size.height <= 0.0f) {
		return;
	}

	const float pad = 4.0f;
	const float iconBox = sprt::min(_iconSize, size.width - pad * 2.0f);
	const float iconLeft = (size.width - iconBox) / 2.0f;
	const float iconBottom = size.height - pad - iconBox;

	_icon->setPosition(Vec2(iconLeft, iconBottom));
	_icon->setContentSize(Size2(iconBox, iconBox));

	_thumb->setPosition(Vec2(iconLeft, iconBottom));
	_thumb->setContentSize(Size2(iconBox, iconBox));

	for (size_t i = 0; i < _previews.size() && i < sizeof(s_previewSlots) / sizeof(PreviewSlot);
			++i) {
		auto &slot = s_previewSlots[i];
		auto side = iconBox * slot.side;
		_previews[i]->setPosition(
				Vec2(iconLeft + iconBox * slot.x, iconBottom + iconBox * slot.y));
		_previews[i]->setContentSize(Size2(side, side));
	}

	_label->setPosition(Vec2(pad, iconBottom - pad));

	/* Where a line wraps and where it is cut: a name with no break opportunity only obeys the cut,
	and an overflowing line is not centred. The cut is one em wider than the wrap, since the
	formatter tests it while pushing a glyph and the wrap only after - equal widths never wrap. */
	const float text = size.width - pad * 2.0f;
	_label->setWidth(sprt::max(text - _label->getFontSize().val(), 0.0f));
	_label->setMaxWidth(text);
}

} // namespace stappler::xenolith::examples
