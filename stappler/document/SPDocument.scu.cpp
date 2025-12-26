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

#include "SPCommon.h" // IWYU pragma: keep
#include "SPDocNode.cc"
#include "SPDocStyle.cc"
#include "SPDocStyleCss.cc"
#include "SPDocStyleContainer.cc"
#include "SPDocPageContainer.cc"

#include "SPDocFormat.cc"
#include "SPDocParser.cc"
#include "SPDocAsset.cc"

#include "html/SPDocHtml.cc"
#include "epub/SPDocEpub.cc"

namespace STAPPLER_VERSIONIZED stappler::document {

static std::atomic<uint64_t> s_docId = 1;

DocumentData::~DocumentData() { }

DocumentData::DocumentData(memory::pool_t *p, StringView str) : pool(p), id(s_docId.fetch_add(1)) {
	if (str.empty()) {
		name = StringView(string::toString<memory::StandartInterface>(id)).pdup(pool);
	} else {
		name = str.pdup(pool);
	}

	registerCleanupDestructor(this, pool);
}

StringId DocumentData::addString(const StringView &str) {
	strings.emplace_back(str.pdup(pool));
	return strings.size() - 1;
}

MediaQueryId DocumentData::addQuery(MediaQuery &&query) {
	queries.emplace_back(move(query));
	return MediaQueryId(queries.size() - 1);
}

bool Document::canOpen(FileInfo path, StringView ct) {
	return canOpen(memory::pool::acquire(), path, ct);
}

bool Document::canOpen(BytesView data, StringView ct) {
	return canOpen(memory::pool::acquire(), data, ct);
}

bool Document::canOpen(memory::pool_t *p, FileInfo path, StringView ct) {
	return Format::canOpenDocumnt(p, path, ct);
}

bool Document::canOpen(memory::pool_t *p, BytesView data, StringView ct) {
	return Format::canOpenDocumnt(p, data, ct);
}

Rc<Document> Document::open(FileInfo path, StringView ct) {
	return Document::open(memory::app_root_pool, path, ct);
}

Rc<Document> Document::open(BytesView data, StringView ct) {
	return Document::open(memory::app_root_pool, data, ct);
}

Rc<Document> Document::open(memory::pool_t *p, FileInfo path, StringView ct) {
	return Format::openDocument(p, path, ct);
}

Rc<Document> Document::open(memory::pool_t *p, BytesView data, StringView ct) {
	return Format::openDocument(p, data, ct);
}

Document::~Document() {
	if (_pool) {
		memory::pool::destroy(_pool);
	}
}

bool Document::init() { return init(memory::app_root_pool); }

bool Document::init(memory::pool_t *pool) {
	_pool = memory::pool::create(pool);
	_data = allocateData(_pool);
	return true;
}

bool Document::init(memory::pool_t *pool, const Callback<DocumentData *(memory::pool_t *)> &cb) {
	_pool = memory::pool::create(pool);
	memory::perform([&] { _data = cb(_pool); }, pool);
	return true;
}

StringView Document::getType() const { return _data->type; }

StringView Document::getName() const { return _data->name; }

SpanView<SpineFile> Document::getSpine() const { return _data->spine; }

const DocumentContentRecord &Document::getTableOfContents() const { return _data->tableOfContents; }

StringView Document::getMeta(StringView key) const {
	auto it = _data->meta.find(key);
	if (it != _data->meta.end()) {
		return it->second;
	}
	return StringView();
}

bool Document::isFileExists(StringView path) const {
	if (_data->pages.find(path) != _data->pages.end()) {
		return true;
	}
	if (_data->styles.find(path) != _data->styles.end()) {
		return true;
	}
	if (_data->images.find(path) != _data->images.end()) {
		return true;
	}
	if (_data->fonts.find(path) != _data->fonts.end()) {
		return true;
	}
	return false;
}

const DocumentImage *Document::getImage(StringView path) const {
	auto it = _data->images.find(path);
	if (it != _data->images.end()) {
		return &it->second;
	}
	return nullptr;
}

const PageContainer *Document::getContentPage(StringView path) const {
	auto it = _data->pages.find(path);
	if (it != _data->pages.end()) {
		return it->second;
	}
	return nullptr;
}

const StyleContainer *Document::getStyleDocument(StringView path) const {
	auto it = _data->styles.find(path);
	if (it != _data->styles.end()) {
		return it->second;
	}
	return nullptr;
}

const PageContainer *Document::getRoot() const {
	if (!_data->spine.empty()) {
		return getContentPage(_data->spine.front().file);
	}

	if (!_data->pages.empty()) {
		return _data->pages.begin()->second;
	}
	return nullptr;
}

const Node *Document::getNodeById(StringView path, StringView id) const {
	if (auto page = getContentPage(path)) {
		return page->getNodeById(id);
	}
	return nullptr;
}

Pair<const PageContainer *, const Node *> Document::getNodeByIdGlobal(StringView id) const {
	StringView tmp = id;
	auto file = tmp.readUntil<StringView::Chars<'#'>>();
	if (tmp.is('#') && !file.empty()) {
		++tmp;
		auto it = _data->pages.find(file);
		if (it != _data->pages.end()) {
			auto id_it = it->second->getNodeById(tmp);
			if (id_it) {
				return pair(it->second, id_it);
			}
		}
	}

	for (auto &it : _data->pages) {
		auto page = it.second;
		auto id_it = page->getNodeById(id);
		if (id_it) {
			return pair(page, id_it);
		}
	}
	return Pair<const PageContainer *, const Node *>{nullptr, nullptr};
}

void Document::foreachPage(const Callback<void(StringView, const PageContainer *)> &cb) {
	for (auto &it : _data->pages) { cb(it.first, it.second); }
}

NodeId Document::getMaxNodeId() const { return _data->maxNodeId; }

void Document::beginStyle(StyleList &style, const Node &node, SpanView<const Node *> stack,
		const MediaParameters &media) const {
	const Node *parent = nullptr;
	if (stack.size() > 1) {
		parent = stack.at(stack.size() - 2);
	}

	getStyleForTag(style, node.getHtmlName(),
			parent ? StringView(parent->getHtmlName()) : StringView(), media);

	auto &attr = node.getAttributes();

	for (auto &it : attr) {
		onStyleAttribute(style, node.getHtmlName(), it.first, it.second, media);
	}
}

void Document::endStyle(StyleList &, const Node &node, SpanView<const Node *> stack,
		const MediaParameters &media) const {
	// do nothing
}

DocumentData *Document::allocateData(memory::pool_t *pool) {
	memory::context ctx(pool);

	return new (pool) DocumentData(pool);
}

void Document::onStyleAttribute(StyleList &style, StringView tag, StringView name, StringView value,
		const MediaParameters &) const {
	if (name == "align") {
		StyleContainer::readCssParameter("text-align", value, [&](StyleParameter &&p) {
			style.set(p);
			return true;
		}, [](StringView) { return StringIdNone; });

		StyleContainer::readCssParameter("text-align", "0px", [&](StyleParameter &&p) {
			style.set(p);
			return true;
		}, [](StringView) { return StringIdNone; });
	} else if (name == "width") {
		if (value.back() == '%') {
			StringView(value).readFloat().unwrap([&](float v) {
				style.set<ParameterName::CssWidth>(Metric(v / 100.0f, Metric::Percent));
			});
		} else {
			StringView(value).readInteger().unwrap(
					[&](int64_t v) { style.set<ParameterName::CssWidth>(Metric(v, Metric::Px)); });
		}
	} else if (name == "height") {
		if (value.back() == '%') {
			StringView(value).readFloat().unwrap([&](float v) {
				style.set<ParameterName::CssWidth>(Metric(v / 100.0f, Metric::Percent));
			});
		} else {
			StringView(value).readInteger().unwrap(
					[&](int64_t v) { style.set<ParameterName::CssHeight>(Metric(v, Metric::Px)); });
		}
	} else if ((tag == "li" || tag == "ul" || tag == "ol") && name == "type") {
		if (value == "disc") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::Disc);
		} else if (value == "circle") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::Circle);
		} else if (value == "square") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::Square);
		} else if (value == "A") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::UpperAlpha);
		} else if (value == "a") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::LowerAlpha);
		} else if (value == "I") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::UpperRoman);
		} else if (value == "i") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::LowerRoman);
		} else if (value == "1") {
			style.set<ParameterName::CssListStyleType>(ListStyleType::Decimal);
		}
	}
}

} // namespace stappler::document
