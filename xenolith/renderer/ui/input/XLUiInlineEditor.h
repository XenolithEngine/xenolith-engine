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

/** Editing over a rectangle, not inside a node.

The editor is not a child of the edited node: virtualized lists (ScrollController, TableView)
destroy rows while an edit may hold the IME. It is given an anchor and a rect and lives on its own
overlay.

Session endings:

  Enter               commit
  a press outside     commit (this is "focus loss"; the overlay sees the press, the editor never
                      does)
  scrolling           commit - the rect it was placed against no longer means the same row
  window resize       commit, for the same reason
  the anchor exits    commit
  Escape              cancel, and the editor is restored to what it was seeded with
  another edit opens  cancel - see InlineEditSession::init

`onCommit` returns false to refuse; a refused session stays open with the text intact. The commit is
delivered at most once (Enter and a following press may land in the same interaction). */
struct SP_PUBLIC InlineEditConfig {
	// Reads the editor's value on commit. Required by `beginInlineEdit`; the convenience entry
	// points fill it in.
	Function<Value()> collect;

	// false refuses the commit: the session stays open and focused, with the text intact.
	Function<bool(const Value &)> onCommit;

	Function<void()> onCancel;

	// Exactly once, however the session ended, after onCommit or onCancel.
	Function<void()> onClose;

	// Close when the nearest scroller above the anchor moves: the rect then covers a different row.
	bool closeOnScroll = true;

	// A press outside commits; when off, it cancels.
	bool commitOnFocusLoss = true;

	/* Grow the rect and keep it from collapsing. Keeping the rect off a scroll bar is the caller's
	job (basic2d::ScrollView::getIndicatorReservedSize). */
	Padding padding;
	Size2 minSize;

	// Added to the editor node, so a stylesheet can target inline editors.
	String styleClass;
};

/** The overlay an inline edit lives on: the editor, pinned to a rectangle in the anchor's space.

Public for tests; callers drive the session instead. */
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

/** One editing session. Keep the Rc for as long as the edit should stay open.

At most one session is open application-wide. Opening one cancels the open session before reading
the scene, since its callbacks may move rows. To keep the outgoing editor's value, commit it first;
if that commit is refused, the next open cancels it. */
class SP_PUBLIC InlineEditSession : public Ref {
public:
	virtual ~InlineEditSession();

	// The open session, or null.
	static InlineEditSession *getActive();

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

	// End the session. Restoring the editor is up to it; the convenience wrappers do it.
	void cancel();

	// End the session without committing or cancelling.
	void close();

	// True once the session has ended; further endings are no-ops.
	bool isFinished() const { return _finished; }

protected:
	friend class InlineEditLayout;

	// The single exit for every path; runs only once.
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

/** The same, with a single-line text editor. */
struct SP_PUBLIC InlineTextEditConfig {
	// What the field opens with, and what Escape puts back.
	String text;
	String placeholder;

	// Select all, so typing replaces the text.
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

	// The target's rectangle in the anchor's space.
	Rect rect;

	// What the target is holding, for a factory that seeds its own editor.
	StringView text;
};

using InlineEditorFactory = Function<Rc<Node>(const InlineEditRequest &)>;

// What starts the edit. DoubleTap by default, since a single tap selects a list row.
enum class InlineEditTrigger {
	DoubleTap,
	SingleTap,

	// Nothing starts it; the owner calls begin() when it decides.
	Manual,
};

/** Attached to the node that is being edited: computes its own rectangle and opens the session.

An InputListener rather than a System owning one: a System may not add or remove sibling Systems in
handleAdded/handleRemoved (Node::removeSystem holds an iterator). */
class SP_PUBLIC InlineEditTarget : public InputListener {
public:
	virtual ~InlineEditTarget() = default;

	// Takes an argument: a zero-argument override would be hidden by
	// InputListener::init(int32_t priority = 0) and never run.
	virtual bool init(InlineEditTrigger);
	virtual bool init(InlineEditTrigger, StringView text);

	virtual void handleExit() override;

	// Unset, an edit opens a text field over getText().
	virtual void setFactory(InlineEditorFactory &&);

	/* Reads the value from a factory-built editor. Required with setFactory if the commit should
	carry a value; without it the commit carries Nil. */
	virtual void setCollectCallback(Function<Value()> &&);

	// What the stock text editor opens with. A commit does not write it back; the owner does.
	virtual void setText(StringView);
	StringView getText() const { return _text; }

	virtual void setCommitCallback(Function<bool(const Value &)> &&);
	virtual void setCancelCallback(Function<void()> &&);
	virtual void setCloseCallback(Function<void()> &&);

	/* The space the rect is expressed in and the node the overlay is pushed onto. Defaults to the
	scene's content node; inside a list, pass the list. */
	virtual void setAnchor(Node *);
	Node *getAnchor() const { return _anchor; }

	virtual void setTrigger(InlineEditTrigger);
	InlineEditTrigger getTrigger() const { return _trigger; }

	// As in InlineEditConfig.
	virtual void setPadding(Padding);
	virtual void setMinSize(Size2);
	virtual void setEditorStyleClass(StringView);

	virtual void setCloseOnScroll(bool);
	virtual void setCommitOnFocusLoss(bool);

	// Open now. False without an owner, a scene, or with an empty rect.
	virtual bool begin();

	bool isEditing() const;
	InlineEditSession *getSession() const { return _session; }

	// The owner's rectangle in the anchor's space, as begin() uses it.
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
