/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNSELECTION_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNSELECTION_H_

#include "XLUiMarkdownFlow.h"
#include "XLUiScrollSystem.h"
#include "XLInputListener.h"
#include "XLHotkey.h"
#include "XL2dVectorSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class MarkdownView;

/* SELECTING TEXT IN A DOCUMENT THAT ALSO SCROLLS.

The whole difficulty of this class is in one sentence: `ui::ScrollSystem` IS an InputListener and
it already holds a swipe - on the view, for the document, and again inside a code block, for its
sideways pan. A drag across text has to become a selection before either of them decides it is a
scroll, and it has to do that without taking the mouse wheel away from them.

Three decisions carry that, and each is load-bearing:

 - DISPATCH ORDER. On one node the system with the HIGHER system priority is offered an event
   first, so this sits one above ScrollDefaultPriority. The number rather than the order of
   addition, because the style resolver removes and re-adds the ScrollSystem whenever `overflow`
   changes, and a re-added system re-inserts itself by priority.

 - THRESHOLD. Both ScrollSystems begin their swipe at the tap tolerance (12pt); this one begins at
   4, so a drag is a selection before it is ever a scroll. Inside a code block that matters twice
   over: `pre-scroll` is DEEPER in the tree and is therefore asked first, so with equal thresholds
   the code would jump sideways before selection took over.

 - CAPTURE. The first listener to claim a pointer keeps it, and the claim cannot be revoked, so
   the gesture callback must answer `true` for the whole drag: a single `false` tears the chain
   down for every listener at once. The ScrollSystem, having claimed nothing, receives a clean
   Cancelled and stops without a fling.

No scroll recognizer is registered here, which is precisely why the wheel keeps working; and
`setSwallowAllEvents` must never be called, because it would promote every Processed to Captured
and eat the wheel with it.

MOUSE AND FINGER ARE NOT THE SAME GESTURE. They cannot be told apart by button - Touch and
MouseLeft are one button - only by InputModifier::Touch. A mouse drag always selects. A finger
drag pans the document, because that is what a finger drag means everywhere else; selection on a
finger starts from a long press and is then carried by the two handles. */
class SP_PUBLIC MarkdownSelectionSystem : public InputListener {
public:
	static constexpr uint32_t MarkdownSelectionPriority = ScrollSystem::ScrollDefaultPriority + 1;

	// Below the 12pt tap tolerance both scrolls use; see the class comment.
	static constexpr float SelectionThreshold = 4.0f;

	// The same ramp ui::TextViewContainer and ui::DragScrollSystem use.
	static constexpr float AutoScrollSpeed = 300.0f;
	static constexpr float AutoScrollEdge = 48.0f;

	// A handle is small and dragged by a fingertip, so it answers well outside its own box.
	static constexpr float HandlePadding = 16.0f;

	virtual ~MarkdownSelectionSystem() = default;

	virtual bool init(NotNull<MarkdownView>);

	virtual void update(const UpdateTime &) override;

	// The range changed: put the handles where it now ends. Called by the view.
	void updateHandles();

	bool isDragging() const { return _dragging; }
	bool isTouchMode() const { return _touchMode; }

	// World position of a handle, or an invalid Vec2 when it is not shown. For a stand to assert
	// on: a handle in the wrong place is the one failure a screenshot alone would not name.
	Vec2 getHandlePosition(bool start) const;

protected:
	bool handleTap(const GestureTap &);
	bool handlePress(const GesturePress &);
	bool handleSwipe(const GestureSwipe &);
	bool handleMove(const GestureData &);
	bool handleSelectionHotkey(HotkeyId, const InputEvent &);

	void makeHandles();

	// Vec2::INVALID stops the pull; it is a pair of NaNs, so the test is isValid().
	void setAutoScrollTarget(Vec2 world);

	void extendTo(uint32_t position);

	MarkdownView *_view = nullptr;
	basic2d::VectorSprite *_handleStart = nullptr;
	basic2d::VectorSprite *_handleEnd = nullptr;

	uint32_t _anchor = 0;
	Vec2 _autoScrollTarget = Vec2::INVALID;
	bool _dragging = false;
	bool _touchMode = false;
	uint8_t _handleDrag = 0; // 0 none, 1 the start edge, 2 the end edge
	HotkeyId _copySourceHotkey;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNSELECTION_H_
