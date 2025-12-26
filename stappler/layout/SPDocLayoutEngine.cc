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

#include "SPDocLayoutEngine.h"
#include "SPDocLayoutResult.h"
#include "SPDocLayoutInlineContext.h"
#include "SPDocLayoutTable.h"
#include "SPDocPageContainer.h"
#include "SPDocNode.h"
#include "SPFontFace.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct LayoutEngine::Data : public memory::AllocPool,
							public InterfaceObject<memory::PoolInterface> {
	memory::pool_t *pool = nullptr;
	LayoutEngine *engine = nullptr;
	Rc<Document> document;
	MediaParameters media;
	Vector<StringView> spine;

	Vector<MediaParameters> originalMedia;
	Margin margin;

	Rc<LayoutResult> result;
	ExternalAssetsMap externalAssets;
	Map<StringView, DocumentImage *> images;

	Vector<LayoutBlock *> layoutStack;
	Vector<FloatContext *> floatStack;
	Rc<font::HyphenMap> hyphens;

	Vector<const Node *> nodeStack;
	Map<NodeId, StyleList> styles;

	const PageContainer *currentPage = nullptr;
	Vector<bool> resolvedMedia;
	NodeId maxNodeId = 0;

	Vector<InlineContext *> contextStorage;
	std::function<Rc<font::FontFaceSet>(const FontStyleParameters &)> fontCallback;

	Data(memory::pool_t *p, LayoutEngine *, Document *doc, const MediaParameters &m,
			SpanView<StringView> s);

	void setPage(const PageContainer *page);
	void addLayoutObjects(LayoutBlock &l);
	void processChilds(LayoutBlock &l, const Node &node);
	bool processChildNode(LayoutBlock &l, const Node &newNode, Vec2 &pos, float &height,
			float &collapsableMarginTop, bool pageBreak);
	void doPageBreak(LayoutBlock *lPtr, Vec2 &vec);
	bool processInlineNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, const Vec2 &pos);
	bool processInlineBlockNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, const Vec2 &pos);
	bool processFloatNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, Vec2 &pos);
	bool processBlockNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, Vec2 &pos, float &height,
			float &collapsableMarginTop);
	bool processNode(LayoutBlock &l, const Vec2 &origin, const Size2 &size, float colTop);
	bool processTableNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, Vec2 &pos, float &height,
			float &collapsableMarginTop);

	const StyleList *compileStyle(const Node &node);

	WideStringView getNodeValue(const LayoutBlock::NodeInfo &) const;
};

LayoutEngine::LayoutEngine(Document *doc,
		std::function<Rc<font::FontFaceSet>(const FontStyleParameters &)> &&fn,
		const MediaParameters &media, SpanView<StringView> spine) {
	auto pool = memory::pool::create(memory::pool::acquire());
	mem_pool::perform([&] {
		_data = new (pool) Data(pool, this, doc, media, spine);
		_data->fontCallback = sp::move(fn);
	}, pool);
}

LayoutEngine::~LayoutEngine() {
	if (_data) {
		auto p = _data->pool;
		_data->document = nullptr;
		delete _data;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

void LayoutEngine::setExternalAssetsMeta(ExternalAssetsMap &&external) {
	mem_pool::perform([&] {
		_data->externalAssets = sp::move(external);

		for (auto &it : _data->externalAssets) {
			const DocumentAssetMeta &a = it.second;
			if (a.isImage()) {
				auto img = new (_data->pool) DocumentImage;
				img->type = DocumentImage::Web;
				img->width = a.imageWidth;
				img->height = a.imageHeight;
				img->path = it.first;
				img->ct = a.type;

				_data->images.emplace(it.first, img);
			}
		}
	}, _data->pool);
}

void LayoutEngine::setHyphens(font::HyphenMap *map) { _data->hyphens = map; }

void LayoutEngine::setMargin(const Margin &m) { _data->margin = m; }

LayoutResult *LayoutEngine::getResult() const { return _data->result; }

const MediaParameters &LayoutEngine::getMedia() const { return _data->media; }
const MediaParameters &LayoutEngine::getOriginalMedia() const {
	return _data->originalMedia.empty() ? _data->media : _data->originalMedia.front();
}

void LayoutEngine::hookMedia(const MediaParameters &media) {
	_data->originalMedia.push_back(_data->media);
	_data->media = media;
}
void LayoutEngine::restoreMedia() {
	if (!_data->originalMedia.empty()) {
		_data->media = _data->originalMedia.back();
		_data->originalMedia.pop_back();
	}
}

bool LayoutEngine::isMediaHooked() const { return !_data->originalMedia.empty(); }

Rc<font::FontFaceSet> LayoutEngine::getFont(const FontStyleParameters &style) {
	auto f = _data->fontCallback(style);
	if (f) {
		_data->result->storeFont(f);
	}
	return f;
}

Document *LayoutEngine::getDocument() const { return _data->document; }

font::HyphenMap *LayoutEngine::getHyphens() const { return _data->hyphens; }

void LayoutEngine::incrementNodeId(NodeId max) { _data->maxNodeId += max; }

NodeId LayoutEngine::getMaxNodeId() const { return _data->maxNodeId; }

LayoutBlock *LayoutEngine::makeLayout(LayoutBlock::NodeInfo &&n, bool disablePageBreaks) {
	memory::context ctx(_data->pool);
	return new (_data->pool)
			LayoutBlock(this, move(n), disablePageBreaks, uint16_t(_data->layoutStack.size()));
}

LayoutBlock *LayoutEngine::makeLayout(LayoutBlock::NodeInfo &&n, LayoutBlock::PositionInfo &&p) {
	memory::context ctx(_data->pool);
	return new (_data->pool)
			LayoutBlock(this, move(n), move(p), uint16_t(_data->layoutStack.size()));
}

LayoutBlock *LayoutEngine::makeLayout(const Node *node, Display ctx, bool disablePageBreaks) {
	memory::context context(_data->pool);
	return new (_data->pool) LayoutBlock(this, node, getStyle(*node), ctx, disablePageBreaks,
			uint16_t(_data->layoutStack.size()));
}

InlineContext *LayoutEngine::acquireInlineContext(float d) {
	memory::context ctx(_data->pool);

	for (auto &it : _data->contextStorage) {
		if (it->finalized) {
			it->reset();
			return it;
		}
	}

	auto l = new (_data->pool)
			InlineContext([e = this](const FontStyleParameters &f) { return e->getFont(f); }, d);

	_data->contextStorage.emplace_back(l);
	return _data->contextStorage.back();
}

bool LayoutEngine::isFileExists(StringView url) const {
	auto it = _data->externalAssets.find(url);
	if (it != _data->externalAssets.end()) {
		return true;
	}

	return _data->document->isFileExists(url);
}

const DocumentImage *LayoutEngine::getImage(StringView url) const {
	auto it = _data->images.find(url);
	if (it != _data->images.end()) {
		return it->second;
	}

	return _data->document->getImage(url);
}

const StyleList *LayoutEngine::compileStyle(const Node &node) { return _data->compileStyle(node); }

const StyleList *LayoutEngine::getStyle(const Node &node) const {
	auto it = _data->styles.find(node.getNodeId());
	if (it != _data->styles.end()) {
		return &it->second;
	}
	return nullptr;
}

bool LayoutEngine::resolveMediaQuery(MediaQueryId queryId) const {
	if (queryId.get() < _data->resolvedMedia.size()) {
		return _data->resolvedMedia[queryId.get()];
	}
	return false;
}

StringView LayoutEngine::resolveString(StringId id) const {
	if (!_data->document) {
		return StringView();
	}
	if (id < _data->document->getData()->strings.size()) {
		return _data->document->getData()->strings[id];
	}
	return StringView();
}

float LayoutEngine::getDensity() const { return _data->media.density; }

float LayoutEngine::getFontScale() const { return _data->media.fontScale; }

Color4B LayoutEngine::getTextColor() const { return Color(_data->media.defaultBackground).text(); }

Float LayoutEngine::getNodeFloating(const Node &node) const {
	auto style = getStyle(node);
	auto d = style->get(ParameterName::CssFloat, this);
	if (!d.empty()) {
		return d.back().value.floating;
	}
	return Float::None;
}

Display LayoutEngine::getNodeDisplay(const Node &node) const {
	auto style = getStyle(node);
	auto d = style->get(ParameterName::CssDisplay, this);
	if (!d.empty()) {
		return d.back().value.display;
	}
	return Display::RunIn;
}

Display LayoutEngine::getNodeDisplay(const Node &node, Display p) const {
	auto d = getNodeDisplay(node);
	// correction of display style property
	// actual value may be different for default/run-in
	// <img> tag is block by default, but inline-block inside <p> or in inline context
	if (d == Display::Default || d == Display::RunIn) {
		if (p == Display::Block) {
			d = Display::Block;
		} else if (p == Display::Inline) {
			if (node.getHtmlName() == "img") {
				d = Display::InlineBlock;
			} else {
				d = Display::Inline;
			}
		}
	}

	if (d == Display::InlineBlock) {
		if (p == Display::Block) {
			d = Display::Block;
		}
	}

	if (node.getHtmlName() == "img"
			&& (_data->media.flags & RenderFlags::NoImages) != RenderFlags::None) {
		d = Display::None;
	}

	return d;
}

Display LayoutEngine::getLayoutContext(const Node &node) {
	auto &nodes = node.getNodes();
	if (nodes.empty()) {
		return Display::Block;
	} else {
		bool inlineBlock = false;
		for (auto &it : nodes) {
			auto style = compileStyle(*it);
			auto d = style->get(ParameterName::CssDisplay, this);
			if (!d.empty()) {
				auto val = d.back().value.display;
				if (val == Display::Inline) {
					return Display::Inline;
				} else if (val == Display::InlineBlock) {
					if (!inlineBlock) {
						inlineBlock = true;
					} else {
						return Display::Inline;
					}
				}
			}
		}
	}
	return Display::Block;
}

Display LayoutEngine::getLayoutContext(const Vector<Node *> &nodes,
		Vector<Node *>::const_iterator it, Display p) {
	if (p == Display::Inline) {
		if (it != nodes.end()) {
			auto d = getNodeDisplay(**it);
			if (d == Display::Table || d == Display::Block || d == Display::ListItem) {
				return d;
			}
		}
		return Display::Inline;
	} else {
		for (; it != nodes.end(); ++it) {
			auto d = getNodeDisplay(**it);
			if (d == Display::Inline || d == Display::InlineBlock) {
				return Display::Inline;
			} else if (d == Display::Block) {
				return Display::Block;
			} else if (d == Display::ListItem) {
				return Display::ListItem;
			}
		}
		return Display::Block;
	}
}

uint16_t LayoutEngine::getLayoutDepth() const { return uint16_t(_data->layoutStack.size()); }

LayoutBlock *LayoutEngine::getTopLayout() const {
	if (_data->layoutStack.size() > 1) {
		return _data->layoutStack.back();
	}
	return nullptr;
}

void LayoutEngine::render() {
	if (_data->spine.empty()) {
		auto spine = _data->document->getSpine();
		_data->spine.reserve(spine.size());

		for (auto &it : spine) { _data->spine.emplace_back(it.file); }
	}

	auto root = _data->document->getRoot();
	if (!root) {
		slog().error("LayoutEngine", "Fail to detect document root");
		return;
	}

	auto rootNode = root->getRoot();

	_data->setPage(root);
	_data->nodeStack.push_back(rootNode);
	_data->contextStorage.reserve(8);

	compileStyle(*rootNode);

	LayoutBlock *l = makeLayout(rootNode, Display::Block, false);

	if (_data->spine.empty()
			|| (_data->media.flags & RenderFlags::RenderById) == RenderFlags::None) {
		l->node.block = BlockModelParameters();
		l->node.block.width = Metric{1.0f, Metric::Units::Percent};
		l->node.block.marginLeft = Metric{_data->margin.left, Metric::Units::Px};
		l->node.block.marginTop = Metric{_data->margin.top, Metric::Units::Px};
		l->node.block.marginBottom = Metric{_data->margin.bottom, Metric::Units::Px};
		l->node.block.marginRight = Metric{_data->margin.right, Metric::Units::Px};
	}
	BackgroundParameters rootBackground = l->node.style->compileBackground(this);

	_data->nodeStack.pop_back();

	bool pageBreak = (_data->media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None;
	FloatContext f{l, pageBreak ? _data->media.surfaceSize.height : nan()};
	_data->floatStack.push_back(&f);
	if (l->init(Vec2::ZERO, Size2(_data->media.surfaceSize.width, nan()), 0.0f)) {
		Vec2 pos = l->pos.position;
		float collapsableMarginTop = l->pos.collapsableMarginTop;
		float height = 0;
		_data->layoutStack.push_back(l);
		l->node.context = Display::Block;

		if (_data->spine.empty()) {
			if (auto page = _data->document->getContentPage(String())) {
				_data->setPage(page);
				_data->processChildNode(*l, *page->getRoot(), pos, height, collapsableMarginTop,
						pageBreak);
			}
		} else {
			if ((_data->media.flags & RenderFlags::RenderById) != RenderFlags::None) {
				pageBreak = false;
				for (auto &it : _data->spine) {
					auto node = _data->document->getNodeByIdGlobal(it);
					if (node.first && node.second) {
						_data->setPage(node.first);
						_data->processChildNode(*l, *node.second, pos, height, collapsableMarginTop,
								pageBreak);
					}
				}
			} else {
				for (auto &it : _data->spine) {
					if (auto page = _data->document->getContentPage(it)) {
						_data->setPage(page);
						_data->processChildNode(*l, *page->getRoot(), pos, height,
								collapsableMarginTop, pageBreak);
					}
				}
			}
		}
		l->finalizeChilds(height);
		_data->layoutStack.pop_back();

		l->finalize(Vec2::ZERO, 0.0f);
	} else {
		_data->nodeStack.pop_back();
	}
	_data->floatStack.pop_back();

	_data->result->setContentSize(l->pos.size + Size2(0.0f, 16.0f));

	if (rootBackground.backgroundColor.a != 0) {
		_data->result->setBackgroundColor(rootBackground.backgroundColor);
	} else {
		_data->result->setBackgroundColor(_data->media.defaultBackground);
	}

	if (!l->layouts.empty()) {
		_data->addLayoutObjects(*l);
	}
	_data->result->finalize();
}

Pair<float, float> LayoutEngine::getFloatBounds(const LayoutBlock *l, float y, float height) {
	float x = 0, width = _data->media.surfaceSize.width;
	if (!_data->layoutStack.empty()) {
		std::tie(x, width) = _data->floatStack.back()->getAvailablePosition(y, height);
	}

	float minX = 0, maxWidth = _data->media.surfaceSize.width;
	if (l->pos.size.width == 0 || std::isnan(l->pos.size.width)) {
		auto f = _data->floatStack.back();
		minX = f->root->pos.position.x;
		maxWidth = f->root->pos.size.width;

		bool found = false;
		for (auto &it : _data->layoutStack) {
			if (!found) {
				if (it == f->root) {
					found = true;
				}
			}
			if (found) {
				minX += it->pos.margin.left + it->pos.padding.left;
				maxWidth -= (it->pos.margin.left + it->pos.padding.left + it->pos.padding.right
						+ it->pos.margin.right);
			}
		}

		minX += l->pos.margin.left + l->pos.padding.left;
		maxWidth -= (l->pos.margin.left + l->pos.padding.left + l->pos.padding.right
				+ l->pos.margin.right);
	} else {
		minX = l->pos.position.x;
		maxWidth = l->pos.size.width;
	}

	if (x < minX) {
		width -= (minX - x);
		x = minX;
	} else if (x > minX) {
		maxWidth -= (x - minX);
	}

	if (width > maxWidth) {
		width = maxWidth;
	}

	if (width < 0) {
		width = 0;
	}

	return pair(x, width);
}

font::Formatter::LinePosition LayoutEngine::getTextBounds(const LayoutBlock *l, uint16_t &linePos,
		uint16_t &lineHeight, float density, float parentPosY) {
	float y = parentPosY + linePos / density;
	float height = lineHeight / density;

	if ((_data->media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None
			&& !l->pos.disablePageBreak) {
		const float pageHeight = _data->media.surfaceSize.height;
		uint32_t curr1 = uint32_t(std::floor(y / pageHeight));
		uint32_t curr2 = uint32_t(std::floor((y + height) / pageHeight));

		if (curr1 != curr2) {
			linePos += uint16_t((curr2 * pageHeight - y) * density) + 1 * density;
			y = parentPosY + linePos / density;
		}
	}

	float x = 0, width = _data->media.surfaceSize.width;
	std::tie(x, width) = getFloatBounds(l, y, height);

	uint16_t retX = uint16_t(floorf((x - l->pos.position.x) * density));
	uint16_t retSize = uint16_t(floorf(width * density));

	return font::Formatter::LinePosition{retX, retSize};
}

void LayoutEngine::pushNode(const Node *node) { _data->nodeStack.push_back(node); }

void LayoutEngine::popNode() { _data->nodeStack.pop_back(); }

void LayoutEngine::processChilds(LayoutBlock &l, const Node &node) {
	_data->processChilds(l, node);
}

// Default style, that can be redefined with css
void LayoutEngine::beginStyle(StyleList &style, const Node &node, SpanView<const Node *> stack,
		const MediaParameters &media) const {
	_data->document->beginStyle(style, node, stack, media);
}

// Default style, that can NOT be redefined with css
void LayoutEngine::endStyle(StyleList &style, const Node &node, SpanView<const Node *> stack,
		const MediaParameters &media) const {
	_data->document->endStyle(style, node, stack, media);
}

LayoutEngine::Data::Data(memory::pool_t *p, LayoutEngine *e, Document *doc,
		const MediaParameters &m, SpanView<StringView> sp)
: pool(p), engine(e), document(doc), media(m), spine(sp.vec<Interface>()) {

	maxNodeId = document->getMaxNodeId();

	if ((media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None) {
		Margin extra(0);
		bool isSplitted = (media.flags & RenderFlags::SplitPages) != RenderFlags::None;
		if (isSplitted) {
			extra.left = 7 * media.fontScale;
			extra.right = 7 * media.fontScale;
		} else {
			extra.left = 3 * media.fontScale;
			extra.right = 3 * media.fontScale;
		}

		media.pageMargin.left += extra.left;
		media.pageMargin.right += extra.right;

		media.surfaceSize.width -= (extra.left + extra.right);
	}

	result = Rc<LayoutResult>::create(media, document);

	layoutStack.reserve(8);

	if ((media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None
			&& media.mediaType == MediaType::Screen) {
		media.mediaType = MediaType::Print;
	}

	resolvedMedia = media.resolveMediaQueries<memory::PoolInterface>(document->getData()->queries);
}

void LayoutEngine::Data::setPage(const PageContainer *page) { currentPage = page; }

void LayoutEngine::Data::addLayoutObjects(LayoutBlock &l) {
	if (l.node.node) {
		if (!l.node.node->getHtmlId().empty()) {
			result->pushIndex(l.node.node->getHtmlId(), l.pos.position);
		}
	}

	if (!l.objects.empty()) {
		for (auto &it : l.objects) { it->bbox.origin += l.pos.position; }
	}

	if (!l.layouts.empty()) {
		for (auto &it : l.layouts) { addLayoutObjects(*it); }
	}
}

void LayoutEngine::Data::processChilds(LayoutBlock &l, const Node &node) {
	auto &nodes = node.getNodes();
	Vec2 pos = l.pos.position;
	float collapsableMarginTop = l.pos.collapsableMarginTop;
	float height = 0;
	layoutStack.push_back(&l);

	l.node.context = (l.context && !l.context->finalized) ? Display::Inline : Display::RunIn;
	for (auto it = nodes.begin(); it != nodes.end(); ++it) {
		nodeStack.push_back((*it));
		compileStyle(**it);
		nodeStack.pop_back();

		l.node.context = engine->getLayoutContext(nodes, it, l.node.context);
		if (!processChildNode(l, **it, pos, height, collapsableMarginTop, false)) {
			break;
		}
	}

	l.finalizeChilds(height);
	layoutStack.pop_back();
}

bool LayoutEngine::Data::processChildNode(LayoutBlock &l, const Node &newNode, Vec2 &pos,
		float &height, float &collapsableMarginTop, bool pageBreak) {
	bool ret = true;
	nodeStack.push_back(&newNode);
	if (pageBreak) {
		if (!l.layouts.empty()) {
			doPageBreak(l.layouts.back(), pos);
		} else {
			doPageBreak(nullptr, pos);
		}
		collapsableMarginTop = 0;
	}

	auto style = compileStyle(newNode);
	// SP_RTBUILDER_LOG("block style: %s", style.css(this).c_str());

	LayoutBlock::NodeInfo nodeInfo(&newNode, style, engine, l.node.context);

	if (newNode.getHtmlName() == "img"
			&& (media.flags & RenderFlags::NoImages) != RenderFlags::None) {
		nodeInfo.block.display = Display::None;
	}

	if (nodeInfo.block.display == Display::None) {
		ret = true;
	} else if (nodeInfo.block.floating != Float::None) {
		ret = processFloatNode(l, move(nodeInfo), pos);
	} else if (nodeInfo.block.display == Display::Inline) {
		ret = processInlineNode(l, move(nodeInfo), pos);
	} else if (nodeInfo.block.display == Display::InlineBlock) {
		ret = processInlineBlockNode(l, move(nodeInfo), pos);
	} else if (nodeInfo.block.display == Display::Table) {
		ret = processTableNode(l, move(nodeInfo), pos, height, collapsableMarginTop);
	} else {
		ret = processBlockNode(l, move(nodeInfo), pos, height, collapsableMarginTop);
	}

	nodeStack.pop_back();

	return ret;
}

void LayoutEngine::Data::doPageBreak(LayoutBlock *lPtr, Vec2 &vec) {
	while (lPtr) {
		if (lPtr->pos.margin.bottom > 0) {
			vec.y -= lPtr->pos.margin.bottom;
			lPtr->pos.margin.bottom = 0;
		}
		if (lPtr->pos.padding.bottom > 0) {
			vec.y -= lPtr->pos.padding.bottom;
			lPtr->pos.padding.bottom = 0;
		}

		if (!lPtr->layouts.empty()) {
			lPtr = lPtr->layouts.back();
		} else {
			lPtr = nullptr;
		}
	}

	const float pageHeight = media.surfaceSize.height;
	float curr = std::ceil((vec.y - 1.1f) / pageHeight);

	if (vec.y > 0) {
		vec.y = curr * pageHeight;
	}
}

bool LayoutEngine::Data::processInlineNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node,
		const Vec2 &pos) {
	//SP_RTBUILDER_LOG("paragraph style: %s", styleVec.css(this).c_str());

	InlineContext &ctx = l.makeInlineContext(pos.y, *node.node);

	const float density = media.density;
	auto fstyle = node.style->compileFontStyle(engine);
	auto font = engine->getFont(fstyle);
	if (!font) {
		return false;
	}

	auto fontMetrics = font->getMetrics();

	auto front = uint16_t((media.computeValueAuto(node.block.marginLeft, l.pos.size.width,
								   fontMetrics.height / density)
								  + media.computeValueAuto(node.block.paddingLeft, l.pos.size.width,
										  fontMetrics.height / density))
			* density);
	auto back = uint16_t((media.computeValueAuto(node.block.marginRight, l.pos.size.width,
								  fontMetrics.height / density)
								 + media.computeValueAuto(node.block.paddingRight, l.pos.size.width,
										 fontMetrics.height / density))
			* density);

	uint16_t firstCharId = 0, lastCharId = 0;

	auto textStyle = node.style->compileTextLayout(engine);

	firstCharId = ctx.targetLabel->layout.chars.size();

	ctx.reader.read(fstyle, textStyle, getNodeValue(node), front, back);
	ctx.pushNode(node.node, [&](InlineContext &ctx) {
		lastCharId = (ctx.targetLabel->layout.chars.size() > 0)
				? (ctx.targetLabel->layout.chars.size() - 1)
				: 0;

		while (firstCharId < ctx.targetLabel->layout.chars.size()
				&& ctx.targetLabel->layout.chars.at(firstCharId).charID == ' ') {
			firstCharId++;
		}
		while (lastCharId < ctx.targetLabel->layout.chars.size() && lastCharId >= firstCharId
				&& ctx.targetLabel->layout.chars.at(lastCharId).charID == ' ') {
			lastCharId--;
		}

		if (ctx.targetLabel->layout.chars.size() > lastCharId && firstCharId <= lastCharId) {
			auto hrefIt = node.node->getAttributes().find("href");
			if (hrefIt != node.node->getAttributes().end() && !hrefIt->second.empty()) {
				auto targetIt = node.node->getAttributes().find("target");
				ctx.refPos.push_back(InlineContext::RefPosInfo{firstCharId, lastCharId,
					hrefIt->second,
					(targetIt == node.node->getAttributes().end()) ? String() : targetIt->second});
			}

			auto outline = node.style->compileOutline(engine);
			if (outline.outline.style != BorderStyle::None
					|| outline.left.style != BorderStyle::None
					|| outline.top.style != BorderStyle::None
					|| outline.right.style != BorderStyle::None
					|| outline.bottom.style != BorderStyle::None) {
				ctx.outlinePos.emplace_back(
						InlineContext::OutlinePosInfo{firstCharId, lastCharId, outline});
			}

			auto background = node.style->compileBackground(engine);
			if (background.backgroundColor.a != 0 || !background.backgroundImage.empty()) {
				ctx.backgroundPos.push_back(
						InlineContext::BackgroundPosInfo{firstCharId, lastCharId, background,
							Padding(media.computeValueAuto(node.block.paddingTop, l.pos.size.width,
											fontMetrics.height / density),
									media.computeValueAuto(node.block.paddingRight,
											l.pos.size.width, fontMetrics.height / density),
									media.computeValueAuto(node.block.paddingBottom,
											l.pos.size.width, fontMetrics.height / density),
									media.computeValueAuto(node.block.paddingLeft, l.pos.size.width,
											fontMetrics.height / density))});
			}

			if (!node.node->getHtmlId().empty()) {
				ctx.idPos.push_back(
						InlineContext::IdPosInfo{firstCharId, lastCharId, node.node->getHtmlId()});
			}
		} else {
			if (!node.node->getHtmlId().empty()) {
				ctx.idPos.push_back(
						InlineContext::IdPosInfo{firstCharId, firstCharId, node.node->getHtmlId()});
			}
		}
	});

	processChilds(l, *node.node);
	ctx.popNode(node.node);

	return true;
}

bool LayoutEngine::Data::processInlineBlockNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node,
		const Vec2 &pos) {
	if (node.node->getHtmlName() == "img"
			&& (media.flags & RenderFlags::NoImages) != RenderFlags::None) {
		return true;
	}

	LayoutBlock *newL = engine->makeLayout(sp::move(node), true);
	if (processNode(*newL, pos, l.pos.size, 0.0f)) {
		auto bbox = newL->getBoundingBox();

		if (bbox.size.width > 0.0f && bbox.size.height > 0.0f) {
			l.inlineBlockLayouts.emplace_back(newL);
			LayoutBlock &ref = *newL;

			InlineContext &ctx = l.makeInlineContext(pos.y, *node.node);

			const float density = media.density;
			auto fstyle = node.style->compileFontStyle(engine);
			auto font = engine->getFont(fstyle);
			if (!font) {
				return false;
			}

			auto fontMetrics = font->getMetrics();

			auto textStyle = node.style->compileTextLayout(engine);
			uint16_t width = uint16_t(bbox.size.width * density);
			uint16_t height = uint16_t(bbox.size.height * density);

			switch (textStyle.verticalAlign) {
			case VerticalAlign::Baseline: height += (fontMetrics.size - fontMetrics.height); break;
			case VerticalAlign::Sub:
				height += (fontMetrics.size - fontMetrics.height) + fontMetrics.descender / 2;
				break;
			case VerticalAlign::Super:
				height += (fontMetrics.size - fontMetrics.height) - fontMetrics.ascender / 2;
				break;
			default: break;
			}

			auto lineHeightVec = node.style->get(ParameterName::CssLineHeight, engine);
			if (lineHeightVec.size() == 1) {
				float lineHeight = media.computeValueStrong(lineHeightVec.front().value.sizeValue,
						width / density);

				if (lineHeight * density > height) {
					height = uint16_t(lineHeight * density);
				}
			}

			if (ctx.reader.read(fstyle, textStyle, width, height)) {
				ref.charBinding = (ctx.targetLabel->layout.ranges.size() > 0)
						? (ctx.targetLabel->layout.ranges.size() - 1)
						: 0;
			}
		}
	}
	return true;
}

bool LayoutEngine::Data::processFloatNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, Vec2 &pos) {
	pos.y += l.finalizeInlineContext();
	auto currF = floatStack.back();
	LayoutBlock *newL = engine->makeLayout(move(node), true);
	bool pageBreak = (media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None;
	FloatContext f{newL, pageBreak ? media.surfaceSize.height : nan()};
	floatStack.push_back(&f);
	if (processNode(*newL, pos, l.pos.size, 0.0f)) {
		Vec2 p = pos;
		if (currF->pushFloatingNode(l, *newL, p)) {
			l.layouts.emplace_back(newL);
		}
	}
	floatStack.pop_back();
	return true;
}

bool LayoutEngine::Data::processBlockNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, Vec2 &pos,
		float &height, float &collapsableMarginTop) {
	l.cancelInlineContext(pos, height, collapsableMarginTop);
	LayoutBlock *newL = engine->makeLayout(move(node), l.pos.disablePageBreak);
	if (processNode(*newL, pos, l.pos.size, collapsableMarginTop)) {
		float nodeHeight = newL->getBoundingBox().size.height;
		pos.y += nodeHeight;
		height += nodeHeight;
		collapsableMarginTop = newL->pos.margin.bottom;

		l.layouts.emplace_back(newL);
		if (!std::isnan(l.pos.maxHeight) && height > l.pos.maxHeight) {
			height = l.pos.maxHeight;
			return false;
		}
	}
	return true;
}

bool LayoutEngine::Data::processNode(LayoutBlock &l, const Vec2 &origin, const Size2 &size,
		float colTop) {
	if (l.init(origin, size, colTop)) {
		LayoutBlock *parent = nullptr;
		if (layoutStack.size() > 1) {
			parent = layoutStack.at(layoutStack.size() - 1);
			if (parent && parent->listItem != LayoutBlock::ListNone
					&& l.node.block.display == Display::ListItem) {
				auto valuePtr = l.node.node->getAttribute("value");
				if (!valuePtr.empty()) {
					valuePtr.readInteger().unwrap([&](int64_t id) { parent->listItemIndex = id; });
				}
			}
		}

		if (l.node.block.display == Display::ListItem) {
			if (l.node.block.listStylePosition == ListStylePosition::Inside) {
				l.makeInlineContext(origin.y, *l.node.node);
			}
		}

		bool pageBreaks = false;
		if (l.node.block.pageBreakInside == PageBreak::Avoid) {
			pageBreaks = l.pos.disablePageBreak;
			l.pos.disablePageBreak = true;
		}
		processChilds(l, *l.node.node);
		if (l.node.block.pageBreakInside == PageBreak::Avoid) {
			l.pos.disablePageBreak = pageBreaks;
		}

		l.finalize(origin, colTop);
		return true;
	}
	return false;
}

bool LayoutEngine::Data::processTableNode(LayoutBlock &l, LayoutBlock::NodeInfo &&node, Vec2 &pos,
		float &height, float &collapsableMarginTop) {
	l.cancelInlineContext(pos, height, collapsableMarginTop);
	LayoutTable table(engine->makeLayout(move(node), l.pos.disablePageBreak), l.pos.size,
			maxNodeId);
	++maxNodeId;
	if (table.layout->init(pos, l.pos.size, collapsableMarginTop)) {
		table.layout->node.context = Display::Table;

		bool pageBreaks = false;
		if (table.layout->node.block.pageBreakInside == PageBreak::Avoid) {
			pageBreaks = table.layout->pos.disablePageBreak;
			table.layout->pos.disablePageBreak = true;
		}

		table.processTableChilds();

		layoutStack.push_back(table.layout);
		table.processTableLayouts();
		layoutStack.pop_back();

		table.layout->finalizeChilds(table.layout->getContentBox().size.height);

		if (table.layout->node.block.pageBreakInside == PageBreak::Avoid) {
			table.layout->pos.disablePageBreak = pageBreaks;
		}

		table.layout->finalize(pos, collapsableMarginTop);

		float nodeHeight = table.layout->getBoundingBox().size.height;
		pos.y += nodeHeight;
		height += nodeHeight;
		collapsableMarginTop = table.layout->pos.margin.bottom;

		l.layouts.emplace_back(table.layout);
		if (!std::isnan(l.pos.maxHeight) && height > l.pos.maxHeight) {
			height = l.pos.maxHeight;
			return false;
		}
		return true;
	}
	return false;
}

const StyleList *LayoutEngine::Data::compileStyle(const Node &node) {
	auto it = styles.find(node.getNodeId());
	if (it != styles.end()) {
		return &it->second;
	}

	bool push = false;
	auto back = nodeStack.empty() ? nullptr : nodeStack.back();
	if (back != &node) {
		push = true;
		nodeStack.push_back(&node);
	}

	it = styles.emplace(node.getNodeId(), StyleList()).first;
	if (nodeStack.size() > 1) {
		const Node *parent = nodeStack.at(nodeStack.size() - 2);
		if (parent) {
			auto p_it = styles.find(parent->getNodeId());
			if (p_it != styles.end()) {
				it->second.merge(p_it->second, true);
			}
		}
	}

	engine->beginStyle(it->second, node, nodeStack, media);

	for (auto &ref_it : currentPage->getStyleLinks()) {
		if (ref_it.media == MediaQueryIdNone || resolvedMedia[ref_it.media.get()]) {
			if (auto page = document->getStyleDocument(ref_it.href)) {
				page->resolveNodeStyle(it->second, node, nodeStack, media, resolvedMedia);
			}
		}
	}

	currentPage->resolveNodeStyle(it->second, node, nodeStack, media, resolvedMedia);

	engine->endStyle(it->second, node, nodeStack, media);

	it->second.merge(node.getStyle());

	if (push) {
		nodeStack.pop_back();
	}

	return &it->second;
}

WideStringView LayoutEngine::Data::getNodeValue(const LayoutBlock::NodeInfo &node) const {
	if (node.node->getHtmlName() == "br") {
		return u"\n";
	} else {
		return node.node->getValue();
	}
}

} // namespace stappler::document
