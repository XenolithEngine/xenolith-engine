/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#include "MaterialDecoratedLayout.h"
#include "XLDirector.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool DecoratedLayout::init(ColorRole role) {
	if (!SceneLayout2d::init()) {
		return false;
	}

	_decorationMask = DecorationMask::All;

	_decorationRoot = addChild(Rc<Node>::create(), ZOrderMax);

	_decorationTop = _decorationRoot->addChild(
			Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)));
	_decorationTop->setAnchorPoint(Anchor::TopLeft);
	_decorationTop->setVisible(false);
	_decorationTop->setStyleDirtyCallback(
			[this](const SurfaceStyleData &style) { updateStatusBar(style); });

	_decorationBottom = _decorationRoot->addChild(
			Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)));
	_decorationBottom->setAnchorPoint(Anchor::BottomLeft);
	_decorationBottom->setVisible(false);

	_decorationLeft = _decorationRoot->addChild(
			Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)));
	_decorationLeft->setAnchorPoint(Anchor::BottomLeft);
	_decorationLeft->setVisible(false);

	_decorationRight = _decorationRoot->addChild(
			Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)));
	_decorationRight->setAnchorPoint(Anchor::BottomRight);
	_decorationRight->setVisible(false);

	_background = addChild(
			Rc<LayerSurface>::create(SurfaceStyle(ColorRole::Background, NodeStyle::SurfaceTonal)),
			ZOrderMin);
	_background->setStyleDirtyCallback([this](const SurfaceStyleData &data) {
		if (data.shadowValue > 0.0f) {
			setDepthIndex(data.shadowValue);
		}
	});
	_background->setAnchorPoint(Anchor::Middle);

	return true;
}

void DecoratedLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	_decorationRoot->setContentSize(_contentSize);
	_decorationRoot->setDepthIndex(20.0f);

	if (_decorationPadding.left > 0.0f) {
		_decorationLeft->setPosition(Vec2::ZERO);
		_decorationLeft->setContentSize(Size2(_decorationPadding.left, _contentSize.height));
		_decorationLeft->setVisible(true);
	} else {
		_decorationLeft->setVisible(false);
	}

	if (_decorationPadding.right > 0.0f) {
		_decorationRight->setPosition(Vec2(_contentSize.width, 0.0f));
		_decorationRight->setContentSize(Size2(_decorationPadding.right, _contentSize.height));
		_decorationRight->setVisible(true);
	} else {
		_decorationRight->setVisible(false);
	}

	if (_decorationPadding.top > 0.0f) {
		_decorationTop->setPosition(Vec2(_decorationPadding.left, _contentSize.height));
		_decorationTop->setContentSize(Size2(_contentSize.width - _decorationPadding.horizontal(),
				_decorationPadding.top));
		_decorationTop->setVisible(true);
	} else {
		_decorationTop->setVisible(false);
	}

	if (_decorationPadding.bottom > 0.0f) {
		_decorationBottom->setPosition(Vec2(_decorationPadding.left, 0.0f));
		_decorationBottom->setContentSize(Size2(
				_contentSize.width - _decorationPadding.horizontal(), _decorationPadding.bottom));
		_decorationBottom->setVisible(true);
	} else {
		_decorationBottom->setVisible(false);
	}

	_background->setPosition(_contentSize / 2.0f);
	_background->setContentSize(_contentSize);

	updateStatusBar(_decorationTop->getStyleCurrent());
}

bool DecoratedLayout::visitDraw(FrameInfo &info, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto maxDepth = getMaxDepthIndex();
	_decorationRoot->setDepthIndex(maxDepth);

	return SceneLayout2d::visitDraw(info, parentFlags);
}

void DecoratedLayout::setDecorationColorRole(ColorRole role) {
	SurfaceStyle tmp;

	tmp = _decorationLeft->getStyleOrigin();
	tmp.colorRole = role;
	_decorationLeft->setStyle(tmp);
	_decorationRight->setStyle(tmp);
	_decorationTop->setStyle(tmp);
	_decorationBottom->setStyle(tmp);
}

ColorRole DecoratedLayout::getDecorationColorRole() const {
	return _decorationLeft->getStyleTarget().colorRole;
}

void DecoratedLayout::setDecorationElevation(Elevation e) {
	SurfaceStyle tmp;

	tmp = _decorationLeft->getStyleOrigin();
	tmp.elevation = e;
	_decorationLeft->setStyle(tmp);
	_decorationRight->setStyle(tmp);
	_decorationTop->setStyle(tmp);
	_decorationBottom->setStyle(tmp);
}

Elevation DecoratedLayout::getDecorationElevation() const {
	return _decorationLeft->getStyleTarget().elevation;
}

void DecoratedLayout::setViewDecorationFlags(ViewDecorationFlags value) {
	if (_viewDecoration != value) {
		_viewDecoration = value;
	}
}

ViewDecorationFlags DecoratedLayout::getViewDecorationFlags() const { return _viewDecoration; }

void DecoratedLayout::handleForeground(SceneContent2d *l, SceneLayout2d *overlay) {
	updateStatusBar(_decorationTop->getStyleCurrent());
}

void DecoratedLayout::updateStatusBar(const SurfaceStyleData &style) {
	if (_director
			&& (_viewDecoration & ViewDecorationFlags::Visible) != ViewDecorationFlags::None) {
		//_director->getWindow()->setInsetDecorationTone(style.colorOn.data.tone / 50.0f);
	}
}

float DecoratedLayout::getMaxDepthIndex() const {
	float maxIndex = _depthIndex;
	for (auto &it : _children) {
		if (it != _background && it != _decorationRoot) {
			maxIndex = std::max(it->getMaxDepthIndex(), maxIndex);
		}
	}
	return maxIndex;
}

} // namespace stappler::xenolith::material2d
