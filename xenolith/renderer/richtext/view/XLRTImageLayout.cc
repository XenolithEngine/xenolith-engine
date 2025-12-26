/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLRTImageLayout.h"
#include "XLRTRenderer.h"
#include "XLTemporaryResource.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

/*
ImageLayout::~ImageLayout() { }

bool ImageLayout::init(RendererResult *res, const StringView &id, const StringView &src, const StringView &alt) {
	if (!DecoratedLayout::init() || !res || !res->resource) {
		return false;
	}

	_result = res;
	_src = src.str<Interface>();

	_toolbar = addChild(Rc<material2d::AppBar>::create(material2d::AppBarLayout::Minified), ZOrder(2));
	_toolbar->setNodeStyle(material2d::NodeStyle::Filled);
	_toolbar->setNavButtonIcon(IconName::Dynamic_Nav, 1.0f);
	_toolbar->setNavCallback([this] {
		close();
	});
	_toolbar->setAnchorPoint(Anchor::TopLeft);

	_sprite = addChild(Rc<material2d::ImageLayer>::create(), ZOrder(1));
	_sprite->setPosition(Vec2(0, 0));
	_sprite->setAnchorPoint(Vec2(0, 0));
	_sprite->setActionCallback([this] { });

	if (auto tex = _result->resource->resource->acquireTexture(_src)) {
		_sprite->setTexture(move(tex));
	}

	return true;
}

void ImageLayout::handleContentSizeDirty() {
	DecoratedLayout::handleContentSizeDirty();

	_toolbar->setPosition(Vec2(_decorationPadding.left, _contentSize.height - _decorationPadding.top));
	_toolbar->setContentSize(Size2(_contentSize.width - _decorationPadding.horizontal(), 32.0f));

	_sprite->setContentSize(Size2(_contentSize.width - _decorationPadding.horizontal(), _contentSize.height - 32.0f - _decorationPadding.vertical()));
	_sprite->setPosition(Vec2(_decorationPadding.left, _decorationPadding.bottom));
}

void ImageLayout::handleEnter(Scene *scene) {
	DecoratedLayout::handleEnter(scene);
}

void ImageLayout::close() {
	auto contentLayer = dynamic_cast<material2d::SceneContent2d *>(_parent);
	if (contentLayer) {
		contentLayer->popLayout(this);
	}
}
*/

}
