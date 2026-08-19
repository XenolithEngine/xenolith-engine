/**
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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DOVERLAYLAYOUT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DOVERLAYLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

/** What every in-scene overlay surface has to do, with nothing about WHERE its content goes.

An overlay that holds content the user interacts with owes four things, and each of them is a thing
to get wrong exactly once:

 1. a FocusGroup, installed BEFORE any listener on the same node. A listener records the nearest
    group it finds as it registers, so one added first comes up unaffiliated - and an unaffiliated
    listener is exactly the bug the group exists to prevent. The mask and flags are arguments
    because they are the one part that genuinely differs: a menu that only has to swallow taps
    wants EventMaskTouch, while anything holding a text field needs the keyboard too;
 2. a press outside the content takes the surface down. SceneContent2d::pushOverlay stretches the
    layout over the whole parent, so "outside" is a real question this layer can answer and its
    content cannot;
 3. a display-size change takes it down. An overlay is placed against a geometry that no longer
    exists once the window is resized, and moving it somewhere plausible is worse than closing it;
 4. `ready` and `close` are reported to whoever opened it, once each.

What a subclass adds is the placement: `layoutContent()` is called when the surface is up and sized,
and that is the whole of the difference between an expanding menu and an editor pinned to a rect. */
class SP_PUBLIC OverlaySurface : public SceneLayout2d {
public:
	virtual ~OverlaySurface() = default;

	// The mask and flags the surface's own FocusGroup is created with.
	virtual bool init(InputEventMask &&, FocusGroup::Flags);

	virtual void handleContentSizeDirty() override;

	virtual void handlePushTransitionEnded(SceneContent2d *l, bool replace) override;
	virtual void handlePopTransitionBegan(SceneContent2d *l, bool replace) override;

	// Reports true when the content has settled in place, false when the surface starts going away.
	virtual void setReadyCallback(Function<void(bool)> &&);
	virtual void setCloseCallback(Function<void()> &&);

	Node *getContent() const { return _content; }
	FocusGroup *getFocusGroup() const { return _focusGroup; }

	// Take the surface down. Idempotent: an overlay already popped is not popped twice.
	virtual void close();

protected:
	using SceneLayout2d::init;

	// Called once the surface is up and has a size. Where a subclass puts its content.
	virtual void layoutContent();

	virtual Rc<Node> makeContent();

	// A press at `pt`, in this layout's space. The default closes when it landed outside content.
	virtual bool handleTap(Vec2);

	Node *_content = nullptr;
	FocusGroup *_focusGroup = nullptr;
	InputListener *_listener = nullptr;
	Size2 _displaySize;
	Function<void(bool)> _readyCallback;
	Function<void()> _closeCallback;
};

/** The expanding surface: content grows from a collapsed strip into its target size, bound to a
point on the screen rather than to a rectangle. What a drop-down menu is. */
class SP_PUBLIC OverlayLayout : public OverlaySurface {
public:
	static constexpr float Incr = 56.0f;

	enum class Binding {
		Relative,
		OriginLeft,
		OriginRight,
		Anchor,
	};

	virtual ~OverlayLayout() = default;

	virtual bool init(Vec2 globalOrigin, Binding b, Size2 targetSize);

	virtual Rc<Transition> makeExitTransition(SceneContent2d *) const override;

	virtual void setTargetSize(Size2);

protected:
	using OverlaySurface::init;

	virtual void layoutContent() override;

	void emplaceNode(Vec2 o, Binding b);
	virtual Size2 emplaceContent(Node *, Vec2 o, Binding b, Size2 contentSize, Size2 targetSize);

	virtual Rc<Action> makeEasing(Action *);

	virtual Size2 trimSize(Size2) const;

	Vec2 _globalOrigin;
	Size2 _collapsedSize;
	Size2 _fullSize;
	Binding _binding = Binding::Anchor;
};

} // namespace stappler::xenolith::basic2d

#endif // XENOLITH_RENDERER_BASIC2D_XL2DOVERLAYLAYOUT_H_
