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
#include "XLFontLocale.h"
#include "XLDirector.h"
#include "XLInheritedStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

Label::Selection::~Selection() { }

bool Label::Selection::init() {
	if (!Sprite::init()) {
		return false;
	}

	return true;
}

void Label::Selection::clear() {
	_vertexes.clear();
	_bounds = Rect::ZERO;
}

void Label::Selection::emplaceRect(const Rect &rect) {
	// The layout hands rects down from the top of the box; the node measures from the bottom.
	const Rect placed(rect.origin.x, _contentSize.height - rect.origin.y - rect.size.height,
			rect.size.width, rect.size.height);

	_vertexes.addQuad().setGeometry(Vec4(placed.origin.x, placed.origin.y, _textureLayer, 1.0f),
			placed.size);

	// ZERO is the empty marker set by clear(); a real highlight always has a height.
	_bounds = _bounds.equals(Rect::ZERO) ? placed : _bounds.unionWithRect(placed);
}

// Every call rebuilds the quads, and a fresh quad starts fully transparent, while
// Sprite::updateColor only marks vertexes when the colour value changed - so re-apply it always.
void Label::Selection::updateColor() {
	Sprite::updateColor();
	markVertexColorDirty();
}

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
			if (!sprt::chars::isspace(c.charID) && c.charID != char16_t(0x0A)) {
				++ret;
			}
			end -= 1;
		}

		for (auto charIdx = start; charIdx < end; ++charIdx) {
			const font::CharLayoutData &c = format->chars[charIdx];
			if (!sprt::chars::isspace(c.charID) && c.charID != char16_t(0x0A)
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
		const font::CharLayoutData &c, uint16_t glyphId, const font::RangeLayoutData &range,
		const font::LineLayoutData &line, VertexArray::Quad &quad, float layer) {
	// c.yOffset is the HarfBuzz vertical glyph offset (mark positioning), y-up like the font, so it
	// is added to the glyph baseline y.
	switch (range.align) {
	case font::VerticalAlign::Sub:
		quad.drawChar(m, glyphId, c.pos,
				height - float(int16_t(line.pos) - int16_t(m.descender * 2 / 3)) + c.yOffset,
				range.color, range.decoration, c.face, layer);
		break;
	case font::VerticalAlign::Super:
		quad.drawChar(m, glyphId, c.pos,
				height - float(int16_t(line.pos) - int16_t(m.ascender * 2 / 3)) + c.yOffset,
				range.color, range.decoration, c.face, layer);
		break;
	default:
		quad.drawChar(m, glyphId, c.pos, height - line.pos + c.yOffset, range.color,
				range.decoration, c.face, layer);
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
			// `c.gid` is the glyph index to render; 0 means "no glyph" - skip it like whitespace.
			if (c.gid != 0 && !sprt::chars::isspace(c.charID) && c.charID != char16_t(0x0A)
					&& c.charID != char16_t(0x00AD)
					&& c.charID != font::CharLayoutData::InvalidChar) {
				auto quad = vertexes.addQuad();
				Label_pushColorMap(*it.range, colorMap);
				Label_writeTextureQuad(format->height, metrics, c, c.gid, *it.range, *it.line, quad,
						layer);
			}
		}

		if (it.line->start + it.line->count == end) {
			const font::CharLayoutData &c = format->chars[end - 1];
			if (c.charID == char16_t(0x00AD)) {
				// render a real hyphen glyph in place of the soft-hyphen at a line break
				uint16_t face = 0;
				auto dash = targetRange->layout->getChar('-', face);

				if (dash.charID == '-' && dash.glyphIndex != 0) {
					auto quad = vertexes.addQuad();
					Label_pushColorMap(*it.range, colorMap);
					Label_writeTextureQuad(format->height, metrics, c, dash.glyphIndex, *it.range,
							*it.line, quad, layer);
				}
			}
			end -= 1;
		}

		if (it.count() > 0 && it.range->decoration != font::TextDecoration::None) {
			auto chstart = it.start();
			auto chend = it.end();
			while (chstart < chend && sprt::chars::isspace(format->chars[chstart].charID)) {
				++chstart;
			}

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
		const font::TextLayoutData<mem_std::Interface> *format, Vector<ColorMask> &colorMap,
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

// Bridges the content measurement protocol (System::handleMeasure /
// handleLayoutApplied) to the label's own shaping machinery; installed by
// Label::init on every label, so any layout engine can measure text
class LabelMeasureSystem : public System {
public:
	virtual ~LabelMeasureSystem() = default;

	virtual bool init() override {
		if (!System::init()) {
			return false;
		}
		_systemFlags = SystemFlags::HandleMeasure;
		return true;
	}

	virtual bool handleMeasure(const MeasureConstraints &c, Size2 &result) override {
		result = static_cast<Label *>(_owner)->measureContent(c);
		return true;
	}

	virtual void handleLayoutApplied(const Size2 &size) override {
		static_cast<Label *>(_owner)->applyMeasuredSize(size);
	}
};

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
	addSystem(Rc<LabelMeasureSystem>::create());

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

	// a (re-)attached label may face a different set of inherited-style
	// components in its new ancestor chain
	setLabelDirty();

	if (!_source) {
		auto source = _director->getApplication()->getExtension<font::FontController>();
		if (source) {
			_listener->clear();
			_localeDelegate = nullptr; // the clear above took it with everything else

			_listener->listenForEventWithObject(font::FontController::onFontSourceUpdated, source,
					[this](const Event &) { onFontSourceUpdated(); });

			if (source->isLoaded()) {
				setTexture(Rc<Texture>(source->getTexture()));
			} else {
				_listener->listenForEventWithObject(font::FontController::onLoaded, source,
						[this](const Event &) { onFontSourceUpdated(); }, true);
			}

			_source = source;
		}
	}

	/* Relocalizes the window on a language change: `_string16` keeps `@Locale:Key` unresolved and
	tags expand during layout, so marking the label dirty is enough. Registered after the
	`_listener->clear()` above, which drops every delegate, and guarded against a double enter. */
	if (!_localeDelegate) {
		_localeDelegate = _listener->listenForEvent(locale::onLocale,
				[this](const Event &) { handleLocaleChanged(); });
	}
	applyLocaleTextFeatures();
}

/* Enables shaping and bidi for RTL locales (both cost time per layout, so they follow the locale).
The base direction is `Neutral` (CSS `dir=auto`, UAX #9 P2-P3), not `RightToLeft`: untranslated
Latin text such as paths must keep its neutral characters in place.
`_localeTextFeatures` records what the locale enabled, so switching back turns off only that. */
void Label::applyLocaleTextFeatures() {
	const bool rtl = (locale::getTextDirection() == font::TextDirection::RightToLeft);

	setTextDirection(rtl ? font::TextDirection::Neutral : font::TextDirection::LeftToRight);

	if (rtl && !_localeTextFeatures) {
		_localeTextFeatures = true;
		setBidiEnabled(true);
		setShapingEnabled(true);
	} else if (!rtl && _localeTextFeatures) {
		_localeTextFeatures = false;
		setBidiEnabled(false);
		setShapingEnabled(false);
	}
}

void Label::handleLocaleChanged() {
	applyLocaleTextFeatures();
	setLabelDirty();
}

void Label::handleExit() { Sprite::handleExit(); }

void Label::handleComponentsDirty(const ComponentMask &mask) {
	Sprite::handleComponentsDirty(mask);

	// Inherited-style components on the label's own node changed (typically by
	// ui::StyleResolver) - re-shape with the new effective style. Changes on ancestors are not
	// tracked here (see XLInheritedStyle.h).
	if (mask.contains(InheritedColorStyle::Id.value) || mask.contains(InheritedFontStyle::Id.value)
			|| mask.contains(InheritedTextStyle::Id.value)) {
		setLabelDirty();
	}
}

void Label::tryUpdateLabel() {
	if (_parent) {
		updateLabelScale(_parent->getNodeToWorldTransform());
	}
	if (_labelDirty) {
		updateLabel();
	}
}

Size2 Label::measureContent(const MeasureConstraints &c) {
	if (!_source || !_source->isLoaded()) {
		return getContentSize();
	}

	if (_parent) {
		updateLabelDensity(_parent->getNodeToWorldTransform());
	}

	// Anything that would change the answer bumps the revision or the density; either one throws
	// the whole cache away rather than trying to keep part of it.
	if (_measureRevision != getLabelRevision() || _measureDensity != _labelDensity) {
		_measureCache.clear();
		_measureRevision = getLabelRevision();
		_measureDensity = _labelDensity;
	}

	for (auto &it : _measureCache) {
		if (it.mode == c.mode && it.maxWidth == c.maxWidth) {
			return it.result;
		}
	}

	auto remember = [&](Size2 result) {
		if (_measureCache.size() >= MaxMeasureCache) {
			_measureCache.erase(_measureCache.begin());
		}
		_measureCache.emplace_back(MeasureCacheEntry{c.mode, c.maxWidth, result});
		return result;
	};

	if (_string16.empty()) {
		return remember(Size2(0.0f, getFontHeight() / _labelDensity));
	}

	auto request = font::Formatter::ContentRequest::Normal;
	const float savedWidth = _width;
	switch (c.mode) {
	case MeasureMode::Normal:
		// Formatter's width is uint16_t: unconstrained must be 0 (no wrap)
		_width = (c.maxWidth != maxOf<float>()) ? c.maxWidth : 0.0f;
		break;
	case MeasureMode::MinContent:
		request = font::Formatter::ContentRequest::Minimize;
		_width = 0.0f;
		break;
	case MeasureMode::MaxContent:
		request = font::Formatter::ContentRequest::Maximize;
		_width = 0.0f;
		break;
	}

	auto spec = Rc<font::TextLayout>::alloc(_source, _string16.size(), _compiledStyles.size() + 1);

	// mirror updateLabel's style setup so the measurement is bit-identical
	// to what applyLayout will later produce
	_compiledStyles = compileStyle();
	_style.text.whiteSpace = font::WhiteSpace::PreWrap;

	const bool ok = updateFormatSpec(spec, _compiledStyles, _labelDensity, _adjustValue, request);
	_width = savedWidth;

	if (!ok) {
		// Not cached: an overflowing measurement is a failure, not an answer.
		return getContentSize();
	}
	if (spec->empty()) {
		return remember(Size2(0.0f, getFontHeight() / _labelDensity));
	}
	return remember(Size2(spec->getWidth() / _labelDensity, spec->getHeight() / _labelDensity));
}

void Label::applyMeasuredSize(const Size2 &size) {
	// The width being written is the measurement, so setLabelDirty must not announce it as a
	// change to it; see setLabelDirty.
	_applyingMeasuredSize = true;
	if (_width != size.width) {
		setWidth(size.width);
	}
	tryUpdateLabel();
	// the assigned box wins over the shaped extent committed by applyLayout
	setContentSize(size);
	_applyingMeasuredSize = false;
}

void Label::setLabelDirty() {
	LabelBase::setLabelDirty();
	markSceneChanged();

	// See the note on the declaration. Not while a measured box is being applied: that write is
	// the answer to a measurement, not a change to one.
	if (!_applyingMeasuredSize) {
		markIntrinsicSizeDirty();
	}
}

void Label::setStyle(const DescriptionStyle &style) {
	_style = style;

	setColor(Color4F(_style.text.color, _style.text.opacity), true);

	setLabelDirty();
}

const Label::DescriptionStyle &Label::getStyle() const { return _style; }

// Same call, with the frame's spawn counter bumped first. Kept out of runDeferred, which is
// virtual, so an override cannot lose the count.
Rc<LabelDeferredResult> Label::runDeferredCounted(sprt::dispatch::Looper *queue, TextLayout *format,
		const Color4F &color) {
#if XL_FRAME_ACCOUNT
	if (_director) {
		_director->countDeferredSpawned();
	}
#endif
	return runDeferred(queue, format, color);
}

Rc<LabelDeferredResult> Label::runDeferred(sprt::dispatch::Looper *queue, TextLayout *format,
		const Color4F &color) {
	Rc<LabelDeferredResult> ret = Rc<LabelDeferredResult>::create();
	queue->performAsync(
			[format = Rc<Label::TextLayout>(format), color, ret, layer = _textureLayer]() mutable {
#if XL_FRAME_ACCOUNT
		// Counted too: a node view at full detail is a dozen Labels, so text can outweigh
		// tesselation.
		const auto workStart = core::getAccountClock();
#endif
		auto result = Label::writeResult(format, color, layer);
#if XL_FRAME_ACCOUNT
		ret->setWorkTime(core::getAccountClock() - workStart);
#endif
		ret->setResult(sp::move(result));
	}, ret);
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
		markVertexesDirty();
	} else {
		markVertexesDirty();
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

void Label::makeEffectiveStyle(font::LabelBase::EffectiveStyle &out) const {
	LabelBase::makeEffectiveStyle(out);

	// Overlay inherited-style components (own node first, then ancestors — see
	// XLInheritedStyle.h): a defined inherited value wins over the label's stored
	// explicit value; the stored fields are left untouched, so they take effect
	// again as soon as the components disappear.
	auto color = accumulateInheritedStyle<InheritedColorStyle>(this);
	auto font = accumulateInheritedStyle<InheritedFontStyle>(this);
	auto text = accumulateInheritedStyle<InheritedTextStyle>(this);

	if (color.defined & InheritedColorStyle::DefinedColor) {
		out.style.text.color = color.color;
		// keep the inherited color across _displayedColor refreshes (updateColor
		// skips ranges with colorDirty)
		out.style.colorDirty = true;
	}
	if (color.defined & InheritedColorStyle::DefinedOpacity) {
		out.style.text.opacity = color.opacity;
		out.style.opacityDirty = true;
	}

	if (font.defined & InheritedFontStyle::DefinedFontSize) {
		out.style.font.fontSize = font.fontSize;
	}
	if (font.defined & InheritedFontStyle::DefinedFontStyle) {
		out.style.font.fontStyle = font.fontStyle;
	}
	if (font.defined & InheritedFontStyle::DefinedFontWeight) {
		out.style.font.fontWeight = font.fontWeight;
	}
	if (font.defined & InheritedFontStyle::DefinedFontStretch) {
		out.style.font.fontStretch = font.fontStretch;
	}
	if (font.defined & InheritedFontStyle::DefinedFontGrade) {
		out.style.font.fontGrade = font.fontGrade;
	}
	if (font.defined & InheritedFontStyle::DefinedFontVariant) {
		out.style.font.fontVariant = font.fontVariant;
	}
	if (font.defined & InheritedFontStyle::DefinedFontFamily) {
		out.fontFamilyStorage = sp::move(font.fontFamily);
	}

	if (text.defined & InheritedTextStyle::DefinedTextTransform) {
		out.style.text.textTransform = text.textTransform;
	}
	if (text.defined & InheritedTextStyle::DefinedTextDecoration) {
		out.style.text.textDecoration = text.textDecoration;
	}
	if (text.defined & InheritedTextStyle::DefinedWhiteSpace) {
		out.style.text.whiteSpace = text.whiteSpace;
	}
	if (text.defined & InheritedTextStyle::DefinedHyphens) {
		out.style.text.hyphens = text.hyphens;
	}
	if (text.defined & InheritedTextStyle::DefinedVerticalAlign) {
		out.style.text.verticalAlign = text.verticalAlign;
	}
	if (text.defined & InheritedTextStyle::DefinedTextAlign) {
		out.alignment = text.textAlign;
	}

	/* CSS `direction` and `unicode-bidi`. `direction` sets the base direction. On the label root
	`plaintext` means "resolve the base from the content" (`Neutral` with bidi on), not an FSI...PDI
	isolate: UAX #9 P2 skips isolates when looking for the paragraph base. Precedence: CSS, then
	the caller, then the locale (applyLocaleTextFeatures). */
	if (text.defined & InheritedTextStyle::DefinedDirection) {
		out.direction = text.direction;
		if (text.direction == font::TextDirection::RightToLeft) {
			// An RTL base is honoured only by the bidi pass, and RTL scripts need shaping to join.
			out.bidiEnabled = true;
			out.shapingEnabled = true;
		}
	}

	/* `unicode-bidi` must be applied after `direction`: `plaintext` overrides an inherited
	`direction: rtl`, not the other way round. */
	if (text.defined & InheritedTextStyle::DefinedBidi) {
		if (text.bidi == font::BidiMode::Plaintext) {
			out.direction = font::TextDirection::Neutral;
			out.bidiEnabled = true;
			out.bidiMode = font::BidiMode::Normal; // no isolate around the whole paragraph
			/* No shaping: `plaintext` concerns the base direction only. Joining is enabled by the
			   `direction: rtl` branch above and by applyLocaleTextFeatures for an RTL locale. */
		} else {
			out.bidiMode = text.bidi;
			if (text.bidi != font::BidiMode::Normal) {
				out.bidiEnabled = true;
			}
		}
	}

	if (text.defined & InheritedTextStyle::DefinedLineHeight) {
		out.lineHeight = text.lineHeight;
		out.lineHeightAbsolute = text.lineHeightAbsolute;
	}
}

void Label::handleContentSizeDirty() {
	Sprite::handleContentSizeDirty();

	_selection->setContentSize(_contentSize);
	_marked->setContentSize(_contentSize);

	// Highlight quads depend on the node height (label is Y-up, layout rects are Y-down), which is
	// only known here, one phase after applyLayout; rebuild an open selection to keep them in step
	// on the first layout and on every resize.
	if (_selection->getTextCursor() != core::TextCursor::InvalidCursor) {
		setSelectionCursor(_selection->getTextCursor());
	}
	if (_marked->getTextCursor() != core::TextCursor::InvalidCursor) {
		setMarkedCursor(_marked->getTextCursor());
	}
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
	markVertexColorDirty();
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

void Label::refreshPendingDependencies() {
	if (!_source || !_format) {
		return;
	}

	// An upload started after layout replaces the atlas instance, and a label with nothing to
	// re-shape would keep drawing CharIds the atlas cannot resolve. So the gate is re-checked every
	// frame against these quads' glyph generation (one atomic load once the atlas catches up).
	if (_source->isGlyphGenerationUploaded(_glyphGeneration)) {
		return;
	}

	if (auto dep = _source->acquireGatingDependency()) {
		emplace_ordered(_pendingDependencies, move(dep));
	}
}

NodeVisitFlags Label::processParentFlags(FrameInfo &info, NodeVisitFlags parentFlags) {
	updateLabelDensity(info.modelTransformStack.back());

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
	updateLabelDensity(parent);

	if (_labelDirty) {
#if XL_FRAME_ACCOUNT
		auto &account = getVisitAccount();
		++account.labelShapes;
		account.labelShapeChars += uint32_t(_string16.size());
		const auto shapeStart = core::getAccountClock();
#endif
		updateLabel();
#if XL_FRAME_ACCOUNT
		account.labelShapeNs += core::getAccountClock() - shapeStart;
#endif
	}
}

void Label::updateLabelDensity(const Mat4 &parent) {
#if XL_FRAME_ACCOUNT
	auto &densityAccount = getVisitAccount();
	++densityAccount.labelDensity;
	const auto densityStart = core::getAccountClock();
	struct DensityClose {
		VisitAccount *a;
		uint64_t start;
		~DensityClose() { a->labelDensityNs += core::getAccountClock() - start; }
	} densityClose{&densityAccount, densityStart};
#endif
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

	auto density = sprt::min(sprt::min(scale.x, scale.y), scale.z);
	if (density != _labelDensity) {
#if XL_FRAME_ACCOUNT
		// A change under 1% of the old value is jitter rather than a new scale - see VisitAccount.
		if (_labelDensity > 0.0f && sprt::abs(density - _labelDensity) < _labelDensity * 0.01f) {
			++getVisitAccount().labelShapeJitter;
		}
#endif
		_labelDensity = density;
		setLabelDirty();
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
	// No controller before handleEnter - a label measured before it joined a scene has no height
	// to answer with, and answering 0 beats dereferencing null.
	if (!_source) {
		return 0;
	}

	EffectiveStyle eff;
	makeEffectiveStyle(eff);
	if (!eff.fontFamilyStorage.empty()) {
		// fontFamily is a non-owning view: re-point it at the owning storage only after the
		// EffectiveStyle reached its final address (as updateFormatSpec does)
		eff.style.font.fontFamily = eff.fontFamilyStorage;
	}
	eff.style.font.density = _labelDensity;

	auto l = _source->getLayout(eff.style.font);
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

	// Remember which glyph set these quads belong to: they carry CharIds, not atlas coordinates,
	// and stay drawable only while the atlas holds that generation.
	_glyphGeneration = _source->getGlyphGeneration();

	if (_deferred) {
		_deferredResult = runDeferredCounted(_director->getApplication()->getLooper(), _format,
				_displayedColor);
		_vertexes.clear();
		_vertexColorDirty = false;
	} else {
		_deferredResult = nullptr;
		updateQuadsForeground(_source, _format, _colorMap);
		markVertexColorDirty();
	}
}

void Label::onFontSourceUpdated() {
	// (Re)bind the atlas texture: needed when the controller loads after handleEnter (the remote
	// client), where the texture was never set.
	if (_source) {
		setTexture(Rc<Texture>(_source->getTexture()));
	}
	setLabelDirty();
	markVertexesDirty();
	_deferredResult = nullptr;
}

void Label::onFontSourceLoaded() {
	if (_source) {
		setTexture(Rc<Texture>(_source->getTexture()));
		markVertexesDirty();
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
				// The caret edge follows glyph direction: `front` is the left edge for an LTR
				// glyph, the right edge for an RTL one.
				const bool leftEdge = (front != bool(c.bidiLevel & 1));
				return Vec2((leftEdge ? c.pos : c.pos + c.advance) / _labelDensity,
						_contentSize.height - line->pos / _labelDensity);
			}
		} else if (charIndex >= d->chars.size() && charIndex != 0 && d->chars.empty()) {
			// A non-empty string can lay out to nothing (every char missing from the font), so
			// answer the origin instead of calling back() on empty vectors.
			return getCursorOrigin();
		} else if (charIndex >= d->chars.size() && charIndex != 0) {
			auto &c = d->chars.back();
			auto &l = d->lines.back();
			if (c.charID == char16_t(0x0A)) {
				return getCursorOrigin();
			} else {
				// past-the-end caret = the trailing (back) edge of the last glyph, RTL-aware
				const bool leftEdge = bool(c.bidiLevel & 1);
				return Vec2((leftEdge ? c.pos : c.pos + c.advance) / _labelDensity,
						_contentSize.height - l.pos / _labelDensity);
			}
		}
	}

	return Vec2::ZERO;
}

Vec2 Label::getCursorOrigin() const {
	// No layout yet (e.g. a fresh empty text field asking for its caret); getCursorPosition()
	// guards the same way.
	if (!_format) {
		return Vec2::ZERO;
	}

	/* `start`/`end` are direction-relative. For a `Neutral` base the first line's resolved
	direction is used, which is right for an empty or single-paragraph field. */
	auto base = _direction;
	if (base == font::TextDirection::Neutral) {
		auto data = _format->getData();
		base = (data && !data->lines.empty()) ? data->lines.front().direction
											  : font::TextDirection::LeftToRight;
	}
	const bool rtl = (base == font::TextDirection::RightToLeft);

	auto alignment = _alignment;
	switch (alignment) {
	case TextAlign::Start: alignment = rtl ? TextAlign::Right : TextAlign::Left; break;
	case TextAlign::End: alignment = rtl ? TextAlign::Left : TextAlign::Right; break;
	default: break;
	}

	switch (alignment) {
	case TextAlign::Left:
	case TextAlign::Justify:
	case TextAlign::Start:
		return Vec2(0.0f / _labelDensity,
				_contentSize.height - _format->getHeight() / _labelDensity);
		break;
	case TextAlign::Center:
		return Vec2(_contentSize.width * 0.5f / _labelDensity,
				_contentSize.height - _format->getHeight() / _labelDensity);
		break;
	case TextAlign::Right:
	case TextAlign::End:
		return Vec2(_contentSize.width / _labelDensity,
				_contentSize.height - _format->getHeight() / _labelDensity);
		break;
	}
	return Vec2::ZERO;
}

Rect Label::getInlineObjectRect(uint32_t index) const {
	if (!_format || index >= _inlineObjects.size()) {
		return Rect::ZERO;
	}

	auto rect = _format->getObjectRect(_inlineObjects[index].rangeIndex, _labelDensity);
	if (rect.size.width <= 0.0f && rect.size.height <= 0.0f) {
		return Rect::ZERO;
	}

	// The layout measures downward from the top of the text; this node is Y-up from its own
	// origin, exactly the flip getCursorPosition makes for a caret.
	rect.origin.y = _contentSize.height - rect.origin.y - rect.size.height;
	return rect;
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
	// An empty label has no layout, and getCharIndex() answers {0, false} for it rather than maxOf,
	// so callers reach here anyway.
	if (!_format) {
		return core::TextCursor::InvalidCursor;
	}
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
		markVertexesDirty();
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

Rect Label::getSelectionRect() const { return _selection->getBounds(); }

void Label::setMarkedCursor(core::TextCursor c) {
	_marked->clear();
	_marked->setVisible(c != core::TextCursor::InvalidCursor && c.length > 0);
	if (_format && c != core::TextCursor::InvalidCursor && c.length > 0) {
		auto rects = _format->getLabelRects(c.start, c.start + c.length, _labelDensity);
		for (auto &rect : rects) { _marked->emplaceRect(rect); }
		_marked->updateColor();
	}
	_marked->setTextCursor(c);
}

core::TextCursor Label::getMarkedCursor() const { return _marked->getTextCursor(); }

void Label::setMarkedColor(const Color4F &c) { _marked->setColor(c, false); }

Color4F Label::getMarkedColor() const { return _marked->getColor(); }

Rect Label::getMarkedRect() const { return _marked->getBounds(); }


LabelDeferredResult::~LabelDeferredResult() { }

bool LabelDeferredResult::init() { return true; }

bool LabelDeferredResult::acquireResult(
		const Callback<void(SpanView<InstanceVertexData>, Flags)> &cb) {
	//log::debug("Label", "acquireResult: ", this);
	_timeline.wait(SignalValue);
	cb(makeSpanView(&_result->data, 1), Immutable);
	return true;
}

void LabelDeferredResult::setResult(Rc<LabelResult> &&res) {
	_result = move(res);
	_timeline.signal(1);
	//log::debug("Label", "setResult: ", this);
}

void LabelDeferredResult::updateColor(const Color4F &color) {
	_timeline.wait(SignalValue);

	if (_result) {
		VertexArray arr;
		arr.init(_result->data.data);
		arr.updateColorQuads(color, _result->colorMap);
		_result->data.data = arr.pop();
	}
}

Rc<VertexData> LabelDeferredResult::getResult() const {
	_timeline.wait(SignalValue);
	return _result ? _result->data.data : nullptr;
}

} // namespace stappler::xenolith::basic2d
