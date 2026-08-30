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

#ifndef TESTS_WINDOW_SRC_WIDGETS_CONTEXTMENULAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_CONTEXTMENULAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiContextMenu.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* ui::ContextMenuTarget / ui::ContextMenuSystem: who gets asked for a menu, and who stops it.

None of this is visible. A menu that opened for the wrong target looks exactly like the right one
until you read its items, and every rule under test is about a menu that must NOT open - which
looks like nothing at all. So the stand is a set of nested regions with different answers, and the
check reads the items out of the popup's own scene.

The regions, from the bottom up:

  * `#region` - a target whose builder answers with DIFFERENT items for the left and right halves
    of itself. That is the claim that the point reaches the builder at all, and it is in the
    target's own space, not the window's;
  * `#under` / `#over` - two overlapping targets with distinct ZOrders. Registration order is paint
    order, so the one on top must be the one asked. Their orders are distinct on purpose:
    sortAllChildren is unstable and equal orders permute between frames;
  * `#hidden` - a target on an invisible node, over the region. It is never visited, so it is not
    in the roster, and the region under it must answer instead;
  * `#blocked` - a target that offers NOTHING. It must stop the region's menu rather than let it
    through: a widget with no menu of its own is not a hole in its container;
  * `#swallow` - no target at all, but a listener that swallows the right button. That is the other
    way to say "this gesture is mine", and it has to work for a widget the context menu has never
    heard of.

The counters are what an activation is checked by: `activations` counts item callbacks, `opens`
counts openings, `builderCalls` counts how often a builder was asked - which is how "the topmost
target refused and nothing below was tried" is told from "nobody was asked". */
class ContextMenuLayout : public TestLayout {
public:
	// Duplicated by context-menu-check.py on purpose: a check that reads its expectations out of
	// the thing it is checking cannot fail.
	static constexpr float RegionWidth = 600.0f;
	static constexpr float RegionHeight = 420.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleEnter(Scene *) override;

protected:
	virtual void registerCommands() override;

	// A menu of `count` buttons named "<prefix>-1"..., each bumping the counters
	Rc<ui::MenuSource> makeMenu(StringView prefix, size_t count);

	Node *addRegion(StringView name, const Rect &, ZOrder, const Color4F &);

	Value encodeState() const;
	static Value encodeRect(const Node *);

	ui::ContextMenuSystem *_menus = nullptr;

	Node *_region = nullptr;
	Node *_under = nullptr;
	Node *_over = nullptr;
	Node *_hidden = nullptr;
	Node *_blocked = nullptr;
	Node *_swallow = nullptr;

	// What the last builder call was asked, in the TARGET's own space - the whole point of putting
	// the location there rather than in the window's
	Vec2 _lastRequest = Vec2::INVALID;
	bool _lastFromTouch = false;
	String _lastTarget;
	String _lastItem;

	uint32_t _builderCalls = 0;
	uint32_t _opens = 0;
	uint32_t _activations = 0;

	// Right taps that reached the swallowing widget: "the bar was not in the way" told from "the
	// press never arrived", the same distinction the scrollbar stand needs
	uint32_t _swallowTaps = 0;

	// Ordinary left clicks that reached the region. What a dismissing click must NOT produce.
	uint32_t _regionTaps = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_CONTEXTMENULAYOUT_H_
