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


#ifndef XENOLITH_RENDERER_UI_INPUT_XLUIINLINEEDITOR_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUIINLINEEDITOR_H_

#include "XLUiConfig.h"
#include "XLUiTextInput.h"
#include "XL2dOverlayLayout.h"
#include "XL2dScrollViewBase.h"
#include "XLUiScrollSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class InlineEditSession;

/** Editing over a RECTANGLE, not inside a node.

WHY IT IS NOT A CHILD OF THE THING BEING EDITED. A virtualized list destroys the node under the
edit, and it does so by design, twice over: ScrollController drops a row that leaves the window, and
TableView rebuilds every row whose RowKey changed - which `invalidateSource()` forces for all of
them at once. A ui::TextInput holds the IME and the keyboard focus while it is being typed into, so
a node destroyed mid-edit is not a redraw glitch, it is lost input.

So an inline editor is given an ANCHOR and a RECT rather than a parent. It lives on an overlay of
its own, above everything, and the list underneath is free to rebuild as often as it likes.

WHAT ENDS A SESSION, and what each ending does with what was typed:

  Enter            commit
  a press outside  commit (this is "focus loss"; the overlay sees the press, the editor never does)
  scrolling        commit - the rect it was placed against no longer means the same row
  window resize    commit, for the same reason
  the anchor exits commit
  Escape           CANCEL, and the editor is restored to what it was seeded with

Committing is a QUESTION, not a notification: `onCommit` returns false to refuse, and a refused
session stays open with the text intact so the author can fix it. And whatever the ending, the
commit is delivered AT MOST ONCE - Enter and the press that follows it land in the same interaction
often enough that this has to be a guarantee rather than an intention. */
struct SP_PUBLIC InlineEditConfig {
	/* Reads the editor's value when the session commits.

	Required by `beginInlineEdit`, which is handed a node it knows nothing about. The convenience
	entry points below fill it in for the editor they build. */
	Function<Value()> collect;

	// false REFUSES the commit: the session stays open, focused, with what was typed still in it.
	Function<bool(const Value &)> onCommit;

	Function<void()> onCancel;

	// Exactly once, however the session ended, after onCommit or onCancel.
	Function<void()> onClose;

	/* Close when the nearest scroller above the anchor moves.

	On by default because the rect is the whole address of what is being edited: once the list has
	scrolled, that rectangle is over a different row, and an editor left hanging there is editing
	something the author did not point at. */
	bool closeOnScroll = true;

	// A press outside commits. Off makes an outside press CANCEL instead.
	bool commitOnFocusLoss = true;

	// Grows the rect the editor is placed on, and keeps it from collapsing.
	Padding padding;
	Size2 minSize;

	// Put on the editor node, so a stylesheet can tell an inline editor from a placed one.
	String styleClass;
};

/** The overlay an inline edit lives on: the editor, pinned to a rectangle in the anchor's space.

Public because a test has to be able to find it; a caller drives the session, not this. */
class SP_PUBLIC InlineEditLayout : public basic2d::OverlaySurface {
public:
	virtual ~InlineEditLayout() = default;

	virtual bool init(NotNull<Node> anchor, const Rect &, Rc<Node> &&editor, InlineEditSession *);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void update(const UpdateTime &) override;

	const Rect &getRect() const { return _rect; }
	void setRect(const Rect &);

	Node *getAnchor() const { return _anchor; }

protected:
	using basic2d::OverlaySurface::init;

	virtual Rc<Node> makeContent() override;
	virtual void layoutContent() override;
	virtual bool handleTap(Vec2) override;

	// The nearest thing that can scroll what is under the rect, and where it currently is.
	void findScroller();
	bool takeScroller(Node *);
	bool findScrollerBelow(Node *);
	bool readScrollPosition(float &) const;

	Node *_anchor = nullptr;
	Rect _rect;
	Rc<Node> _editor;
	InlineEditSession *_session = nullptr;

	basic2d::ScrollViewBase *_scroll = nullptr;
	ScrollSystem *_scrollSystem = nullptr;
	float _scrollPosition = 0.0f;
	bool _hasScroll = false;
};

/** One editing session. Keep the Rc for as long as the edit should stay open. */
class SP_PUBLIC InlineEditSession : public Ref {
public:
	virtual ~InlineEditSession() = default;

	virtual bool init(NotNull<Node> anchorContent, const Rect &, Rc<Node> &&editor,
			InlineEditConfig &&);

	bool isOpen() const { return _layout != nullptr; }

	Node *getEditor() const;
	Node *getAnchor() const { return _anchor; }
	InlineEditLayout *getLayout() const { return _layout; }

	const Rect &getRect() const;

	// The anchor moved, or the caller re-measured. Does not end the session.
	void setRect(const Rect &);

	// Ask to commit. False when the commit was refused - the session is then still open.
	bool commit();

	// End the session, restoring nothing here: what "restore" means belongs to the editor, and the
	// convenience wrappers below do it for the editor they built.
	void cancel();

	// End it without committing and without cancelling: for a caller that has already decided.
	void close();

	// True once the session has ended, whichever way. A second ending is a no-op, not a second
	// callback.
	bool isFinished() const { return _finished; }

protected:
	friend class InlineEditLayout;

	// The single exit. Every path goes through it, and it answers only once.
	bool finish(bool commitValue);

	InlineEditConfig _config;
	Node *_anchor = nullptr;
	InlineEditLayout *_layout = nullptr;
	Rc<CallbackSystem> _anchorWatch;
	bool _finished = false;
	bool _inCommit = false;
};

/** Opens an editor over `rect`, expressed in `anchorContent`'s coordinate space.

`config.collect` must be set: this entry point is handed a node it cannot interpret. */
SP_PUBLIC Rc<InlineEditSession> beginInlineEdit(NotNull<Node> anchorContent, const Rect &,
		Rc<Node> &&editor, InlineEditConfig &&);

/** The same, over a line of text - which is what three of the four callers actually want. */
struct SP_PUBLIC InlineTextEditConfig {
	// What the field opens with, and what Escape puts back.
	String text;
	String placeholder;

	// Select it all, so that typing replaces rather than appends. What every rename does.
	bool selectAll = true;

	Function<bool(StringView)> onCommit;
	Function<void()> onCancel;
	Function<void()> onClose;

	bool closeOnScroll = true;
	bool commitOnFocusLoss = true;

	Padding padding;
	Size2 minSize;
	String styleClass;
};

SP_PUBLIC Rc<InlineEditSession> beginInlineTextEdit(NotNull<Node> anchorContent, const Rect &,
		InlineTextEditConfig &&);

// ---- starting an edit from the node being edited ------------------------------------------------

class InlineEditTarget;

// Everything a factory could want to know about why it is being asked for an editor.
struct SP_PUBLIC InlineEditRequest {
	Node *target = nullptr;
	InlineEditTarget *source = nullptr;
	Node *anchor = nullptr;

	// The target's rectangle, already in the anchor's space - which is what the session wants.
	Rect rect;

	// What the target is holding, for a factory that seeds its own editor.
	StringView text;
};

using InlineEditorFactory = Function<Rc<Node>(const InlineEditRequest &)>;

/* What starts the edit.

DoubleTap by default, and that is not a style choice: in a list a single tap already means "select
this row", and a widget that took it would make selecting impossible. */
enum class InlineEditTrigger {
	DoubleTap,
	SingleTap,

	// Nothing starts it; the owner calls begin() when it decides.
	Manual,
};

/** Attached to the node that is being edited: computes its own rectangle and opens the session.

An InputListener rather than a plain System, and for a reason the tree states twice: a System may
not add or remove a sibling System from its own handleAdded/handleRemoved, because Node::removeSystem
runs those while holding an iterator into the list. That is why ScrollSystem, FormInputListener,
ScrollSystem and DragSource ARE listeners instead of owning one. This is the same family. */
class SP_PUBLIC InlineEditTarget : public InputListener {
public:
	virtual ~InlineEditTarget() = default;

	/* Takes its argument on purpose: InputListener::init(int32_t priority = 0) would be HIDDEN by a
	zero-argument override, which then silently never runs. Same reason ScrollSystem
	and FormInputListener all take arguments. */
	virtual bool init(InlineEditTrigger);
	virtual bool init(InlineEditTrigger, StringView text);

	virtual void handleExit() override;

	// Unset, an edit opens a text field over getText().
	virtual void setFactory(InlineEditorFactory &&);

	/* How to read the value out of an editor the FACTORY built.

	The stock one-line editor reads itself - beginInlineTextEdit fills `collect` in on the caller's
	behalf - but a factory hands back a node this side knows nothing about, and until this existed
	that meant a factory-built editor committed Nil. Silently: `collect` is optional, so its absence
	looked like "the value is empty" rather than "there is nobody to ask".

	Required whenever setFactory is used and the commit is supposed to carry anything. */
	virtual void setCollectCallback(Function<Value()> &&);

	// What the stock text editor opens with. A commit does NOT write it back - the owner does, in
	// its commit callback, because only it knows whether the value was accepted.
	virtual void setText(StringView);
	StringView getText() const { return _text; }

	virtual void setCommitCallback(Function<bool(const Value &)> &&);
	virtual void setCancelCallback(Function<void()> &&);
	virtual void setCloseCallback(Function<void()> &&);

	/* Whose space the rect is expressed in, and which node the overlay is pushed onto.

	Unset means the scene's content node, which is right whenever the owner is not inside anything
	that scrolls. Inside a list, pass the list: its space is where the rect stays meaningful. */
	virtual void setAnchor(Node *);
	Node *getAnchor() const { return _anchor; }

	virtual void setTrigger(InlineEditTrigger);
	InlineEditTrigger getTrigger() const { return _trigger; }

	// Grows the rect and floors it, the same as InlineEditConfig's.
	virtual void setPadding(Padding);
	virtual void setMinSize(Size2);
	virtual void setEditorStyleClass(StringView);

	virtual void setCloseOnScroll(bool);
	virtual void setCommitOnFocusLoss(bool);

	// Open now. False when there is nothing to open over - no owner, no scene, an empty rect.
	virtual bool begin();

	bool isEditing() const;
	InlineEditSession *getSession() const { return _session; }

	// The owner's rectangle in the anchor's space, which is what begin() would use.
	bool getTargetRect(Rect &out) const;

protected:
	using InputListener::init;

	Node *resolveAnchor() const;
	Rc<Node> makeEditor(const InlineEditRequest &);

	InlineEditTrigger _trigger = InlineEditTrigger::DoubleTap;
	String _text;
	Node *_anchor = nullptr;

	InlineEditorFactory _factory;
	Function<Value()> _collectCallback;
	Function<bool(const Value &)> _commitCallback;
	Function<void()> _cancelCallback;
	Function<void()> _closeCallback;

	Padding _padding;
	Size2 _minSize;
	String _styleClass;
	bool _closeOnScroll = true;
	bool _commitOnFocusLoss = true;

	Rc<InlineEditSession> _session;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_INPUT_XLUIINLINEEDITOR_H_ */
