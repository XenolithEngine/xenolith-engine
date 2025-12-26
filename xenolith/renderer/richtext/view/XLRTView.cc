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

#include "XLRTView.h"
#include "XLRTTooltip.h"
#include "XLRTImageLayout.h"
#include "XLEventListener.h"
#include "XLRTRenderer.h"
#include "XL2dLinearProgress.h"

#include "XLScene.h"
#include "XL2dSceneContent.h"

#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

XL_DECLARE_EVENT_CLASS(View, onImageLink);
XL_DECLARE_EVENT_CLASS(View, onContentLink);
XL_DECLARE_EVENT_CLASS(View, onError);
XL_DECLARE_EVENT_CLASS(View, onDocument);
XL_DECLARE_EVENT_CLASS(View, onLayout);

View::~View() { }

bool View::init(CommonSource *source, Vector<String> &&ids) {
	return init(ListenerView::Vertical, source, sp::move(ids));
}

bool View::init(Layout l, CommonSource *source, Vector<String> &&ids) {
	if (!ListenerView::init(Layout::Vertical, nullptr, sp::move(ids))) {
		return false;
	}

	_pageMargin = Padding(2.0f, 6.0f, 12.0f);

	_progress = addChild(Rc<basic2d::LinearProgress>::create());
	_progress->setAnchorPoint(Vec2(0.0f, 1.0f));
	_progress->setPosition(Vec2(0.0f, 0.0f));
	_progress->setLineColor(Color::Blue_500);
	_progress->setLineOpacity(56.0f / 256.0f);
	_progress->setBarColor(Color::Blue_500);
	_progress->setBarOpacity(1.0f);
	_progress->setVisible(false);
	_progress->setAnimated(false);

	_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};

	_highlight = addChild(Rc<Highlight>::create(this));

	_eventListener = addSystem(Rc<EventListener>::create());

	if (source) {
		setSource(source);
	}

	updateProgress();

	if (_renderer) {
		_renderer->setEnabled(_renderingEnabled);
	}

	return true;
}

void View::onPosition() {
	_highlight->setPosition(_root->getPosition());
	_highlight->setContentSize(_root->getContentSize());
	ListenerView::onPosition();
}

void View::onRestorePosition(document::LayoutResult *res, float pos) {
	if (_layoutChanged
			|| (!isnan(_savedFontScale) && _savedFontScale != res->getMedia().fontScale)) {
		setViewPosition(_savedPosition);
	} else if (!isnan(_savedRelativePosition) && _savedRelativePosition != 0.0f) {
		_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};
		setScrollRelativePosition(_savedRelativePosition);
	} else if (!isnan(pos) && pos != 0.0f) {
		_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};
		setScrollRelativePosition(pos);
	}
}

void View::onRenderer(RendererResult *res, bool status) {
	_highlight->setDirty();
	auto pos = getScrollRelativePosition();
	if (!isnan(pos) && pos != 0.0f) {
		_savedRelativePosition = pos;
	}
	if (_renderSize != Size2::ZERO && _savedPosition.object == maxOf<size_t>()) {
		_savedPosition = getViewPosition();
		_renderSize = Size2::ZERO;
	}

	ListenerView::onRenderer(res, status);

	if (status) {
		_rendererUpdate = true;
		updateProgress();
	} else {
		_rendererUpdate = false;
		updateProgress();
	}

	if (res) {
		_renderSize = _contentSize;
		_savedRelativePosition = nan();
		updateScrollBounds();
		onRestorePosition(res->result, pos);
		_savedFontScale = res->result->getMedia().fontScale;
		_layoutChanged = false;
		onDocument(this);
	}
}

void View::handleContentSizeDirty() {
	if (_renderSize != Size2::ZERO && _renderSize != _contentSize) {
		_savedPosition = getViewPosition();
		_renderSize = Size2::ZERO;
		_layoutChanged = true;
	}

	ListenerView::handleContentSizeDirty();
	if (getLayout() == Horizontal) {
		_progress->setPosition(Vec2(0.0f, _contentSize.height));
	}
	_progress->setContentSize(Size2(_contentSize.width, 6.0f));
	_highlight->setDirty();
}

void View::setLayout(Layout l) {
	if (_renderSize != Size2::ZERO) {
		_savedPosition = getViewPosition();
		_renderSize = Size2::ZERO;
		_layoutChanged = true;
	}

	ListenerView::setLayout(l);
	_highlight->setDirty();
	onLayout(this);
}

void View::setOverscrollFrontOffset(float value) {
	ListenerView::setOverscrollFrontOffset(value);
	_progress->setPosition(Vec2(0.0f, _contentSize.height - value));
}

void View::setSource(CommonSource *source) {
	if (_source) {
		if (_sourceErrorListener) {
			_eventListener->removeDelegate(_sourceErrorListener);
		}
		if (_sourceUpdateListener) {
			_eventListener->removeDelegate(_sourceUpdateListener);
		}
		if (_sourceAssetListener) {
			_eventListener->removeDelegate(_sourceAssetListener);
		}
	}
	ListenerView::setSource(source);
	if (_source) {
		_source->setEnabled(_renderingEnabled);
		_sourceErrorListener = _eventListener->listenForEventWithObject(CommonSource::onError,
				_source, [this](const Event &ev) {
			onSourceError(CommonSource::Error(ev.getDataValue().asInteger()));
		});
		_sourceUpdateListener = _eventListener->listenForEventWithObject(CommonSource::onUpdate,
				_source, std::bind(&View::onSourceUpdate, this));
		_sourceAssetListener = _eventListener->listenForEventWithObject(CommonSource::onDocument,
				_source, std::bind(&View::onSourceAsset, this));
	}
}

void View::setProgressColor(const Color &color) { _progress->setBarColor(color); }

void View::onLink(StringView ref, StringView target, WideStringView text, Vec2 vec) {
	if (ref.front() == '#') {
		if (target == "_self") {
			onPositionRef(StringView(ref.data() + 1, ref.size() - 1), false);
		} else if (target == "table") {
			onTable(ref);
		} else {
			onId(ref, target, text, vec);
		}
		return;
	}

	StringView r(ref);
	if (r.is("http://") || r.is("https://") || r.is("mailto:") || r.is("tel:")) {
		ListenerView::onLink(ref, target, text, vec);
	} else if (r.is("gallery://")) {
		r.offset("gallery://"_len);
		StringView name = r.readUntil<StringView::Chars<':'>>();
		if (r.is(':')) {
			++r;
			onGallery(name, r, vec);
		} else {
			onGallery(name, StringView(), vec);
		}
	} else {
		onContentLink(this, ref);
		onFile(ref, vec);
	}
}

void View::onId(StringView ref, StringView target, WideStringView text, Vec2 vec) {
	auto doc = _renderer->getDocument();
	if (!doc) {
		return;
	}

	Vector<String> ids;
	string::split(ref, ",", [&ids](const StringView &r) { ids.push_back(r.str<Interface>()); });

	for (auto &it : ids) {
		if (it.front() == '#') {
			it = it.substr(1);
		}
	}

	if (ids.empty()) {
		return;
	}

	if (ids.size() == 1) {
		auto node = doc->getNodeByIdGlobal(ids.front());

		if (!node.first || !node.second) {
			return;
		}

		if (node.second->getHtmlName() == "figure") {
			onFigure(node.second);
			return;
		}

		auto attrIt = node.second->getAttributes().find("x-type");
		if (node.second->getHtmlName() == "img"
				|| (attrIt != node.second->getAttributes().end() && attrIt->second == "image")) {
			onImage(ids.front(), vec);
			return;
		}
	}
}

void View::onImage(StringView id, Vec2) {
	auto node = _source->getDocument()->getNodeByIdGlobal(id);

	if (!node.first || !node.second) {
		return;
	}

	StringView src = _renderer->getLegacyBackground(*node.second, "image-view"), alt;
	if (src.empty() && node.second->getHtmlName() == "img") {
		auto &attr = node.second->getAttributes();
		auto srcIt = attr.find("src");
		if (srcIt != attr.end()) {
			src = srcIt->second;
		}
	}

	if (src.empty()) {
		auto &nodes = node.second->getNodes();
		for (auto &it : nodes) {
			StringView legacyImage = _renderer->getLegacyBackground(*it, "image-view");
			auto &attr = it->getAttributes();
			if (!legacyImage.empty()) {
				src = legacyImage;
			} else if (it->getHtmlName() == "img") {
				auto srcIt = attr.find("src");
				if (srcIt != attr.end()) {
					src = srcIt->second;
				}
			}

			if (!src.empty()) {
				auto altIt = attr.find("alt");
				if (altIt != attr.end()) {
					alt = altIt->second;
				}
				break;
			}
		}
	} else {
		auto &attr = node.second->getAttributes();
		auto altIt = attr.find("alt");
		if (altIt != attr.end()) {
			alt = altIt->second;
		}
	}
}

void View::onGallery(StringView name, StringView image, Vec2) { }

void View::onContentFile(const StringView &str) { onFile(str, Vec2(0.0f, 0.0f)); }

void View::setRenderingEnabled(bool value) {
	if (_renderingEnabled != value) {
		_renderingEnabled = value;
		if (_renderer) {
			_renderer->setEnabled(value);
		}
		if (_source) {
			_source->setEnabled(value);
		}
	}
}

void View::clearHighlight() { _highlight->clearSelection(); }
void View::addHighlight(const Pair<SelectionPosition, SelectionPosition> &p) {
	_highlight->addSelection(p);
}
void View::addHighlight(const SelectionPosition &first, const SelectionPosition &second) {
	addHighlight(pair(first, second));
}

float View::getBookmarkScrollPosition(size_t objIdx, uint32_t pos, bool inView) const {
	float ret = nan();
	auto res = _renderer->getResult();
	if (res) {
		if (auto obj = res->result->getObject(objIdx)) {
			if (auto label = obj->asLabel()) {
				auto line = label->layout.getLine(pos);
				if (line) {
					ret = ((obj->bbox.origin.y
							+ (line->pos - line->height) / res->result->getMedia().density));
				}
			}
			ret = obj->bbox.origin.y;
		}
		if (!isnan(ret) && inView) {
			if ((res->result->getMedia().flags & document::RenderFlags::PaginatedLayout)
					== document::RenderFlags::None) {
				ret -= res->result->getMedia().pageMargin.top;
			}
			ret += _objectsOffset;

			if (ret > getScrollMaxPosition()) {
				ret = getScrollMaxPosition();
			} else if (ret < getScrollMinPosition()) {
				ret = getScrollMinPosition();
			}
		}
	}
	return ret;
}

float View::getBookmarkScrollRelativePosition(size_t objIdx, uint32_t pos, bool inView) const {
	float ret = nan();
	auto res = _renderer->getResult();
	if (res) {
		if (auto obj = res->result->getObject(objIdx)) {
			if (auto label = obj->asLabel()) {
				auto line = label->layout.getLine(pos);
				if (line) {
					ret = ((obj->bbox.origin.y
								  + (line->pos - line->height) / res->result->getMedia().density))
							/ res->result->getContentSize().height;
				}
			}
			ret = (obj->bbox.origin.y) / res->result->getContentSize().height;
		}
		if (!isnan(ret) && inView) {
			auto s = _objectsOffset + ret * res->result->getContentSize().height;
			ret = s / getScrollLength();
		}
	}
	return ret;
}

void View::onFile(StringView str, Vec2 pos) { onPositionRef(str, false); }

void View::onPositionRef(StringView str, bool middle) {
	auto res = _renderer->getResult();
	if (res) {
		auto indexPoint = res->result->getIndexPoint(str);
		if (indexPoint) {
			float pos = indexPoint->y;
			if ((res->result->getMedia().flags & document::RenderFlags::PaginatedLayout)
					!= document::RenderFlags::None) {
				int num = roundf(pos / res->result->getSurfaceSize().height);
				if (_renderer->isPageSplitted()) {
					if (_positionCallback) {
						_positionCallback((num / 2) * _renderSize.width);
					}
					setScrollPosition((num / 2) * _renderSize.width);
				} else {
					if (_positionCallback) {
						_positionCallback(num * _renderSize.width);
					}
					setScrollPosition(num * _renderSize.width);
				}
			} else {
				if (middle) {
					pos -= getScrollSize() * 0.35f;
				}
				pos -= _paddingGlobal.top;
				pos += _objectsOffset;
				if (pos > getScrollMaxPosition()) {
					pos = getScrollMaxPosition();
				} else if (pos < getScrollMinPosition()) {
					pos = getScrollMinPosition();
				}
				if (_positionCallback) {
					_positionCallback(pos);
				}
				setScrollPosition(pos);
			}
		}
	}
}

void View::onFigure(const document::Node *node) {
	auto &attr = node->getAttributes();
	auto &nodes = node->getNodes();
	auto it = attr.find("type");
	if (it == attr.end() || it->second == "image") {
		StringView source, alt;
		const document::Node *caption = nullptr;
		for (auto &it : nodes) {
			if (it->getHtmlName() == "img") {
				auto &nattr = it->getAttributes();
				auto srcAttr = nattr.find("src");
				if (srcAttr != nattr.end()) {
					source = srcAttr->second;
				}

				srcAttr = nattr.find("alt");
				if (srcAttr != nattr.end()) {
					alt = srcAttr->second;
				}
			} else if (it->getHtmlName() == "figcaption") {
				caption = it;
			}
		}
		if (!source.empty()) {
			onImageFigure(source, alt, caption);
		}
	} else if (it->second == "video") {
		for (auto &it : nodes) {
			if (it->getHtmlName() == "img") {
				auto &nattr = it->getAttributes();
				auto srcAttr = nattr.find("src");
				if (srcAttr != nattr.end()) {
					onVideoFigure(srcAttr->second);
				}
			}
		}
	}
}

void View::onImageFigure(StringView src, StringView alt, const document::Node *node) {
	/*Rc<ImageLayout> image;
	if (!node || node->getHtmlId().empty()) {
		image = Rc<ImageLayout>::create(_renderer->getResult(), StringView(), src, alt);
	} else {
		image = Rc<ImageLayout>::create(_renderer->getResult(), node->getHtmlId(), src, alt);
	}
	if (image) {
		auto content = dynamic_cast<material2d::SceneContent2d *>(_scene->getContent());
		if (content) {
			content->pushLayout(image);
		}
	}*/
}

void View::onVideoFigure(StringView src) { _director->getApplication()->openUrl(src); }

void View::onTable(StringView src) {
	if (!src.empty()) {
		/*Rc<TableView> view = Rc<TableView>::create(_source, _renderer->getMedia(), src);
		if (view) {
			material::Scene::getRunningScene()->pushContentNode(view);
		}*/
	}
}

void View::onSourceError(CommonSource::Error err) {
	onError(this, err);
	updateProgress();
}

void View::onSourceUpdate() { updateProgress(); }

void View::onSourceAsset() { updateProgress(); }

void View::updateProgress() {
	if (_progress) {
		if (!_renderingEnabled || !_source) {
			_progress->setVisible(false);
		} else {
			auto a = _source->getAsset();
			if (a && a->isDownloadInProgress()) {
				if (auto tmp = dynamic_cast<SourceNetworkAsset *>(a)) {
					if (auto tmpa = tmp->getAsset()) {
						_progress->setVisible(true);
						_progress->setAnimated(false);
						_progress->setProgress(tmpa->getProgress());
					} else {
						_progress->setVisible(true);
						_progress->setAnimated(true);
					}
				} else {
					_progress->setVisible(true);
					_progress->setAnimated(true);
				}
			} else if (_source->isDocumentLoading() || _rendererUpdate) {
				_progress->setVisible(true);
				_progress->setAnimated(true);
			} else {
				_progress->setVisible(false);
				_progress->setAnimated(false);
			}
		}
	}
}

View::ViewPosition View::getViewObjectPosition(float pos) const {
	ViewPosition ret{maxOf<size_t>(), 0.0f, pos};
	auto res = _renderer->getResult();
	if (res) {
		size_t objectId = 0;
		auto objs = res->result->getObjects();
		for (auto &it : objs) {
			if (it->bbox.origin.y > pos) {
				ret.object = objectId;
				break;
			}

			if (it->bbox.origin.y <= pos && it->bbox.origin.y + it->bbox.size.height >= pos) {
				ret.object = objectId;
				ret.position =
						(it->bbox.origin.y + it->bbox.size.height - pos) / it->bbox.size.height;
				break;
			}

			objectId++;
		}
	}
	return ret;
}

/*Rc<View::Page> View::onConstructPageNode(const PageData &data, float density) {
	return Rc<PageWithLabel>::create(data, density);
}*/

float View::getViewContentPosition(float pos) const {
	if (isnan(pos)) {
		pos = getScrollPosition();
	}
	auto res = _renderer->getResult();
	if (res) {
		auto flags = res->result->getMedia().flags;
		if ((flags & document::RenderFlags::PaginatedLayout) != document::RenderFlags::None) {
			float segment =
					((flags & document::RenderFlags::SplitPages) != document::RenderFlags::None)
					? _renderSize.width / 2.0f
					: _renderSize.width;
			int num = roundf(pos / segment);
			if ((flags & document::RenderFlags::SplitPages) != document::RenderFlags::None) {
				++num;
			}

			auto data = getPageData(num);
			return data.texRect.origin.y;
		} else {
			return pos + getScrollSize() / 2.0f;
		}
	}
	return 0.0f;
}

View::ViewPosition View::getViewPosition() const {
	if (_renderSize == Size2::ZERO) {
		return ViewPosition{maxOf<size_t>(), 0.0f, 0.0f};
	}

	return getViewObjectPosition(getViewContentPosition());
}

void View::setViewPosition(const ViewPosition &pos, bool offseted) {
	if (_renderingEnabled) {
		auto res = _renderer->getResult();
		if (res && pos.object != maxOf<size_t>()) {
			auto objs = res->result->getObjects();
			if (objs.size() > pos.object) {
				auto &obj = objs.at(pos.object);
				if ((res->result->getMedia().flags & document::RenderFlags::PaginatedLayout)
						!= document::RenderFlags::None) {
					float scrollPos = obj->bbox.origin.y + obj->bbox.size.height * pos.position;
					int num = roundf(scrollPos / res->result->getSurfaceSize().height);
					if ((res->result->getMedia().flags & document::RenderFlags::SplitPages)
							!= document::RenderFlags::None) {
						setScrollPosition((num / 2) * _renderSize.width);
					} else {
						setScrollPosition(num * _renderSize.width);
					}
				} else {
					float scrollPos = (obj->bbox.origin.y + obj->bbox.size.height * pos.position)
							- (!offseted ? (getScrollSize() / 2.0f) : 0.0f);
					if (scrollPos < getScrollMinPosition()) {
						scrollPos = getScrollMinPosition();
					} else if (scrollPos > getScrollMaxPosition()) {
						scrollPos = getScrollMaxPosition();
					}
					setScrollPosition(scrollPos);
				}
				if (_scrollCallback) {
					_scrollCallback(0.0f, true);
				}
			}
			_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};
			return;
		}
	}
	_savedPosition = pos;
	_layoutChanged = true;
}

void View::setPositionCallback(const PositionCallback &cb) { _positionCallback = cb; }
const View::PositionCallback &View::getPositionCallback() const { return _positionCallback; }

bool View::Highlight::init(View *view) {
	if (!Sprite::init()) {
		return false;
	}

	_view = view;

	setColor(Color::Teal_500);
	setOpacity(48.0f / 256.0f);

	setCascadeOpacityEnabled(false);

	return true;
}

bool View::Highlight::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	return Sprite::visitDraw(frame, parentFlags);
}

void View::Highlight::clearSelection() {
	_selectionBounds.clear();
	setDirty();
}
void View::Highlight::addSelection(const Pair<SelectionPosition, SelectionPosition> &p) {
	_selectionBounds.push_back(p);
	setDirty();
}

void View::Highlight::setDirty() {
	_vertexesDirty = true;
	_vertexes.clear();
}

void View::Highlight::emplaceRect(const Rect &rect, size_t idx, size_t count) {
	Vec2 origin;
	if (_view->isVertical()) {
		origin = Vec2(rect.origin.x, -rect.origin.y - _view->getObjectsOffset() - rect.size.height);
	} else {
		auto res = _view->getResult();
		if (res) {
			size_t page = size_t(floorf((rect.origin.y + rect.size.height)
					/ res->result->getMedia().surfaceSize.height));
			auto pageData = res->result->getPageData(page, 0);

			origin = Vec2(rect.origin.x + pageData.viewRect.origin.x + pageData.margin.left,
					pageData.margin.bottom
							+ (pageData.texRect.size.height - rect.origin.y - rect.size.height
									+ pageData.texRect.origin.y));
		} else {
			return;
		}
	}

	auto quad = _vertexes.addQuad();
	quad.setGeometry(Vec4(origin, 0, 1), rect.size);
}

void View::Highlight::updateVertexes(FrameInfo &frame) {
	auto res = _view->getResult();
	if (res) {
		_vertexes.clear();
		Vector<Rect> rects;
		auto density = res->result->getMedia().density;
		for (auto &it : _selectionBounds) {
			if (it.first.object == it.second.object) {
				auto obj = res->result->getObject(it.first.object);
				if (obj && obj->isLabel()) {
					auto l = obj->asLabel();
					uint32_t size = uint32_t(l->layout.chars.size() - 1);
					l->layout.getLabelRects([&](const Rect &rect) { rects.emplace_back(rect); },
							std::min(it.first.position, size), std::min(it.second.position, size),
							density, obj->bbox.origin, Padding());
				}
			} else {
				auto firstObj = res->result->getObject(it.first.object);
				auto secondObj = res->result->getObject(it.second.object);
				if (firstObj && firstObj->isLabel()) {
					auto label = firstObj->asLabel();
					uint32_t size = uint32_t(label->layout.chars.size() - 1);
					label->layout.getLabelRects([&](const Rect &rect) { rects.emplace_back(rect); },
							std::min(it.first.position, size), size, density, firstObj->bbox.origin,
							Padding());
				}
				for (size_t i = it.first.object + 1; i < it.second.object; ++i) {
					auto obj = res->result->getObject(i);
					if (obj && obj->isLabel()) {
						auto label = obj->asLabel();
						label->layout.getLabelRects(
								[&](const Rect &rect) { rects.emplace_back(rect); }, 0,
								uint32_t(label->layout.chars.size() - 1), density, obj->bbox.origin,
								Padding());
					}
				}
				if (secondObj && secondObj->isLabel()) {
					auto label = secondObj->asLabel();
					uint32_t size = uint32_t(label->layout.chars.size() - 1);
					label->layout.getLabelRects([&](const Rect &rect) { rects.emplace_back(rect); },
							0, std::min(it.second.position, size), density, secondObj->bbox.origin,
							Padding());
				}
			}
		}

		size_t rectIdx = 0;
		for (auto &it : rects) {
			emplaceRect(it, rectIdx, rects.size());
			++rectIdx;
		}
	}
	_vertexColorDirty = true;
}

} // namespace stappler::xenolith::richtext
