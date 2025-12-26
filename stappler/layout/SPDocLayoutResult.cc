/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "SPDocLayoutResult.h"
#include "SPDocLayoutBlock.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct LayoutResult::Data : public memory::AllocPool,
							public InterfaceObject<memory::PoolInterface> {
	memory::pool_t *pool = nullptr;
	Rc<Document> document;
	MediaParameters media;
	Size2 size;

	Vector<Object *> objects;
	Vector<Link *> refs;
	Vector<LayoutBoundIndex> bounds;
	Vector<StringView> extraStrings;
	Map<StringView, Vec2> index;

	Set<Rc<font::FontFaceSet>> faces;

	Color4B background;

	size_t numPages = 1;

	StringView addString(StringView str) { return str.pdup(pool); }
};

LayoutResult::~LayoutResult() {
	if (_data) {
		auto p = _data->pool;
		_data->document = nullptr;
		delete _data;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

bool LayoutResult::init(const MediaParameters &media, Document *doc) {
	auto pool = memory::pool::create(memory::app_root_pool);
	mem_pool::perform([&] {
		_data = new (pool) Data;
		_data->pool = pool;
		_data->document = doc;

		_data->media = media;
		_data->size = media.surfaceSize;
	}, pool);

	return true;
}

void LayoutResult::storeFont(font::FontFaceSet *f) {
	memory::context ctx(_data->pool);

	_data->faces.emplace(f);
}

const MediaParameters &LayoutResult::getMedia() const { return _data->media; }

Document *LayoutResult::getDocument() const { return _data->document; }

void LayoutResult::pushIndex(StringView str, Vec2 pos) {
	_data->index.emplace(str.pdup(_data->pool), pos);
}

void LayoutResult::finalize() {
	if ((_data->media.flags & RenderFlags::NoHeightCheck) != RenderFlags::None) {
		_data->numPages = 1;
	} else {
		_data->numPages = size_t(ceilf(_data->size.height / _data->media.surfaceSize.height));
	}

	auto &toc = _data->document->getTableOfContents();

	StringView title = toc.label;
	if (title.empty()) {
		auto metaTitle = _data->document->getMeta("title");
		if (!metaTitle.empty()) {
			title = metaTitle;
		}
	}

	_data->bounds.emplace_back(
			LayoutBoundIndex{0, 0, 0.0f, _data->size.height, 0, title, toc.href});

	for (auto &it : toc.childs) { processContents(it, 1); }
}

void LayoutResult::setBackgroundColor(Color4B c) { _data->background = c; }
Color4B LayoutResult::getBackgroundColor() const { return _data->background; }

void LayoutResult::setContentSize(Size2 s) {
	_data->size = s;
	_data->size.width = _data->media.surfaceSize.width;

	if ((_data->media.flags & RenderFlags::NoHeightCheck) != RenderFlags::None) {
		_data->media.surfaceSize.height = _data->size.height;
	}

	_data->numPages = size_t(ceilf(_data->size.height / _data->media.surfaceSize.height)) + 1;
}

Size2 LayoutResult::getContentSize() const { return _data->size; }

Size2 LayoutResult::getSurfaceSize() const { return _data->media.surfaceSize; }

SpanView<Object *> LayoutResult::getObjects() const { return _data->objects; }

SpanView<Link *> LayoutResult::getRefs() const { return _data->refs; }

SpanView<LayoutBoundIndex> LayoutResult::getBounds() const { return _data->bounds; }

const Vec2 *LayoutResult::getIndexPoint(StringView str) const {
	auto it = _data->index.find(str);
	if (it != _data->index.end()) {
		return &it->second;
	}
	return nullptr;
}

size_t LayoutResult::getNumPages() const { return _data->numPages; }

LayoutBoundIndex LayoutResult::getBoundsForPosition(float pos) const {
	if (!_data->bounds.empty()) {
		auto it = std::lower_bound(_data->bounds.begin(), _data->bounds.end(), pos,
				[](const LayoutBoundIndex &idx, float pos) { return idx.end < pos; });
		if (it != _data->bounds.end()
				&& fabs(pos - it->end) < std::numeric_limits<float>::epsilon()) {
			++it;
		}
		if (it == _data->bounds.end()) {
			return _data->bounds.back();
		}
		if (it->start > pos) {
			return LayoutBoundIndex{maxOf<size_t>(), 0, 0.0f, 0.0f, maxOf<int64_t>()};
		}
		return *it;
	}
	return LayoutBoundIndex{maxOf<size_t>(), 0, 0.0f, 0.0f, maxOf<int64_t>()};
}

Label *LayoutResult::emplaceLabel(const LayoutBlock &l, ZOrder zIndex, bool isBullet) {
	memory::context ctx(_data->pool);

	auto ret = new (_data->pool) Label;
	ret->type = Object::Type::Label;
	ret->depth = l.depth;
	ret->zIndex = zIndex.get();
	ret->index = _data->objects.size();

	if (!isBullet) {
		auto &node = l.node.node;
		auto hashPtr = node->getAttribute("x-data-hash");
		if (!hashPtr.empty()) {
			ret->hash = _data->addString(hashPtr);
		}

		auto indexPtr = node->getAttribute("x-data-index");
		if (!indexPtr.empty()) {
			indexPtr.readInteger().unwrap([&](int64_t val) { ret->sourceIndex = size_t(val); });
		}
	}

	_data->objects.push_back(ret);
	return ret;
}

Background *LayoutResult::emplaceBackground(const LayoutBlock &l, const Rect &rect,
		const BackgroundParameters &style, ZOrder zIndex) {
	memory::context ctx(_data->pool);

	auto ret = new (_data->pool) Background;
	ret->type = Object::Type::Background;
	ret->depth = l.depth;
	ret->bbox = rect;
	ret->zIndex = zIndex.get();
	ret->background = style;
	ret->background.backgroundImage = _data->addString(ret->background.backgroundImage);
	ret->index = _data->objects.size();
	_data->objects.push_back(ret);
	return ret;
}

Link *LayoutResult::emplaceLink(const LayoutBlock &l, const Rect &rect, StringView href,
		StringView target, WideStringView text) {
	memory::context ctx(_data->pool);

	auto ret = new (_data->pool) Link;
	ret->type = Object::Type::Link;
	ret->depth = l.depth;
	ret->bbox = rect;
	ret->target = _data->addString(href);
	ret->mode = _data->addString(target);
	ret->index = _data->refs.size();
	if (!text.empty()) {
		ret->text = text.pdup(_data->pool);
	}
	_data->refs.push_back(ret);
	return ret;
}

PathObject *LayoutResult::emplaceOutline(const LayoutBlock &l, const Rect &rect,
		const Color4B &color, ZOrder zIndex, float width, BorderStyle style) {
	memory::context ctx(_data->pool);

	auto ret = new (_data->pool) PathObject;
	ret->type = Object::Type::Path;
	ret->depth = l.depth;
	ret->bbox = rect;
	ret->zIndex = zIndex.get();
	ret->drawOutline(rect, color, width, style);
	ret->index = _data->objects.size();
	_data->objects.push_back(ret);
	return ret;
}

void LayoutResult::emplaceBorder(LayoutBlock &l, const Rect &rect, const OutlineParameters &style,
		float width, ZOrder zIndex) {
	memory::context ctx(_data->pool);

	PathObject::makeBorder(this, l, rect, style, width, zIndex, _data->media);
}

PathObject *LayoutResult::emplacePath(const LayoutBlock &l, ZOrder zIndex) {
	memory::context ctx(_data->pool);

	auto ret = new (_data->pool) PathObject;
	ret->type = Object::Type::Path;
	ret->depth = l.depth;
	ret->zIndex = zIndex.get();
	ret->index = _data->objects.size();
	_data->objects.push_back(ret);
	return ret;
}

LayoutPageData LayoutResult::getPageData(size_t idx, float offset) const {
	auto surfaceSize = getSurfaceSize();
	if ((_data->media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None) {
		auto m = _data->media.pageMargin;
		bool isSplitted = (_data->media.flags & RenderFlags::SplitPages) != RenderFlags::None;
		auto pageSize =
				Size2(surfaceSize.width + m.horizontal(), surfaceSize.height + m.vertical());
		return LayoutPageData{m, Rect(pageSize.width * idx, 0, pageSize.width, pageSize.height),
			Rect(0, surfaceSize.height * idx, surfaceSize.width, surfaceSize.height), idx,
			isSplitted};
	} else {
		if (idx == _data->numPages - 1) {
			return LayoutPageData{Margin(),
				Rect(0, surfaceSize.height * idx + offset, surfaceSize.width,
						_data->size.height - surfaceSize.height * idx),
				Rect(0, surfaceSize.height * idx, surfaceSize.width,
						_data->size.height - surfaceSize.height * idx),
				idx, false};
		} else if (idx >= _data->numPages - 1) {
			return LayoutPageData{Margin(), Rect::ZERO, Rect::ZERO, idx, false};
		} else {
			return LayoutPageData{Margin(),
				Rect(0, surfaceSize.height * idx + offset, surfaceSize.width, surfaceSize.height),
				Rect(0, surfaceSize.height * idx, surfaceSize.width, surfaceSize.height), idx,
				false};
		}
	}
}

const Object *LayoutResult::getObject(size_t size) const {
	if (size < _data->objects.size()) {
		return _data->objects[size];
	}
	return nullptr;
}

const Label *LayoutResult::getLabelByHash(StringView hash, size_t idx) const {
	const Label *indexed = nullptr;
	for (auto &it : _data->objects) {
		if (auto l = it->asLabel()) {
			if (l->hash == hash) {
				return l;
			} else if (l->sourceIndex == idx) {
				indexed = l;
			}
		}
	}
	return indexed;
}

void LayoutResult::processContents(const DocumentContentRecord &rec, size_t level) {
	auto it = _data->index.find(rec.href);
	if (it != _data->index.end()) {
		auto pos = it->second.y;
		if (!_data->bounds.empty()) {
			_data->bounds.back().end = pos;
		}

		int64_t page = maxOf<int64_t>();
		if (!isnan(pos)
				&& (_data->media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None) {
			page = int64_t(roundf(pos / _data->media.surfaceSize.height));
		}

		if (!rec.label.empty()) {
			_data->bounds.emplace_back(LayoutBoundIndex{_data->bounds.size(), level, pos,
				_data->size.height, page, rec.label, rec.href});
		}
	}

	for (auto &it : rec.childs) { processContents(it, level + 1); }
}

} // namespace stappler::document
