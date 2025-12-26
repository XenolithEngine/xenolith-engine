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

#include "XLRTCommonView.h"
#include "XL2dScrollController.h"
#include "XLEventListener.h"

#include "XLAction.h"
#include "XL2dActionAcceleratedMove.h"
#include "XLDirector.h"
#include "XLRTCommonView.h"
#include "XLRTRenderer.h"
#include "XL2dLabel.h"
#include "XL2dVectorSprite.h"
#include "XLTemporaryResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

constexpr uint16_t ImageFillerWidth = 312;
constexpr uint16_t ImageFillerHeight = 272;

constexpr auto ImageFillerData =
		R"ImageFillerSvg(<svg xmlns="http://www.w3.org/2000/svg" height="272" width="312" version="1.1">
	<rect x="0" y="0" width="312" height="272" opacity="0.25"/>
	<g transform="translate(0 -780.4)">
		<path d="m104 948.4h104l-32-56-24 32-16-12zm-32-96v128h168v-128h-168zm16 16h136v96h-136v-96zm38 20a10 10 0 0 1 -10 10 10 10 0 0 1 -10 -10 10 10 0 0 1 10 -10 10 10 0 0 1 10 10z" fill-rule="evenodd" fill="#ccc"/>
	</g>
</svg>)ImageFillerSvg";

constexpr uint16_t ImageVideoWidth = 128;
constexpr uint16_t ImageVideoHeight = 128;

constexpr auto ImageVideoData =
		R"ImageVideoData(<svg xmlns="http://www.w3.org/2000/svg" height="128" width="128" version="1.1">
	<circle cx="64" cy="64" r="64"/>
	<path fill="#fff" d="m96.76 64-51.96 30v-60z"/>
</svg>)ImageVideoData";

constexpr float ImageVideoSize = 72.0f;

bool CommonObject::init(RendererResult *res, document::Object *obj) {
	if (!basic2d::Sprite::init()) {
		return false;
	}

	_object = obj;
	_result = res;

	setAnchorPoint(Anchor::TopLeft);
	setPosition(obj->bbox.origin);
	setContentSize(_object->bbox.size);

	return true;
}

void CommonObject::handleEnter(Scene *scene) {
	basic2d::Sprite::handleEnter(scene);

	switch (_object->type) {
	case document::Object::Type::Label: initAsLabel((document::Label *)_object); break;
	case document::Object::Type::Background:
		initAsBackground((document::Background *)_object);
		break;
	case document::Object::Type::Path: initAsPath((document::PathObject *)_object); break;
	case document::Object::Type::Ref: std::cout << "Ref\n"; break;
	case document::Object::Type::Empty: std::cout << "Empty\n"; break;
	}
}

void CommonObject::handleContentSizeDirty() {
	Sprite::handleContentSizeDirty();

	if (_pathSprite) {
		_pathSprite->setPosition(Vec2::ZERO);
		_pathSprite->setContentSize(_contentSize);
	}

	if (_overlay) {
		auto size = Size2(std::min(_contentSize.width, ImageVideoSize),
				std::min(_contentSize.height, ImageVideoSize));
		_overlay->setContentSize(size);
		_overlay->setPosition(_contentSize / 2.0f);
	}
}

bool CommonObject::initAsLabel(document::Label *label) {
	auto source = _director->getApplication()->getExtension<font::FontController>();

	setNormalized(true);
	setColorMode(core::ColorMode::AlphaChannel);
	setRenderingLevel(RenderingLevel::Surface);
	setSamplerIndex(core::SamplerIndex::DefaultFilterNearest);

	auto el = Rc<EventListener>::create();
	el->listenForEventWithObject(font::FontController::onFontSourceUpdated, source,
			[this](const Event &) { _vertexesDirty = true; });

	if (source->isLoaded()) {
		setTexture(Rc<Texture>(source->getTexture()));
	} else {
		el->listenForEventWithObject(font::FontController::onLoaded, source,
				[this, source](const Event &) {
			setTexture(Rc<Texture>(source->getTexture()));
			_vertexesDirty = true;
		}, true);
	}
	return true;
}

bool CommonObject::initAsBackground(document::Background *bg) {
	auto tryLoadFromDocument = [&] {
		auto img = _result->document->getImage(bg->background.backgroundImage);
		if (img && img->ct == "image/svg") {
			_pathSprite = addChild(Rc<basic2d::VectorSprite>::create(img->data));
			_pathSprite->setTextureAutofit(basic2d::Autofit::Contain);
			_vertexesVisible = false;
			return true;
		}
		return false;
	};

	if (!bg->background.backgroundImage.empty()) {
		if (_result->resource) {
			if (_result->resource->textures.find(bg->background.backgroundImage)
					!= _result->resource->textures.end()) {
				if (auto tex = _result->resource->resource->acquireTexture(
							bg->background.backgroundImage)) {
					setTexture(move(tex));
					setSamplerIndex(core::SamplerIndex::DefaultFilterLinear);
				}
			} else {
				auto it = _result->resource->svgs.find(bg->background.backgroundImage);
				if (it != _result->resource->svgs.end()) {
					_pathSprite =
							addChild(Rc<basic2d::VectorSprite>::create(StringView(it->second)));
					_pathSprite->setTextureAutofit(basic2d::Autofit::Contain);
					_vertexesVisible = false;
				} else if (!tryLoadFromDocument()) {
					// replace with filler
					_pathSprite = addChild(
							Rc<basic2d::VectorSprite>::create(StringView(ImageFillerData)));
					_pathSprite->setTextureAutofit(basic2d::Autofit::Contain);
				}
			}
		} else {
			if (!tryLoadFromDocument()) {
				// replace with filler
				_pathSprite =
						addChild(Rc<basic2d::VectorSprite>::create(StringView(ImageFillerData)));
				_pathSprite->setTextureAutofit(basic2d::Autofit::Contain);
			}
		}
	} else {
		setColor(Color4F(bg->background.backgroundColor), true);
	}
	if (bg->link && bg->link->mode == "video") {
		_overlay =
				addChild(Rc<basic2d::VectorSprite>::create(StringView(ImageVideoData)), ZOrder(1));
		_overlay->setTextureAutofit(basic2d::Autofit::Contain);
		_overlay->setAnchorPoint(Anchor::Middle);
		_overlay->setRenderingLevel(RenderingLevel::Transparent);
	}
	return true;
}

bool CommonObject::initAsPath(document::PathObject *path) {
	vg::VectorPath vpath;
	vpath.init(path->path);

	_pathSprite = addChild(Rc<basic2d::VectorSprite>::create(path->bbox.size, move(vpath)));
	_pathSprite->setTextureAutofit(basic2d::Autofit::Contain);
	_pathSprite->setRenderingLevel(RenderingLevel::Solid);

	return true;
}

void CommonObject::updateLabel(document::Label *label) {
	Vector<ColorMask> colorMap;

	auto fc = _director->getApplication()->getExtension<font::FontController>();

	for (auto &it : label->layout.ranges) {
		auto dep = fc->addTextureChars(it.layout,
				SpanView<font::CharLayoutData>(label->layout.chars, it.start, it.count));
		if (dep) {
			emplace_ordered(_pendingDependencies, move(dep));
		}
	}

	basic2d::Label::writeQuads(_vertexes, &label->layout, colorMap, _textureLayer);
}

void CommonObject::updateVertexes(FrameInfo &frame) {
	_vertexes.clear();

	switch (_object->type) {
	case document::Object::Type::Label: updateLabel((document::Label *)_object); break;
	case document::Object::Type::Background:
		if (_vertexesVisible) {
			Sprite::updateVertexes(frame);
		}
		break;
	case document::Object::Type::Path: break;
	case document::Object::Type::Ref: break;
	case document::Object::Type::Empty: break;
	}
}

void CommonObject::updateColor() { _vertexColorDirty = false; }

void CommonObject::updateVertexesColor() { }

void CommonObject::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	Sprite::pushCommands(frame, flags);
}

CommonView::~CommonView() { }

bool CommonView::init(Layout l, CommonSource *source, Vector<String> &&ids) {
	if (!ScrollView::init(l)) {
		return false;
	}

	setController(onScrollController());

	_renderer = addSystem(Rc<Renderer>::create(sp::move(ids)));
	_renderer->setRenderingCallback(
			std::bind(&CommonView::onRenderer, this, std::placeholders::_1, std::placeholders::_2));

	_background = addChild(Rc<basic2d::Layer>::create(Color4B(238, 238, 238, 255)));
	_background->setAnchorPoint(Vec2(0, 0));
	_background->setPosition(Vec2(0, 0));

	_pageMargin = Padding(8.0f, 8.0f);
	_scrollSpaceLimit = 720.0f;
	_scrollSpacePadding = 8.0f;

	if (source) {
		setSource(source);
	}

	return true;
}

void CommonView::setLayout(Layout l) {
	if (l != _layout) {
		_savedRelativePosition = getScrollRelativePosition();
		_controller->clear();
		ScrollView::setLayout(l);
	}
}

Vec2 CommonView::convertToObjectSpace(Vec2 vec) const {
	if (_layout == Vertical) {
		auto loc = _root->convertToNodeSpace(vec);
		loc.y = _root->getContentSize().height - loc.y - _objectsOffset;
		return loc;
	} else {
		/*for (auto &node : _root->getChildren()) {
			if (node->isTouched(vec)) {
				auto page = dynamic_cast<Page *>(node);
				if (page) {
					return page->convertToObjectSpace(vec);
				}
			}
		}*/
	}
	return Vec2::ZERO;
}

bool CommonView::isObjectActive(const Object &obj) const {
	return obj.type == document::Object::Type::Ref;
}

bool CommonView::isObjectTapped(Vec2 loc, const Object &obj) const {
	if (obj.type == document::Object::Type::Ref) {
		if (loc.x >= obj.bbox.getMinX() - 8.0f && loc.x <= obj.bbox.getMaxX() + 8.0f
				&& loc.y >= obj.bbox.getMinY() - 8.0f && loc.y <= obj.bbox.getMaxY() + 8.0f) {
			return true;
		}
		return false;
	} else {
		return obj.bbox.containsPoint(loc);
	}
}

void CommonView::onObjectPressBegin(Vec2, const Object &obj) { }

void CommonView::onObjectPressEnd(Vec2, const Object &obj) { }

void CommonView::onSwipeBegin() {
	ScrollView::onSwipeBegin();
	_gestureStart = getScrollPosition();
}

bool CommonView::onPressBegin(Vec2 vec) {
	ScrollView::onPressBegin(vec);
	auto res = _renderer->getResult();
	if (!res) {
		return true;
	}
	auto refs = res->result->getRefs();
	if (refs.empty()) {
		return true;
	}

	if (_linksEnabled) {
		auto loc = convertToObjectSpace(vec);
		for (auto &it : refs) {
			if (isObjectTapped(loc, *it)) {
				onObjectPressBegin(vec, *it);
				return true;
			}
		}
	}

	return true;
}

bool CommonView::onPressEnd(Vec2 vec, TimeInterval r) {
	ScrollView::onPressEnd(vec, r);
	if (_layout == Horizontal && _movement == Movement::None) {
		_gestureStart = nan();
		onSwipe(0.0f, 0.0f, true);
	}

	auto res = _renderer->getResult();
	if (!res) {
		return false;
	}
	auto refs = res->result->getRefs();
	if (refs.empty()) {
		return false;
	}

	if (_linksEnabled) {
		auto loc = convertToObjectSpace(vec);
		for (auto &it : refs) {
			if (isObjectActive(*it)) {
				if (isObjectTapped(loc, *it)) {
					onObjectPressEnd(vec, *it);
					return true;
				}
			}
		}
	}

	return false;
}

bool CommonView::onPressCancel(Vec2 vec, TimeInterval r) {
	ScrollView::onPressCancel(vec, r);
	return false;
}

void CommonView::handleContentSizeDirty() {
	if (_layout == Layout::Horizontal) {
		if (!_renderer->hasFlag(document::RenderFlags::PaginatedLayout)) {
			_renderer->addFlag(document::RenderFlags::PaginatedLayout);
		}
	} else {
		if (_renderer->hasFlag(document::RenderFlags::PaginatedLayout)) {
			_renderer->removeFlag(document::RenderFlags::PaginatedLayout);
		}
	}

	auto m =
			(_layout == Layout::Horizontal) ? _pageMargin : Padding(_paddingGlobal.top, 0.0f, 0.0f);
	//m.top += (_paddingGlobal.top);
	_renderer->setPageMargin(m);

	ScrollView::handleContentSizeDirty();
	_background->setContentSize(getContentSize());

	_gestureStart = nan();
}

void CommonView::setSource(CommonSource *source) {
	if (_source != source) {
		_source = source;
		_renderer->setSource(_source);
	}
}

CommonSource *CommonView::getSource() const { return _source; }

Renderer *CommonView::getRenderer() const { return _renderer; }

Document *CommonView::getDocument() const { return _renderer->getDocument(); }
RendererResult *CommonView::getResult() const { return _renderer->getResult(); }

void CommonView::refresh() {
	if (_source) {
		_source->refresh();
	}
}

void CommonView::setResultCallback(const ResultCallback &cb) { _callback = cb; }

const CommonView::ResultCallback &CommonView::getResultCallback() const { return _callback; }

void CommonView::onRenderer(RendererResult *res, bool) {
	if (res) {
		auto color = Color4F(res->result->getBackgroundColor());

		_background->setColor(color);
		_background->setOpacity(color.a);
		_background->setVisible(color.a > 0);

		_controller->clear();
		_root->removeAllChildren();

		emplaceResultData(res, 0.0f);

		if (_callback) {
			_callback(res);
		}

		if (_running) {
			_controller->onScrollPosition();
		}
	}
}

void CommonView::emplaceResultData(RendererResult *res, float contentOffset) {
	auto surface = res->result->getSurfaceSize();
	if (surface.height == 0) {
		surface.height = res->result->getContentSize().height;
	}
	auto &media = res->result->getMedia();
	if ((media.flags & document::RenderFlags::PaginatedLayout) != document::RenderFlags::None) {
		/*auto num = res->getNumPages();
		bool isSplitted = (media.flags & document::RenderFlags::SplitPages) != document::RenderFlags::None;
		auto page = Size2(surface.width + res->getMedia().pageMargin.horizontal(), surface.height + res->getMedia().pageMargin.vertical());
		for (size_t i = 0; i < num; ++ i) {
			_controller->addItem(std::bind(&CommonView::onPageNode, this, i), page.width, page.width * i);
		}

		if (isSplitted && num % 2 == 1) {
			_controller->addItem(std::bind(&CommonView::onPageNode, this, num), page.width, page.width * num);
		}*/
		_objectsOffset = 0;
	} else {
		if (isnan(contentOffset)) {
			contentOffset = _controller->getNextItemPosition();
		}

		for (auto &obj : res->result->getObjects()) {
			// std::cout << "Obj: " << int(toInt(obj->type)) << " " << obj->bbox << " " << obj->zIndex << "\n";
			_controller->addItem(
					[obj, res = Rc<RendererResult>(res)](
							const basic2d::ScrollController::Item &item) {
				return Rc<CommonObject>::create(res, obj);
			},
					obj->bbox.size, obj->bbox.origin + Vec2(0.0f, contentOffset),
					ZOrder(obj->zIndex));
		}

		_controller->addItem(
				[color = res->result->getBackgroundColor()](
						const basic2d::ScrollController::Item &item) {
			return Rc<basic2d::Layer>::create(Color4F(color));
		},
				Size2(res->result->getContentSize()) + Size2(_scrollSpacePadding * 2.0f, 0.0f),
				Vec2(-_scrollSpacePadding, contentOffset), ZOrder(-1));

		_objectsOffset = contentOffset;
	}
}

PageData CommonView::getPageData(size_t idx) const {
	return _renderer->getResult()->result->getPageData(idx, _objectsOffset);
}

Rc<ActionInterval> CommonView::onSwipeFinalizeAction(float velocity) {
	if (_layout == Layout::Vertical) {
		return ScrollView::onSwipeFinalizeAction(velocity);
	}

	float acceleration = (velocity > 0) ? -3500.0f : 3500.0f;
	float boundary = (velocity > 0) ? _scrollMax : _scrollMin;

	//Vec2 normal = (isVertical())
	//		?(Vec2(0.0f, (velocity > 0)?1.0f:-1.0f))
	//		:(Vec2((velocity > 0)?-1.0f:1.0f, 0.0f));

	ActionInterval *a = nullptr;

	if (!isnan(_maxVelocity)) {
		if (velocity > fabs(_maxVelocity)) {
			velocity = fabs(_maxVelocity);
		} else if (velocity < -fabs(_maxVelocity)) {
			velocity = -fabs(_maxVelocity);
		}
	}

	float pos = getScrollPosition();
	float duration = fabsf(velocity / acceleration);
	float path = velocity * duration + acceleration * duration * duration * 0.5f;
	float newPos = pos + path;
	float segment = _contentSize.width;
	int num = roundf(newPos / segment);

	if (!isnan(_gestureStart)) {
		int prev = roundf(_gestureStart / segment);
		if (num - prev < -1) {
			num = prev - 1;
		}
		if (num - prev > 1) {
			num = prev + 1;
		}
		_gestureStart = nan();
	}
	newPos = num * segment;

	if (!isnan(boundary)) {
		auto from = _root->getPosition().xy();
		auto to = getPointForScrollPosition(boundary);

		float distance = from.distance(to);
		if (distance < 2.0f) {
			setScrollPosition(boundary);
			return nullptr;
		}

		if ((velocity > 0 && newPos > boundary) || (velocity < 0 && newPos < boundary)) {
			_movementAction = basic2d::ActionAcceleratedMove::createAccelerationTo(from, to,
					fabsf(velocity), -fabsf(acceleration));

			auto overscrollPath = path + ((velocity < 0) ? (distance) : (-distance));
			if (fabs(overscrollPath) < std::numeric_limits<float>::epsilon()) {
				auto cb = Rc<CallFunc>::create(
						std::bind(&CommonView::onOverscroll, this, overscrollPath));
				a = Rc<Sequence>::create(_movementAction, cb);
			}
		}
	}

	if (!_movementAction) {
		auto from = _root->getPosition();
		auto to = getPointForScrollPosition(newPos);

		return basic2d::ActionAcceleratedMove::createBounce(fabsf(acceleration), from.xy(), to,
				Vec2(-velocity, 0.0f), 25'000);
	}

	if (!a) {
		a = _movementAction;
	}

	return a;
}

void CommonView::setScrollRelativePosition(float value) {
	if (isVertical()) {
		ScrollView::setScrollRelativePosition(value);
		return;
	}

	if (!isnan(value)) {
		if (value < 0.0f) {
			value = 0.0f;
		} else if (value > 1.0f) {
			value = 1.0f;
		}
	} else {
		value = 0.0f;
	}

	float areaSize = getScrollableAreaSize();
	float areaOffset = getScrollableAreaOffset();

	auto &padding = getPadding();
	auto paddingFront = (isVertical()) ? padding.top : padding.left;
	auto paddingBack = (isVertical()) ? padding.bottom : padding.right;

	if (!isnan(areaSize) && !isnan(areaOffset) && areaSize > 0) {
		float liveSize = areaSize + paddingFront + paddingBack;
		float pos = (value * liveSize) - paddingFront + areaOffset;
		float segment = _contentSize.width;

		int num = roundf(pos / segment);
		if (num < 0) {
			num = 0;
		}

		setScrollPosition(num * segment);
	} else {
		_savedRelativePosition = value;
	}
}

bool CommonView::showNextPage() {
	if (isHorizontal() && !isMoved() && !_animationAction) {
		_movement = Movement::Auto;
		_animationAction = Rc<Sequence>::create(
				basic2d::ActionAcceleratedMove::createBounce(7'500, _root->getPosition().xy(),
						_root->getPosition().xy() - Vec2(_contentSize.width, 0), Vec2(0, 0)),
				[this] { onAnimationFinished(); });
		_root->runAction(_animationAction);
		return true;
	}
	return false;
}

bool CommonView::showPrevPage() {
	if (isHorizontal() && !isMoved() && !_animationAction) {
		_movement = Movement::Auto;
		_animationAction = Rc<Sequence>::create(
				basic2d::ActionAcceleratedMove::createBounce(7'500, _root->getPosition().xy(),
						_root->getPosition().xy() + Vec2(_contentSize.width, 0), Vec2(0, 0)),
				[this] { onAnimationFinished(); });
		_root->runAction(_animationAction);
		return true;
	}
	return false;
}

float CommonView::getObjectsOffset() const { return _objectsOffset; }

void CommonView::setLinksEnabled(bool value) { _linksEnabled = value; }
bool CommonView::isLinksEnabled() const { return _linksEnabled; }

void CommonView::onPosition() {
	ScrollView::onPosition();
	if (_movement == Movement::None) {
		_gestureStart = nan();
	}
}

Rc<basic2d::ScrollController> CommonView::onScrollController() const {
	return Rc<basic2d::ScrollController>::create();
}

} // namespace stappler::xenolith::richtext
