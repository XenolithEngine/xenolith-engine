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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DLABEL_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DLABEL_H_

#include "XL2dSprite.h"
#include "XLCoreTextInput.h"
#include "XLFontLabelBase.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class EventListener;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

struct SP_PUBLIC LabelResult : Ref {
	InstanceVertexData data;
	Vector<ColorMask> colorMap;
};

class SP_PUBLIC LabelDeferredResult : public DeferredVertexResult {
public:
	virtual ~LabelDeferredResult();

	bool init();

	virtual bool acquireResult(
			const Callback<void(SpanView<InstanceVertexData>, Flags)> &) override;

	void setResult(Rc<LabelResult> &&);

	void updateColor(const Color4F &);

	Rc<VertexData> getResult() const;

protected:
	Rc<LabelResult> _result;
};

class SP_PUBLIC Label : public Sprite, public font::LabelBase {
public:
	using TextLayout = font::TextLayout;
	using LineLayout = font::LineLayoutData;
	using TextAlign = font::TextAlign;

	using ColorMapVec = Vector<Vector<bool>>;

	class Selection : public Sprite {
	public:
		virtual ~Selection();
		virtual bool init() override;
		virtual void clear();
		virtual void emplaceRect(const Rect &);
		virtual void updateColor() override;

		virtual core::TextCursor getTextCursor() const { return _cursor; }
		virtual void setTextCursor(core::TextCursor c) { _cursor = c; }

	protected:
		virtual void updateVertexes(FrameInfo &frame) override;

		core::TextCursor _cursor = core::TextCursor::InvalidCursor;
	};

	static void writeQuads(VertexArray &vertexes,
			const font::TextLayoutData<mem_std::Interface> *format, Vector<ColorMask> &colorMap,
			float layer);
	static void writeQuads(VertexArray &vertexes,
			const font::TextLayoutData<memory::PoolInterface> *format, Vector<ColorMask> &colorMap,
			float layer);
	static Rc<LabelResult> writeResult(TextLayout *format, const Color4F &, float layer);

	virtual ~Label();

	virtual bool init() override;
	virtual bool init(StringView) override;
	virtual bool init(StringView, float w, TextAlign = TextAlign::Left);
	virtual bool init(font::FontController *, const DescriptionStyle & = DescriptionStyle(),
			StringView = StringView(), float w = 0.0f, TextAlign = TextAlign::Left);
	virtual bool init(const DescriptionStyle &, StringView = StringView(), float w = 0.0f,
			TextAlign = TextAlign::Left);

	virtual void handleEnter(xenolith::Scene *) override;
	virtual void handleExit() override;

	virtual void handleComponentsDirty(const ComponentMask &) override;

	virtual void tryUpdateLabel();

	// Measure the natural text size under the given constraints without
	// committing any node state (content measurement protocol; served to
	// layout engines via the label's internal HandleMeasure system)
	Size2 measureContent(const MeasureConstraints &);

	// Apply a size assigned by a layout engine: re-wrap the text to the new
	// width synchronously, then adopt the assigned box as contentSize
	void applyMeasuredSize(const Size2 &);

	virtual void setStyle(const DescriptionStyle &);
	virtual const DescriptionStyle &getStyle() const;

	virtual void handleContentSizeDirty() override;
	virtual void handleTransformDirty(const Mat4 &) override;
	virtual void handleGlobalTransformDirty(const Mat4 &) override;

	virtual void setAdjustValue(uint8_t);
	virtual uint8_t getAdjustValue() const;

	virtual bool isOverflow() const;

	virtual size_t getCharsCount() const;
	virtual size_t getLinesCount() const;
	virtual LineLayout getLine(uint32_t num) const;

	virtual uint16_t getFontHeight() const;

	virtual Vec2 getCursorPosition(uint32_t charIndex, bool prefix = true) const;
	virtual Vec2 getCursorOrigin() const;

	/*
	returns character index in FormatSpec for position in label or maxOf<uint32_t>()

	pair.second - true if index match suffix or false if index match prefix

	use convertToNodeSpace for input position
	*/
	virtual Pair<uint32_t, bool> getCharIndex(const Vec2 &,
			font::CharSelectMode = font::CharSelectMode::Best) const;

	virtual core::TextCursor selectWord(uint32_t) const;

	virtual float getMaxLineX() const;

	virtual void setDeferred(bool);
	virtual bool isDeferred() const { return _deferred; }

	virtual void setSelectionCursor(core::TextCursor);
	virtual core::TextCursor getSelectionCursor() const;

	virtual void setSelectionColor(const Color4F &);
	virtual Color4F getSelectionColor() const;

	virtual void setMarkedCursor(core::TextCursor);
	virtual core::TextCursor getMarkedCursor() const;

	virtual void setMarkedColor(const Color4F &);
	virtual Color4F getMarkedColor() const;

protected:
	using Sprite::init;

	virtual Rc<LabelDeferredResult> runDeferred(sprt::dispatch::Looper *, TextLayout *format,
			const Color4F &color);

	virtual void applyLayout(TextLayout *);

	virtual void makeEffectiveStyle(font::LabelBase::EffectiveStyle &) const override;

	virtual void updateLabel();
	virtual void onFontSourceUpdated();
	virtual void onFontSourceLoaded();
	virtual void onLayoutUpdated();
	virtual void updateColor() override;
	virtual void updateVertexes(FrameInfo &frame) override;
	virtual void updateVertexesColor() override;

	virtual void updateQuadsForeground(font::FontController *, TextLayout *, Vector<ColorMask> &);

	virtual bool checkVertexDirty() const override;
	virtual void refreshPendingDependencies() override;

	virtual NodeVisitFlags processParentFlags(FrameInfo &info, NodeVisitFlags parentFlags) override;

	virtual void pushCommands(FrameInfo &, NodeVisitFlags flags) override;

	void updateLabelScale(const Mat4 &parent);

	// density refresh half of updateLabelScale: no re-shaping, only marks
	// the label dirty when the accumulated world scale changed
	void updateLabelDensity(const Mat4 &parent);

	EventListener *_listener = nullptr;
	Time _quadRequestTime;
	Rc<font::FontController> _source;
	// Glyph generation this label's quads were laid out against. Its CharIds are only resolvable
	// while the atlas holds that generation - see refreshPendingDependencies().
	uint64_t _glyphGeneration = 0;
	Rc<TextLayout> _format;
	Vector<ColorMask> _colorMap;

	bool _deferred = true;

	uint8_t _adjustValue = 0;
	size_t _updateCount = 0;

	Selection *_selection = nullptr;
	Selection *_marked = nullptr;

	Rc<LabelDeferredResult> _deferredResult;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DLABEL_H_ */
