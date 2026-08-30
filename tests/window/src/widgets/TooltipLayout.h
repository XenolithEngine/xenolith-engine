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

#ifndef TESTS_WINDOW_SRC_WIDGETS_TOOLTIPLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_TOOLTIPLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiTooltipSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* ui::TooltipComponent / ui::TooltipSystem: which node the pointer is resting on, and what it says.

A hint is declared as DATA on a node, and one listener on the scene decides which of them the
pointer is on, from the frame's hit-test registry. Everything that used to be per-node - a listener,
a hover recognizer, a "am I hovered" flag - is gone, so everything that used to fall out of having
one has to be checked deliberately:

  * `#plain` declares its hint in init(), BEFORE the layout is in a scene. Nothing here acquires a
    TooltipSystem, so the coordinator existing at all is the check: a hint declared while a widget
    is being built must still work;
  * `#under` / `#over` overlap with distinct ZOrders. The registry is walked backwards, so the
    upper one must be the one hovered;
  * `#padded` has a hover padding. The padding is the NODE's, so the same point resolves to it and
    to nothing depending on whose question it is;
  * `#disabled` carries a hint that is turned off - it must be invisible to the resolution while
    remaining a perfectly ordinary node;
  * `move-plain` slides a node out from under a pointer that has not moved. That case used to be
    covered by each target's own geometry updates; now it is the per-frame re-resolution, and it is
    the one thing no amount of synthetic pointer movement would catch.

The stand deliberately does NOT call TooltipSystem::acquireForNode: that is what it is checking. */
class TooltipLayout : public TestLayout {
public:
	// Duplicated by tooltip-check.py on purpose: a check that reads its expectations out of the
	// thing it is checking cannot fail.
	static constexpr float RegionWidth = 600.0f;
	static constexpr float RegionHeight = 380.0f;
	static constexpr float PadHover = 16.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Node *addBox(Node *parent, StringView name, const Rect &, ZOrder, const Color4F &);
	Node *findBox(StringView name) const;

	Value encodeState() const;

	Node *_region = nullptr;
	Node *_plain = nullptr;
	Node *_under = nullptr;
	Node *_over = nullptr;
	Node *_padded = nullptr;
	Node *_disabled = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_TOOLTIPLAYOUT_H_
