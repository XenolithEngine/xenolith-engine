/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XL2dLabel.h"
#include "XLEventListener.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

Label::Selection::~Selection() { }

bool Label::Selection::init() {
	if (!Sprite::init()) {
		return false;
	}

	return true;
}

void Label::Selection::clear() { _vertexes.clear(); }

void Label::Selection::emplaceRect(const Rect &rect) {
	_vertexes.addQuad().setGeometry(Vec4(rect.origin.x,
											_contentSize.height - rect.origin.y - rect.size.height,
											_textureLayer, 1.0f),
			rect.size);
}

void Label::Selection::updateColor() { Sprite::updateColor(); }

void Label::Selection::updateVertexes(FrameInfo &frame) {
	//_vertexes.clear();
}

template <typename Interface>
static size_t Label_getQuadsCount(const font::TextLayoutData<Interface> *format) {
	size_t ret = 0;

	const font::RangeLayoutData *targetRange = nullptr;

	for (auto it = format->begin(); it != format->end(); ++it) {
		if (&(*it.range) != targetRange) {
			targetRange = &(*it.range);
		}

		const auto start = it.start();
		auto end = start + it.count();
		if (it.line->start + it.line->count == end) {
			const font::CharLayoutData &c = format->chars[end - 1];
			if (!chars::isspace(c.charID) && c.charID != char16_t(0x0A)) {
				++ret;
			}
			end -= 1;
		}

		for (auto charIdx = start; charIdx < end; ++charIdx) {
			const font::CharLayoutData &c = format->chars[charIdx];
			if (!chars::isspace(c.charID) && c.charID != char16_t(0x0A)
					&& c.charID != char16_t(0x00AD)
					&& c.charID != font::CharLayoutData::InvalidChar) {
				++ret;
			}
		}
	}

	return ret;
}

static void Label_pushColorMap(const font::RangeLayoutData &range, Vector<ColorMask> &cMap) {
	ColorMask mask = ColorMask::None;
	if (!range.colorDirty) {
		mask |= ColorMask::Color;
	}
	if (!range.opacityDirty) {
		mask |= ColorMask::A;
	}
	cMap.push_back(mask);
}

static void Label_writeTextureQuad(float height, const font::Metrics &m,
		const font::CharLayoutData &c, const font::CharShape &l, const font::RangeLayoutData &range,
		const font::LineLayoutData &line, VertexArray::Quad &quad, float layer) {
	switch (range.align) {
	case font::VerticalAlign::Sub:
		quad.drawChar(m, l, c.pos, height - float(int16_t(line.pos) - int16_t(m.descender * 2 / 3)),
				range.color, range.decoration, c.face, layer);
		break;
	case font::VerticalAlign::Super:
		quad.drawChar(m, l, c.pos, height - float(int16_t(line.pos) - int16_t(m.ascender * 2 / 3)),
				range.color, range.decoration, c.face, layer);
		break;
	default:
		quad.drawChar(m, l, c.pos, height - line.pos, range.color, range.decoration, c.face, layer);
		break;
	}
}

template <typename Interface>
static void Label_writeQuads(VertexArray &vertexes, const font::TextLayoutData<Interface> *format,
		Vector<ColorMask> &colorMap, float layer) {
	auto quadsCount = Label_getQuadsCount(format);
	colorMap.clear();
	colorMap.reserve(quadsCount);

	const font::RangeLayoutData *targetRange = nullptr;
	font::Metrics metrics;

	vertexes.clear();

	auto fbegin = format->begin();
	auto fend = format->end();
	for (auto it = fbegin; it != fend; ++it) {
		if (it.count() == 0) {
			continue;
		}

		if (&(*it.range) != targetRange) {
			targetRange = &(*it.range);
			metrics = targetRange->layout->getMetrics();
		}

		const auto start = it.start();
		auto end = start + it.count();

		for (auto charIdx = start; charIdx < end; ++charIdx) {
			const font::CharLayoutData &c = format->chars[charIdx];
			if (!chars::isspace(c.charID) && c.charID != char16_t(0x0A)
					&& c.charID != char16_t(0x00AD)
					&& c.charID != font::CharLayoutData::InvalidChar) {

				uint16_t face = 0;
				auto ch = targetRange->layout->getChar(c.charID, face);

				if (ch.charID == c.charID) {
					auto quad = vertexes.addQuad();
					Label_pushColorMap(*it.range, colorMap);
					Label_writeTextureQuad(format->height, metrics, c, ch, *it.range, *it.line,
							quad, layer);
				}
			}
		}

		if (it.line->start + it.line->count == end) {
			const font::CharLayoutData &c = format->chars[end - 1];
			if (c.charID == char16_t(0x00AD)) {
				uint16_t face = 0;
				auto ch = targetRange->layout->getChar('-', face);

				if (ch.charID == '-') {
					auto quad = vertexes.addQuad();
					Label_pushColorMap(*it.range, colorMap);
					Label_writeTextureQuad(format->height, metrics, c, ch, *it.range, *it.line,
							quad, layer);
				}
			}
			end -= 1;
		}

		if (it.count() > 0 && it.range->decoration != font::TextDecoration::None) {
			auto chstart = it.start();
			auto chend = it.end();
			while (chstart < chend && chars::isspace(format->chars[chstart].charID)) { ++chstart; }

			if (chstart == chend) {
				continue;
			}

			const font::CharLayoutData &firstChar = format->chars[chstart];
			const font::CharLayoutData &lastChar = format->chars[chend - 1];

			auto color = it.range->color;
			color.a = uint8_t(0.75f * color.a);
			auto layoutMetrics = it.range->layout->getMetrics();

			float offset = 0.0f;
			switch (it.range->decoration) {
			case font::TextDecoration::None: break;
			case font::TextDecoration::Overline: offset = layoutMetrics.height; break;
			case font::TextDecoration::LineThrough:
				offset = (layoutMetrics.height * 11.0f) / 24.0f;
				break;
			case font::TextDecoration::Underline: offset = layoutMetrics.height / 8.0f; break;
			}

			const float width = layoutMetrics.height / 16.0f;
			const float base = floorf(width);
			const float frac = width - base;

			const auto underlineBase = uint16_t(base);
			const auto underlineX = firstChar.pos;
			const auto underlineWidth = lastChar.pos + lastChar.advance - firstChar.pos;
			const auto underlineHeight = underlineBase;
			auto underlineY = int16_t(format->height) - int16_t(it.line->pos) + offset
					- int16_t(underlineBase / 2);

			switch (it.range->align) {
			case font::VerticalAlign::Sub:
				underlineY += int16_t(layoutMetrics.descender * 2 / 3);
				break;
			case font::VerticalAlign::Super:
				underlineY += int16_t(layoutMetrics.ascender * 2 / 3);
				break;
			default: break;
			}

			auto quad = vertexes.addQuad();
			Label_pushColorMap(*it.range, colorMap);
			quad.drawUnderlineRect(underlineX, underlineY, underlineWidth, underlineHeight, color,
					layer);
			if (frac > 0.1) {
				color.a *= frac;

				auto uquad = vertexes.addQuad();
				Label_pushColorMap(*it.range, colorMap);
				uquad.drawUnderlineRect(underlineX, underlineY - 1, underlineWidth, 1, color,
						layer);
			}
		}
	}
}

void Label::writeQuads(VertexArray &vertexes,
		const font::TextLayoutData<memory::StandartInterface> *format, Vector<ColorMask> &colorMap,
		float layer) {
	Label_writeQuads(vertexes, format, colorMap, layer);
}

void Label::writeQuads(VertexArray &vertexes,
		const font::TextLayoutData<memory::PoolInterface> *format, Vector<ColorMask> &colorMap,
		float layer) {
	Label_writeQuads(vertexes, format, colorMap, layer);
}

Rc<LabelResult> Label::writeResult(TextLayout *format, const Color4F &color, float layer) {
	auto result = Rc<LabelResult>::alloc();
	VertexArray array;
	array.init(uint32_t(format->getData()->chars.size() * 4),
			uint32_t(format->getData()->chars.size() * 6));

	writeQuads(array, format->getData(), result->colorMap, layer);
	result->data.data = array.pop();
	return result;
}

Label::~Label() { _format = nullptr; }

bool Label::init() { return init(nullptr); }

bool Label::init(StringView str) {
	return init(nullptr, DescriptionStyle(), str, 0.0f, TextAlign::Left);
}

bool Label::init(StringView str, float w, TextAlign a) {
	return init(nullptr, DescriptionStyle(), str, w, a);
}

bool Label::init(font::FontController *source, const DescriptionStyle &style, StringView str,
		float width, TextAlign alignment) {
	if (!Sprite::init()) {
		return false;
	}

	_style = style;
	setNormalized(true);

	setColorMode(core::ColorMode::AlphaChannel);
	setRenderingLevel(RenderingLevel::Surface);

	_listener = addSystem(Rc<EventListener>::create());

	_selection = addChild(Rc<Selection>::create());
	_selection->setAnchorPoint(Vec2(0.0f, 0.0f));
	_selection->setPosition(Vec2(0.0f, 0.0f));
	_selection->setColor(Color::BlueGrey_500);
	_selection->setOpacity(OpacityValue(64));
	_selection->setVisible(false);

	_marked = addChild(Rc<Selection>::create());
	_marked->setAnchorPoint(Vec2(0.0f, 0.0f));
	_marked->setPosition(Vec2(0.0f, 0.0f));
	_marked->setColor(Color::Green_500);
	_marked->setOpacity(OpacityValue(64));
	_marked->setVisible(false);

	setColor(Color4F(_style.text.color, _style.text.opacity), true);

	setString(str);
	setWidth(width);
	setAlignment(alignment);

	return true;
}

bool Label::init(const DescriptionStyle &style, StringView str, float w, TextAlign a) {
	return init(nullptr, style, str, w, a);
}

void Label::handleEnter(xenolith::Scene *scene) {
	Sprite::handleEnter(scene);

	if (_source) {
		return;
	}

	auto source = _director->getApplication()->getExtension<font::FontController>();
	if (source) {
		_listener->clear();

		_listener->listenForEventWithObject(font::FontController::onFontSourceUpdated, source,
				std::bind(&Label::onFontSourceUpdated, this));

		if (source->isLoaded()) {
			setTexture(Rc<Texture>(source->getTexture()));
		} else {
			_listener->listenForEventWithObject(font::FontController::onLoaded, source,
					std::bind(&Label::onFontSourceLoaded, this), true);
		}

		_source = source;
	}
}

void Label::handleExit() { Sprite::handleExit(); }

void Label::tryUpdateLabel() {
	if (_parent) {
		updateLabelScale(_parent->getNodeToWorldTransform());
	}
	if (_labelDirty) {
		updateLabel();
	}
}

void Label::setStyle(const DescriptionStyle &style) {
	_style = style;

	setColor(Color4F(_style.text.color, _style.text.opacity), true);

	setLabelDirty();
}

const Label::DescriptionStyle &Label::getStyle() const { return _style; }

Rc<LabelDeferredResult> Label::runDeferred(event::Looper *queue, TextLayout *format,
		const Color4F &color) {
	auto result = new std::promise<Rc<LabelResult>>;
	Rc<LabelDeferredResult> ret = Rc<LabelDeferredResult>::create(result->get_future());
	queue->performAsync(
			[queue, format = Rc<Label::TextLayout>(format), color, ret, result,
					layer = _textureLayer]() mutable {
		auto res = Label::writeResult(format, color, layer);
		result->set_value(res);

		queue->performOnThread([ret = move(ret), res = move(res), result]() mutable {
			ret->handleReady(move(res));
			delete result;
		}, nullptr);
	},
			ret);
	return ret;
}

void Label::applyLayout(TextLayout *layout) {
	_format = layout;

	if (_format) {
		if (_format->empty()) {
			setContentSize(Size2(0.0f, getFontHeight() / _labelDensity));
		} else {
			setContentSize(Size2(_format->getWidth() / _labelDensity,
					_format->getHeight() / _labelDensity));
		}

		setSelectionCursor(getSelectionCursor());
		setMarkedCursor(getMarkedCursor());

		_labelDirty = false;
		_vertexColorDirty = false;
		_vertexesDirty = true;
	} else {
		_vertexesDirty = true;
	}
}

void Label::updateLabel() {
	if (!_source) {
		return;
	}

	if (_string16.empty()) {
		applyLayout(nullptr);
		setContentSize(Size2(0.0f, getFontHeight() / _labelDensity));
		return;
	}

	auto spec = Rc<font::TextLayout>::alloc(_source, _string16.size(), _compiledStyles.size() + 1);

	_compiledStyles = compileStyle();
	_style.text.color = _displayedColor.getColor();
	_style.text.opacity = _displayedColor.getOpacity();
	_style.text.whiteSpace = font::WhiteSpace::PreWrap;

	if (!updateFormatSpec(spec, _compiledStyles, _labelDensity, _adjustValue)) {
		return;
	}

	applyLayout(spec);
}

void Label::handleContentSizeDirty() {
	Sprite::handleContentSizeDirty();

	_selection->setContentSize(_contentSize);
	_marked->setContentSize(_contentSize);
}

void Label::handleTransformDirty(const Mat4 &parent) {
	updateLabelScale(parent);
	Sprite::handleTransformDirty(parent);
}

void Label::handleGlobalTransformDirty(const Mat4 &parent) {
	if (!_transformDirty) {
		updateLabelScale(parent);
	}

	Sprite::handleGlobalTransformDirty(parent);
}

void Label::updateColor() {
	if (_format) {
		for (auto &it : _format->getData()->ranges) {
			if (!it.colorDirty) {
				it.color.r = uint8_t(_displayedColor.r * 255.0f);
				it.color.g = uint8_t(_displayedColor.g * 255.0f);
				it.color.b = uint8_t(_displayedColor.b * 255.0f);
			}
			if (!it.opacityDirty) {
				it.color.a = uint8_t(_displayedColor.a * 255.0f);
			}
		}
	}
	_vertexColorDirty = true;
}

void Label::updateVertexesColor() {
	if (_deferredResult) {
		_deferredResult->updateColor(_displayedColor);
	} else {
		if (!_colorMap.empty()) {
			_vertexes.updateColorQuads(_displayedColor, _colorMap);
		}
	}
}

void Label::updateQuadsForeground(font::FontController *controller, TextLayout *format,
		Vector<ColorMask> &colorMap) {
	writeQuads(_vertexes, format->getData(), colorMap, _textureLayer);
}

bool Label::checkVertexDirty() const { return _vertexesDirty || _labelDirty; }

NodeVisitFlags Label::processParentFlags(FrameInfo &info, NodeVisitFlags parentFlags) {
	if (_labelDirty) {
		updateLabel();
	}

	return Sprite::processParentFlags(info, parentFlags);
}

void Label::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	if (_deferred) {
		if (!_deferredResult
				|| (_deferredResult->isReady() && _deferredResult->getResult()->data.empty())) {
			return;
		}

		FrameContextHandle2d *handle = static_cast<FrameContextHandle2d *>(frame.currentContext);

		handle->commands->pushDeferredVertexResult(_deferredResult,
				frame.viewProjectionStack.back(), frame.modelTransformStack.back(), _normalized,
				buildCmdInfo(frame), _commandFlags);
	} else {
		Sprite::pushCommands(frame, flags);
	}
}

void Label::updateLabelScale(const Mat4 &parent) {
	Vec3 scale;
	parent.decompose(&scale, nullptr, nullptr);

	if (_scale.x != 1.f) {
		scale.x *= _scale.x;
	}
	if (_scale.y != 1.f) {
		scale.y *= _scale.y;
	}
	if (_scale.z != 1.f) {
		scale.z *= _scale.z;
	}

	auto density = std::min(std::min(scale.x, scale.y), scale.z);
	if (density != _labelDensity) {
		_labelDensity = density;
		setLabelDirty();
	}

	if (_labelDirty) {
		updateLabel();
	}
}

void Label::setAdjustValue(uint8_t val) {
	if (_adjustValue != val) {
		_adjustValue = val;
		setLabelDirty();
	}
}
uint8_t Label::getAdjustValue() const { return _adjustValue; }

bool Label::isOverflow() const {
	if (_format) {
		return _format->isOverflow();
	}
	return false;
}

size_t Label::getCharsCount() const { return _format ? _format->getData()->chars.size() : 0; }
size_t Label::getLinesCount() const { return _format ? _format->getData()->lines.size() : 0; }
Label::LineLayout Label::getLine(uint32_t num) const {
	if (_format) {
		if (num < _format->getData()->lines.size()) {
			return _format->getData()->lines[num];
		}
	}
	return LineLayout();
}

uint16_t Label::getFontHeight() const {
	auto l = _source->getLayout(_style.font);
	if (l.get()) {
		return l->getFontHeight();
	}
	return 0;
}

void Label::updateVertexes(FrameInfo &frame) {
	if (!_source) {
		return;
	}

	if (_labelDirty) {
		updateLabel();
	}

	if (!_format || _format->getData()->chars.size() == 0 || _string16.empty()) {
		_vertexes.clear();
		_labelDirty = false;
		_deferredResult = nullptr;
		return;
	}

	for (auto &it : _format->getData()->ranges) {
		auto dep = _source->addTextureChars(it.layout,
				SpanView<font::CharLayoutData>(_format->getData()->chars, it.start, it.count));
		if (dep) {
			emplace_ordered(_pendingDependencies, move(dep));
		}
	}

	if (_deferred) {
		_deferredResult =
				runDeferred(_director->getApplication()->getLooper(), _format, _displayedColor);
		_vertexes.clear();
		_vertexColorDirty = false;
	} else {
		_deferredResult = nullptr;
		updateQuadsForeground(_source, _format, _colorMap);
		_vertexColorDirty = true;
	}
}

void Label::onFontSourceUpdated() {
	setLabelDirty();
	_vertexesDirty = true;
}

void Label::onFontSourceLoaded() {
	if (_source) {
		setTexture(Rc<Texture>(_source->getTexture()));
		_vertexesDirty = true;
		setLabelDirty();
	}
}

void Label::onLayoutUpdated() { _labelDirty = false; }

Vec2 Label::getCursorPosition(uint32_t charIndex, bool front) const {
	if (_format) {
		auto d = _format->getData();
		if (charIndex < d->chars.size()) {
			auto &c = d->chars[charIndex];
			auto line = _format->getLine(charIndex);
			if (line) {
				return Vec2((front ? c.pos : c.pos + c.advance) / _labelDensity,
						_contentSize.height - line->pos / _labelDensity);
			}
		} else if (charIndex >= d->chars.size() && charIndex != 0) {
			auto &c = d->chars.back();
			auto &l = d->lines.back();
			if (c.charID == char16_t(0x0A)) {
				return getCursorOrigin();
			} else {
				return Vec2((c.pos + c.advance) / _labelDensity,
						_contentSize.height - l.pos / _labelDensity);
			}
		}
	}

	return Vec2::ZERO;
}

Vec2 Label::getCursorOrigin() const {
	switch (_alignment) {
	case TextAlign::Left:
	case TextAlign::Justify:
		return Vec2(0.0f / _labelDensity,
				_contentSize.height - _format->getHeight() / _labelDensity);
		break;
	case TextAlign::Center:
		return Vec2(_contentSize.width * 0.5f / _labelDensity,
				_contentSize.height - _format->getHeight() / _labelDensity);
		break;
	case TextAlign::Right:
		return Vec2(_contentSize.width / _labelDensity,
				_contentSize.height - _format->getHeight() / _labelDensity);
		break;
	}
	return Vec2::ZERO;
}

Pair<uint32_t, bool> Label::getCharIndex(const Vec2 &pos, font::CharSelectMode mode) const {
	if (!_format) {
		return pair(0, false);
	}

	auto ret = _format->getChar(pos.x * _labelDensity, _format->getHeight() - pos.y * _labelDensity,
			mode);
	if (ret.first == maxOf<uint32_t>()) {
		return pair(maxOf<uint32_t>(), false);
	} else if (ret.second == font::CharSelectMode::Prefix) {
		return pair(ret.first, false);
	} else {
		return pair(ret.first, true);
	}
}

core::TextCursor Label::selectWord(uint32_t chIdx) const {
	auto ret = _format->selectWord(chIdx);
	return core::TextCursor(ret.first, ret.second);
}

float Label::getMaxLineX() const {
	if (_format) {
		return _format->getMaxAdvance() / _labelDensity;
	}
	return 0.0f;
}

void Label::setDeferred(bool val) {
	if (val != _deferred) {
		_deferred = val;
		_vertexesDirty = true;
	}
}

void Label::setSelectionCursor(core::TextCursor c) {
	_selection->clear();
	_selection->setVisible(c != core::TextCursor::InvalidCursor && c.length > 0);
	if (_format && c != core::TextCursor::InvalidCursor && c.length > 0) {
		auto rects = _format->getLabelRects(c.start, c.start + c.length - 1, _labelDensity);
		for (auto &rect : rects) { _selection->emplaceRect(rect); }
		_selection->updateColor();
	}
	_selection->setTextCursor(c);
}

core::TextCursor Label::getSelectionCursor() const { return _selection->getTextCursor(); }

void Label::setSelectionColor(const Color4F &c) { _selection->setColor(c, false); }

Color4F Label::getSelectionColor() const { return _selection->getColor(); }

void Label::setMarkedCursor(core::TextCursor c) {
	_marked->clear();
	_marked->setVisible(c != core::TextCursor::InvalidCursor && c.length > 0);
	if (c != core::TextCursor::InvalidCursor && c.length > 0) {
		auto rects = _format->getLabelRects(c.start, c.start + c.length, _labelDensity);
		for (auto &rect : rects) { _marked->emplaceRect(rect); }
		_marked->updateColor();
	}
	_marked->setTextCursor(c);
}

core::TextCursor Label::getMarkedCursor() const { return _marked->getTextCursor(); }

void Label::setMarkedColor(const Color4F &c) { _marked->setColor(c, false); }

Color4F Label::getMarkedColor() const { return _marked->getColor(); }


LabelDeferredResult::~LabelDeferredResult() {
	if (_future) {
		delete _future;
		_future = nullptr;
	}
}

bool LabelDeferredResult::init(std::future<Rc<LabelResult>> &&future) {
	_future = new std::future<Rc<LabelResult>>(sp::move(future));
	return true;
}

bool LabelDeferredResult::acquireResult(
		const Callback<void(SpanView<InstanceVertexData>, Flags)> &cb) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		_result = _future->get();
		delete _future;
		_future = nullptr;
		DeferredVertexResult::handleReady();
	}
	cb(makeSpanView(&_result->data, 1), Immutable);
	return true;
}

void LabelDeferredResult::handleReady(Rc<LabelResult> &&res) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		delete _future;
		_future = nullptr;
	}
	if (res && (!_result || res.get() != _result.get())) {
		_result = move(res);
		DeferredVertexResult::handleReady();
	}
}

void LabelDeferredResult::updateColor(const Color4F &color) {
	acquireResult([](SpanView<InstanceVertexData>, Flags) { }); // ensure rendering was complete

	std::unique_lock<Mutex> lock(_mutex);
	if (_result) {
		VertexArray arr;
		arr.init(_result->data.data);
		arr.updateColorQuads(color, _result->colorMap);
		_result->data.data = arr.pop();
	}
}

Rc<VertexData> LabelDeferredResult::getResult() const {
	std::unique_lock<Mutex> lock(_mutex);
	return _result->data.data;
}

} // namespace stappler::xenolith::basic2d
