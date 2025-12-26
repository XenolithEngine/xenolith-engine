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

#ifndef EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTLISTENERVIEW_H_
#define EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTLISTENERVIEW_H_

#include "XLRTCommonView.h"
#include "XLRTCommonSource.h"
#include "XLEventListener.h"
#include "XL2dVectorSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

class SP_PUBLIC ListenerView : public CommonView {
public:
	static EventHeader onSelection;
	static EventHeader onExternalLink;

	enum class SelectMode {
		Full,
		Para,
		Indexed
	};

	struct SelectionPosition {
		size_t object;
		uint32_t position;
	};

	class Selection : public basic2d::Sprite {
	public:
		virtual bool init(ListenerView *);

		virtual void clearSelection();
		virtual void selectLabel(const Object *, const Vec2 &);
		virtual void selectWholeLabel();

		virtual bool onTap(int, Vec2);

		virtual bool onPressBegin(Vec2);
		virtual bool onLongPress(Vec2, TimeInterval, int);
		virtual bool onPressEnd(Vec2, TimeInterval);
		virtual bool onPressCancel(Vec2);

		virtual bool onSwipeBegin(Vec2);
		virtual bool onSwipe(Vec2, Vec2);
		virtual bool onSwipeEnd(Vec2);

		virtual void setEnabled(bool);
		virtual bool isEnabled() const;

		virtual void setMode(SelectMode);
		virtual SelectMode getMode() const;

		virtual bool hasSelection() const;

		virtual String getSelectedString(size_t maxWords) const;
		virtual Pair<SelectionPosition, SelectionPosition> getSelectionPosition() const;

		virtual StringView getSelectedHash() const;
		virtual size_t getSelectedSourceIndex() const;

		virtual bool shouldReceiveTouch(Vec2) const;

	protected:
		virtual const Object *getSelectedObject(Result *, Vec2) const;
		virtual const Object *getSelectedObject(Result *, Vec2, size_t pos, int32_t offset) const;

		virtual void emplaceRect(const Rect &, size_t idx, size_t count);
		virtual void updateVertexes(FrameInfo &frame) override;

		ListenerView *_view = nullptr;
		const Object *_object = nullptr;
		size_t _index = 0;
		Pair<SelectionPosition, SelectionPosition> _selectionBounds;

		basic2d::VectorSprite *_markerStart = nullptr;
		basic2d::VectorSprite *_markerEnd = nullptr;
		basic2d::VectorSprite *_markerTarget = nullptr;
		bool _enabled = false;
		SelectMode _mode = SelectMode::Full;
	};

	virtual ~ListenerView();
	virtual bool init(Layout, CommonSource * = nullptr, Vector<String> &&ids = {}) override;

	virtual void handleExit() override;

	virtual void setLayout(Layout l) override;
	virtual void setUseSelection(bool);

	virtual void disableSelection();
	virtual bool isSelectionEnabled() const;

	virtual void setSelectMode(SelectMode);
	virtual SelectMode getSelectMode() const;

	virtual String getSelectedString(size_t maxWords = maxOf<size_t>()) const;
	virtual Pair<SelectionPosition, SelectionPosition> getSelectionPosition() const;

	virtual StringView getSelectedHash() const;
	virtual size_t getSelectedSourceIndex() const;

protected:
	virtual void onTap(int count, Vec2 loc) override;

	virtual void onObjectPressEnd(Vec2, const Object &) override;
	virtual void onLink(StringView ref, StringView target, WideStringView text, Vec2);

	virtual bool onSwipeEventBegin(uint32_t id, Vec2 loc, Vec2 d, Vec2 v) override;
	virtual bool onSwipeEvent(uint32_t id, Vec2 loc, Vec2 d, Vec2 v) override;
	virtual bool onSwipeEventEnd(uint32_t id, Vec2 loc, Vec2 d, Vec2 v) override;

	virtual bool onPressBegin(Vec2) override;
	virtual bool onLongPress(Vec2, TimeInterval, int count) override;
	virtual bool onPressEnd(Vec2, TimeInterval) override;
	virtual bool onPressCancel(Vec2, TimeInterval) override;

	virtual void onPosition() override;
	virtual void onRenderer(RendererResult *, bool) override;

	bool _useSelection = false;

	Selection *_selection = nullptr;
};

} // namespace stappler::xenolith::richtext

#endif /* EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTLISTENERVIEW_H_ */
