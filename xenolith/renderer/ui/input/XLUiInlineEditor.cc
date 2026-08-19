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


#include "XLUiInlineEditor.h"
#include "XL2dSceneContent.h"
#include "XLScene.h"
#include "XLHotkey.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

/* The stock editor: a text field that is its own CSS type.

A class rather than a configured ui::TextInput because registerStyleAppliers is protected - and
that is the right shape anyway: every field built on TextInput declares its own type from init(),
which is what lets a stylesheet address `inline-editor` without touching every text input. */
class InlineTextEditor : public TextInput {
public:
	virtual ~InlineTextEditor() = default;

	virtual bool init() override {
		if (!TextInput::init()) {
			return false;
		}

		setType("inline-editor");
		removeStyleClass("xl-ui-text-input");
		addStyleClass("xl-ui-inline-editor");
		registerStyleAppliers("inline-editor");
		return true;
	}
};

} // namespace

// ---- InlineEditLayout ---------------------------------------------------------------------------

bool InlineEditLayout::init(NotNull<Node> anchor, const Rect &rect, Rc<Node> &&editor,
		InlineEditSession *session) {
	/* Keyboard as well as touch, and Exclusive so the editor owns it outright while it is up.

	Propagate is not optional: an exclusive group makes the dispatcher re-collect receivers scoped
	to itself, and without it a ui::FormSystem anywhere under this overlay stops receiving input at
	all - which its own header says in as many words. */
	if (!OverlaySurface::init(InputEventMask(EventMaskTouch | EventMaskKeyboard),
				FocusGroup::Flags::Exclusive | FocusGroup::Flags::Propagate)) {
		return false;
	}

	_anchor = anchor;
	_rect = rect;
	_editor = sp::move(editor);
	_session = session;

	setName("inline-edit-layout");
	setType("inline-edit-layout");

	/* Escape arrives as a HOTKEY, never as a key event.

	The runtime's text-input processor takes Escape for itself - it is deliberately not marked
	ReserveFromTextInput ("Escape releases input, Backspace and Delete edit") - so a focused
	ui::TextInput only ever learns of it as an echo with enabled=false. Bound as a raw keycode this
	would simply never fire, and the editor would be the one overlay a person cannot back out of. */
	_listener->addHotkey(EngineHotkeys::get().back, [this](HotkeyId, const InputEvent &) {
		if (_session) {
			_session->cancel();
			return true;
		}
		return false;
	}, HotkeyFlags::None);

	// A key event carries the last pointer position, so the default filter would hand the keyboard
	// over only while the mouse happens to hover the editor. An overlay that owns the keyboard owns
	// it wherever the pointer is - the same seam as ui::TextInput's and ui::MenuSystem's.
	_listener->setTouchFilter(
			[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return true;
		}
		return cb(event);
	});

	return true;
}

void InlineEditLayout::handleEnter(Scene *scene) {
	OverlaySurface::handleEnter(scene);

	if (_session && _session->_config.closeOnScroll) {
		findScroller();
		if (_hasScroll) {
			// Sampled, not subscribed: ScrollViewBase has exactly ONE scroll callback slot, and
			// taking it would quietly disconnect whoever owns the list.
			scheduleUpdate();
		}
	}
}

void InlineEditLayout::handleExit() {
	unscheduleUpdate();
	OverlaySurface::handleExit();
}

void InlineEditLayout::update(const UpdateTime &time) {
	OverlaySurface::update(time);

	if (!_hasScroll || !_session || _session->isFinished()) {
		return;
	}

	float position = 0.0f;
	if (readScrollPosition(position) && position != _scrollPosition) {
		// The rect was the whole address of what is being edited. It now points at a different row,
		// so the session ends - keeping what was typed, because losing it is the one outcome a
		// person cannot undo.
		_session->commit();
	}
}

bool InlineEditLayout::takeScroller(Node *node) {
	if (auto scroll = dynamic_cast<basic2d::ScrollViewBase *>(node)) {
		_scroll = scroll;
		return true;
	}
	if (auto system = node->getSystemByType<ScrollSystem>()) {
		_scrollSystem = system;
		return true;
	}
	return false;
}

bool InlineEditLayout::findScrollerBelow(Node *node) {
	if (takeScroller(node)) {
		return true;
	}
	for (auto &it : node->getChildren()) {
		if (findScrollerBelow(it)) {
			return true;
		}
	}
	return false;
}

void InlineEditLayout::findScroller() {
	_scroll = nullptr;
	_scrollSystem = nullptr;
	_hasScroll = false;

	/* Upward first, then downward, because both topologies are real and they are not the same one.

	An anchor that IS a row sits inside the scroller: the walk up finds it. An anchor that is the
	LIST - which is what a cell editor passes, because the table's space is where the rect keeps its
	meaning - CONTAINS the scroller instead: ui::TableView holds its basic2d::ScrollView as a child.
	Searching only upward finds nothing there, and an editor that never notices the list scrolling
	is left pointing at a row the author did not choose. */
	auto node = _anchor;
	while (node) {
		if (takeScroller(node)) {
			break;
		}
		node = node->getParent();
	}

	if (!_scroll && !_scrollSystem && _anchor) {
		findScrollerBelow(_anchor);
	}

	_hasScroll = readScrollPosition(_scrollPosition);
}

bool InlineEditLayout::readScrollPosition(float &out) const {
	if (_scroll) {
		out = _scroll->getScrollPosition();
		return true;
	}
	if (_scrollSystem) {
		// One number, because that is all the comparison needs: any movement on either axis ends
		// the session, and which axis moved changes nothing about that.
		auto position = _scrollSystem->getScrollPosition();
		out = position.x + position.y;
		return true;
	}
	return false;
}

Rc<Node> InlineEditLayout::makeContent() { return _editor; }

void InlineEditLayout::setRect(const Rect &rect) {
	_rect = rect;
	layoutContent();
}

void InlineEditLayout::layoutContent() {
	if (!_content) {
		auto content = makeContent();
		if (!content) {
			return;
		}
		_content = addChild(sp::move(content), ZOrder(1));
	}

	if (!_anchor || !_anchor->isRunning()) {
		return;
	}

	/* Four corners, not origin plus size: the anchor may be rotated or scaled, and what the editor
	has to cover is the axis-aligned box the rect actually occupies on screen. The same arithmetic
	as ui::placementForNode and TooltipSystem::getTargetWorldRect - but stopping in this layout's
	space, because undoing the density and flipping into Y-down are only what a real window needs. */
	const Vec2 corners[4] = {
		convertToNodeSpace(_anchor->convertToWorldSpace(Vec2(_rect.getMinX(), _rect.getMinY()))),
		convertToNodeSpace(_anchor->convertToWorldSpace(Vec2(_rect.getMaxX(), _rect.getMinY()))),
		convertToNodeSpace(_anchor->convertToWorldSpace(Vec2(_rect.getMinX(), _rect.getMaxY()))),
		convertToNodeSpace(_anchor->convertToWorldSpace(Vec2(_rect.getMaxX(), _rect.getMaxY()))),
	};

	Vec2 low = corners[0];
	Vec2 high = corners[0];
	for (uint32_t i = 1; i < 4; ++i) {
		low.x = sprt::min(low.x, corners[i].x);
		low.y = sprt::min(low.y, corners[i].y);
		high.x = sprt::max(high.x, corners[i].x);
		high.y = sprt::max(high.y, corners[i].y);
	}

	_content->setAnchorPoint(Anchor::BottomLeft);
	_content->setPosition(Vec2(low.x, low.y));
	_content->setContentSize(Size2(high.x - low.x, high.y - low.y));
}

bool InlineEditLayout::handleTap(Vec2 pt) {
	if (_content && !_content->isTouched(pt) && _session) {
		// This IS the focus loss: the overlay covers the whole parent, so the press that would have
		// blurred the field lands here and never reaches it.
		_session->finish(_session->_config.commitOnFocusLoss);
	}
	return true;
}

// ---- InlineEditSession --------------------------------------------------------------------------

bool InlineEditSession::init(NotNull<Node> anchorContent, const Rect &rect, Rc<Node> &&editor,
		InlineEditConfig &&config) {
	if (!editor) {
		return false;
	}

	auto scene = anchorContent->getScene();
	auto content = scene ? dynamic_cast<basic2d::SceneContent2d *>(scene->getContent()) : nullptr;
	if (!content) {
		log::source().warn("ui::InlineEditSession",
				"the anchor is not in a scene with a SceneContent2d: no overlay to open on");
		return false;
	}

	_config = sp::move(config);
	_anchor = anchorContent;

	if (!_config.styleClass.empty()) {
		editor->addStyleClass(_config.styleClass);
	}

	auto rectWithPadding = rect;
	rectWithPadding.origin.x -= _config.padding.left;
	rectWithPadding.origin.y -= _config.padding.bottom;
	rectWithPadding.size.width += _config.padding.horizontal();
	rectWithPadding.size.height += _config.padding.vertical();
	rectWithPadding.size.width = sprt::max(rectWithPadding.size.width, _config.minSize.width);
	rectWithPadding.size.height = sprt::max(rectWithPadding.size.height, _config.minSize.height);

	auto layout =
			Rc<InlineEditLayout>::create(anchorContent, rectWithPadding, sp::move(editor), this);
	if (!layout) {
		return false;
	}

	_layout = layout.get();
	if (!content->pushOverlay(layout)) {
		_layout = nullptr;
		return false;
	}

	/* Its OWN CallbackSystem, not Node::setExitCallback.

	That accessor lazily creates the node's shared callback system, tagged DefaultCallbackSystemTag,
	and writing into it would silently replace whatever the node's owner had put there. */
	_anchorWatch = Rc<CallbackSystem>::create();
	_anchorWatch->setExitCallback([this](CallbackSystem *) {
		// The thing being edited is leaving the scene. Keep what was typed rather than dropping it.
		finish(true);
	});
	anchorContent->addSystemItem(_anchorWatch);

	return true;
}

Node *InlineEditSession::getEditor() const { return _layout ? _layout->getContent() : nullptr; }

const Rect &InlineEditSession::getRect() const { return _layout ? _layout->getRect() : Rect::ZERO; }

void InlineEditSession::setRect(const Rect &rect) {
	if (_layout) {
		_layout->setRect(rect);
	}
}

bool InlineEditSession::commit() { return finish(true); }

void InlineEditSession::cancel() { finish(false); }

void InlineEditSession::close() {
	if (_finished) {
		return;
	}
	_finished = true;

	auto onClose = sp::move(_config.onClose);
	_config.onClose = nullptr;

	if (_layout) {
		auto layout = _layout;
		_layout = nullptr;
		layout->close();
	}

	if (_anchor && _anchorWatch) {
		_anchor->removeSystem(_anchorWatch);
		_anchorWatch = nullptr;
	}

	if (onClose) {
		onClose();
	}
}

bool InlineEditSession::finish(bool commitValue) {
	/* The single exit, and the reason it exists.

	Enter fires the field's accept callback while the priority-1 outside-tap listener inside
	ui::TextInput independently calls blur() - both land in one interaction often enough that
	"commit once" has to be a guarantee. `_inCommit` covers the other direction: a commit callback
	that closes something, or opens something else, must not re-enter this. */
	if (_finished || _inCommit) {
		return false;
	}

	if (commitValue && _config.onCommit) {
		Value value;
		if (_config.collect) {
			value = _config.collect();
		}

		_inCommit = true;
		const bool accepted = _config.onCommit(value);
		_inCommit = false;

		if (!accepted) {
			// Refused. The session stays exactly as it was, with what was typed still in it: the
			// author has to be able to see what was rejected in order to fix it.
			return false;
		}
	} else if (!commitValue && _config.onCancel) {
		_config.onCancel();
	}

	close();
	return true;
}

Rc<InlineEditSession> beginInlineEdit(NotNull<Node> anchorContent, const Rect &rect,
		Rc<Node> &&editor, InlineEditConfig &&config) {
	return Rc<InlineEditSession>::create(anchorContent, rect, sp::move(editor), sp::move(config));
}

Rc<InlineEditSession> beginInlineTextEdit(NotNull<Node> anchorContent, const Rect &rect,
		InlineTextEditConfig &&textConfig) {
	auto input = Rc<InlineTextEditor>::create();
	if (!input) {
		return nullptr;
	}

	input->setName("inline-editor");
	input->setText(textConfig.text);
	if (!textConfig.placeholder.empty()) {
		input->setPlaceholder(textConfig.placeholder);
	}

	InlineEditConfig config;
	config.closeOnScroll = textConfig.closeOnScroll;
	config.commitOnFocusLoss = textConfig.commitOnFocusLoss;
	config.padding = textConfig.padding;
	config.minSize = textConfig.minSize;
	config.styleClass = sp::move(textConfig.styleClass);

	config.collect = [input = input.get()] { return Value(input->getText()); };

	if (auto cb = sp::move(textConfig.onCommit)) {
		config.onCommit = [cb = sp::move(cb)](const Value &value) { return cb(value.getString()); };
	} else {
		config.onCommit = [](const Value &) { return true; };
	}

	/* Escape puts the seeded text back before the caller is told.

	The field will not do it: of every widget here only ui::NumberField restores anything on its
	own, and even that is about a value that does not parse. Restoring here means a caller that
	reads the field in onCancel sees what the edit started from. */
	config.onCancel = [input = input.get(), text = textConfig.text,
							  cb = sp::move(textConfig.onCancel)] {
		input->setText(text);
		if (cb) {
			cb();
		}
	};
	config.onClose = sp::move(textConfig.onClose);

	auto session = beginInlineEdit(anchorContent, rect, input.get(), sp::move(config));
	if (!session) {
		return nullptr;
	}

	// After the overlay is up: focus is a request through the IME, and a field that is not yet in a
	// scene has nothing to request it from.
	input->setEnterCallback([session = session.get()] { session->commit(); });
	input->focus();
	if (textConfig.selectAll) {
		input->selectAll();
	}

	return session;
}

// ---- InlineEditTarget ---------------------------------------------------------------------------

bool InlineEditTarget::init(InlineEditTrigger trigger) { return init(trigger, StringView()); }

bool InlineEditTarget::init(InlineEditTrigger trigger, StringView text) {
	if (!InputListener::init()) {
		return false;
	}

	_trigger = trigger;
	_text = text.str<Interface>();

	// One recognizer for both gestures: the count is what tells them apart, and re-registering on
	// every setTrigger would leave a listener with two.
	addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event != GestureEvent::Activated) {
			return true;
		}
		switch (_trigger) {
		case InlineEditTrigger::DoubleTap:
			if (tap.count > 1) {
				begin();
			}
			break;
		case InlineEditTrigger::SingleTap:
			if (tap.count == 1) {
				begin();
			}
			break;
		case InlineEditTrigger::Manual: break;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 2});

	return true;
}

void InlineEditTarget::handleExit() {
	// The owner is leaving the scene; an overlay left standing over it is one the user has to
	// dismiss by hand.
	if (_session) {
		auto session = sp::move(_session);
		_session = nullptr;
		session->commit();
	}
	InputListener::handleExit();
}

void InlineEditTarget::setFactory(InlineEditorFactory &&cb) { _factory = sp::move(cb); }

void InlineEditTarget::setText(StringView value) { _text = value.str<Interface>(); }

void InlineEditTarget::setCommitCallback(Function<bool(const Value &)> &&cb) {
	_commitCallback = sp::move(cb);
}

void InlineEditTarget::setCancelCallback(Function<void()> &&cb) { _cancelCallback = sp::move(cb); }

void InlineEditTarget::setCloseCallback(Function<void()> &&cb) { _closeCallback = sp::move(cb); }

void InlineEditTarget::setAnchor(Node *node) { _anchor = node; }

void InlineEditTarget::setTrigger(InlineEditTrigger value) { _trigger = value; }

void InlineEditTarget::setPadding(Padding value) { _padding = value; }

void InlineEditTarget::setMinSize(Size2 value) { _minSize = value; }

void InlineEditTarget::setEditorStyleClass(StringView value) {
	_styleClass = value.str<Interface>();
}

void InlineEditTarget::setCloseOnScroll(bool value) { _closeOnScroll = value; }

void InlineEditTarget::setCommitOnFocusLoss(bool value) { _commitOnFocusLoss = value; }

bool InlineEditTarget::isEditing() const { return _session && _session->isOpen(); }

Node *InlineEditTarget::resolveAnchor() const {
	if (_anchor) {
		return _anchor;
	}
	auto owner = getOwner();
	auto scene = owner ? owner->getScene() : nullptr;
	return scene ? scene->getContent() : nullptr;
}

bool InlineEditTarget::getTargetRect(Rect &out) const {
	auto owner = getOwner();
	auto anchor = resolveAnchor();
	if (!owner || !anchor || !owner->isRunning()) {
		return false;
	}

	// Four corners again, for the same reason as in the layout: the owner may be rotated or scaled.
	const auto size = owner->getContentSize();
	const Vec2 corners[4] = {
		anchor->convertToNodeSpace(owner->convertToWorldSpace(Vec2::ZERO)),
		anchor->convertToNodeSpace(owner->convertToWorldSpace(Vec2(size.width, 0.0f))),
		anchor->convertToNodeSpace(owner->convertToWorldSpace(Vec2(0.0f, size.height))),
		anchor->convertToNodeSpace(owner->convertToWorldSpace(Vec2(size.width, size.height))),
	};

	Vec2 low = corners[0];
	Vec2 high = corners[0];
	for (uint32_t i = 1; i < 4; ++i) {
		low.x = sprt::min(low.x, corners[i].x);
		low.y = sprt::min(low.y, corners[i].y);
		high.x = sprt::max(high.x, corners[i].x);
		high.y = sprt::max(high.y, corners[i].y);
	}

	out = Rect(low.x, low.y, high.x - low.x, high.y - low.y);
	return out.size.width > 0.0f && out.size.height > 0.0f;
}

Rc<Node> InlineEditTarget::makeEditor(const InlineEditRequest &request) {
	if (_factory) {
		return _factory(request);
	}
	return nullptr;
}

bool InlineEditTarget::begin() {
	if (isEditing()) {
		return false;
	}

	Rect rect;
	if (!getTargetRect(rect)) {
		return false;
	}

	auto anchor = resolveAnchor();
	if (!anchor) {
		return false;
	}

	InlineEditRequest request;
	request.target = getOwner();
	request.source = this;
	request.anchor = anchor;
	request.rect = rect;
	request.text = _text;

	if (auto editor = makeEditor(request)) {
		InlineEditConfig config;
		config.closeOnScroll = _closeOnScroll;
		config.commitOnFocusLoss = _commitOnFocusLoss;
		config.padding = _padding;
		config.minSize = _minSize;
		config.styleClass = _styleClass;
		config.onCommit = [this](const Value &value) {
			return _commitCallback ? _commitCallback(value) : true;
		};
		config.onCancel = [this] {
			if (_cancelCallback) {
				_cancelCallback();
			}
		};
		config.onClose = [this] {
			_session = nullptr;
			if (_closeCallback) {
				_closeCallback();
			}
		};

		_session = beginInlineEdit(anchor, rect, sp::move(editor), sp::move(config));
		return _session != nullptr;
	}

	// No factory: the stock editor is a line of text over what the target is holding.
	InlineTextEditConfig config;
	config.text = _text;
	config.closeOnScroll = _closeOnScroll;
	config.commitOnFocusLoss = _commitOnFocusLoss;
	config.padding = _padding;
	config.minSize = _minSize;
	config.styleClass = _styleClass;
	config.onCommit = [this](StringView text) {
		return _commitCallback ? _commitCallback(Value(text)) : true;
	};
	config.onCancel = [this] {
		if (_cancelCallback) {
			_cancelCallback();
		}
	};
	config.onClose = [this] {
		_session = nullptr;
		if (_closeCallback) {
			_closeCallback();
		}
	};

	_session = beginInlineTextEdit(anchor, rect, sp::move(config));
	return _session != nullptr;
}

} // namespace stappler::xenolith::ui
