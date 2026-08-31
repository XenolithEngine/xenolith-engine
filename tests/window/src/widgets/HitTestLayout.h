/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_WIDGETS_HITTESTLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_HITTESTLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* The hit-test registry itself, with no consumer in the way.

Drag and drop, context menus and tooltips all ask one question - "what is under this point" - and all
three are answered from one per-frame registry a node publishes itself into during its own visit
(HitTestFlags, InputListenerStorage::foreachHitTest). Every rule that registry has is invisible from
the outside: a node that answers when it should not looks exactly like one that answers correctly
until you ask what answered. So this stand asks the registry directly and reports what came back.

The nodes, and what each one is the only witness for:

  * `#region` - the base everything else sits on. Whatever another node fails to answer for must
    fall through to this one, which is how "nothing was found" is told from "the wrong thing was";
  * `#under` / `#over` - overlapping, with distinct ZOrders. Registration order is paint order, so
    the walk runs backwards and `#over` must win. Distinct on purpose: sortAllChildren is unstable,
    and equal orders permute between frames;
  * `#rotated` - a square turned 45 degrees. Its AABB corners are the whole point: the registry
    rejects on the bounding box for speed but ANSWERS on the drawn geometry, so a point in the
    corner of the box is a miss. The old per-target rosters answered on the box and got this wrong;
  * `#hidden` - invisible, so it is never visited and never registered. Absence from the registry,
    not a flag on a record, is what makes it not answer;
  * `#clip` / `#clipped` - a scissor and a child that overflows it. The part scrolled out of sight
    is not clickable, which is a property of the frame the node was drawn in and cannot be derived
    from the node alone;
  * `#pad` - a small node, to show that the padding belongs to the ASKER: the same record answers
    differently to two questions about the same point;
  * `#tagged` - registered under a different flag, so a query for one kind of target does not find
    another kind sitting on top of it.

The flags are application bits (HitTestFlags::ApplicationMask), not DropTarget/ContextMenu/Tooltip:
those belong to their own components, and this stand is about the registry, not its tenants. */
class HitTestLayout : public TestLayout {
public:
	// Duplicated by hit-test-check.py on purpose: a check that reads its expectations out of the
	// thing it is checking cannot fail.
	static constexpr float RegionWidth = 600.0f;
	static constexpr float RegionHeight = 420.0f;

	// Two registries in one scene, told apart by the mask a query carries
	static constexpr HitTestFlags FlagA = HitTestFlags(1 << 16);
	static constexpr HitTestFlags FlagB = HitTestFlags(1 << 17);

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Node *addBox(Node *parent, StringView name, const Rect &, ZOrder, const Color4F &,
			HitTestFlags);

	// Everything in the committed registry, topmost first - including records that contain no point
	// in particular, which is how absence is checked
	Value encodeRegistry() const;

	Value query(const Vec2 &world, HitTestFlags mask, float padding) const;

	Node *_region = nullptr;
	Node *_under = nullptr;
	Node *_over = nullptr;
	Node *_rotated = nullptr;
	Node *_hidden = nullptr;
	Node *_clip = nullptr;
	Node *_clipped = nullptr;
	Node *_pad = nullptr;
	Node *_tagged = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_HITTESTLAYOUT_H_
