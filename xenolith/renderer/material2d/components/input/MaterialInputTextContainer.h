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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTTEXTCONTAINER_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTTEXTCONTAINER_H_

#include "MaterialLabel.h"
#include "MaterialIconSprite.h"
#include "XLTextInputManager.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC InputTextContainer : public Node {
public:
	virtual ~InputTextContainer();

	virtual bool init() override;
	virtual void update(const UpdateTime &time) override;
	virtual void handleContentSizeDirty() override;

	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	TypescaleLabel *getLabel() const { return _label; }

	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }

	virtual void setCursor(TextCursor);
	virtual TextCursor getCursor() const { return _cursor; }

	virtual void handleLabelChanged();

	virtual TextCursor getCursorForPosition(const Vec2 &);

	virtual bool hasHorizontalOverflow() const;
	virtual void moveHorizontalOverflow(float d);

	virtual IconSprite *getTouchedCursor(const Vec2 &, float = 4.0f);

	virtual bool handleLongPress(const Vec2 &, uint32_t tickCount);

	virtual bool handleSwipeBegin(const Vec2 &);
	virtual bool handleSwipe(const Vec2 &, const Vec2 &);
	virtual bool handleSwipeEnd(const Vec2 &);

	virtual void touchPointers();

	virtual void setCursorCallback(Function<void(TextCursor)> &&);
	virtual const Function<void(TextCursor)> &getCursorCallback() const;

protected:
	virtual void updateCursorPosition();
	virtual void updateCursorPointers();
	virtual void runAdjustLabel(float pos);

	void scheduleCursorPointer();
	void unscheduleCursorPointer();
	void setPointerEnabled(bool value);

	TypescaleLabel *_label = nullptr;
	Vec2 _adjustment = Vec2::ZERO;
	Layer *_caret = nullptr;

	IconSprite *_selectedPointer = nullptr;

	IconSprite *_cursorPointer = nullptr;
	IconSprite *_selectionPointerStart = nullptr;
	IconSprite *_selectionPointerEnd = nullptr;
	DynamicStateSystem *_scissorComponent = nullptr;

	float _cursorAnchor = 1.2f;
	TextCursor _cursor = TextCursor::InvalidCursor;
	bool _enabled = false;
	bool _cursorDirty = false;
	bool _pointerEnabled = false;
	Function<void(TextCursor)> _cursorCallback;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTTEXTCONTAINER_H_ */
