/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h" // IWYU pragma: keep

#include "agentchat/ChatBubble.h"
#include "XLUiStyleSystem.h"
#include "XLUiScrollSystem.h"
#include "XLInputListener.h"
#include "XLSelectionSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// The bubble's horizontal padding, which its width has to hold on top of the text.
static constexpr float s_bubblePadding = 24.0f;

// Vertical padding plus the gaps between the labels, as declared in the stylesheet.
static constexpr float s_bubbleVerticalPadding = 16.0f;
static constexpr float s_bubbleRowGap = 4.0f;

/* How often a streaming Markdown answer is rebuilt, in microseconds of application time.

A commit reparses the whole answer and builds the node tree again - ui::MarkdownView::getTimings
names the two halves - and a model emits tokens several times faster than a reader can use a new
layout. Eight rebuilds a second reads as continuous and leaves the frame budget alone; the text
itself is never delayed by more than that, and the end of a turn commits regardless. */
static constexpr uint64_t s_markdownInterval = 125'000;

// A document that has not been laid out yet reports nothing, and a card of no height is a card
// that flickers into existence a frame later. One line is the honest placeholder.
static constexpr float s_minBodyViewHeight = 18.0f;

} // namespace

bool ChatBubble::init(ChatRole role) {
	// Only the model writes Markdown. A question is what the user typed and a tool result is JSON
	// someone has to be able to read literally - both stay flat text.
	return init(getChatRoleName(role), role == ChatRole::Assistant);
}

bool ChatBubble::init(StringView styleClass) { return init(styleClass, false); }

bool ChatBubble::init(StringView styleClass, bool markdown) {
	if (!ui::Panel::init()) {
		return false;
	}

	addStyleClass("bubble");
	addStyleClass(styleClass);

	// A card is something the user can point at: a tap selects it, and the arrow keys then walk
	// the conversation card by card. Selecting text in an answer selects inside the card, so the
	// same `:selection-within` rule marks it either way
	setNodeSelectable(this, true);
	auto listener = addSystem(Rc<InputListener>::create());
	listener->addTapRecognizer([this](const GestureTap &) {
		if (auto system = SelectionSystem::acquireForNode(this)) {
			system->selectNode(this);
		}
		return true;
	});

	// Z-order is document order here, and document order is what the flex column lays out by. The
	// reasoning label is built later than the body but has to appear above it, so it is given the
	// lower order up front rather than sorted in afterwards.
	/* A Label is not born with a CSS type: `setType` is what makes a `label` tag selector match it,
	and without it every rule in the sheet naming one is inert - the text keeps the engine's default
	colour and size, and nothing says why. Every widget that owns a label does this (ui::TextInput
	does it for both of its own). */
	if (markdown) {
		/* The view brings its own stylesheet and its own recursive resolver, so an answer is
		readable before this example says anything about it. What the sheet here then does is
		outrank it: every rule it writes is keyed on this class, and a class beats the bare tag
		selectors the built-in sheet is deliberately written with. */
		_bodyView = addChild(Rc<ui::MarkdownView>::create(), ZOrder(2));
		_bodyView->addStyleClass("answer");

		// Nothing may leave the card by accident: this is somebody's conversation, and the widget
		// offers a context menu of its own.
		_bodyView->setCopyPolicy(ui::MarkdownView::CopyPolicy::Both);
	} else {
		_bodyLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(2));
		_bodyLabel->setType("label");
		_bodyLabel->addStyleClass("body");
	}

	return true;
}

void ChatBubble::setBody(StringView text) {
	if (_body == text) {
		return;
	}

	_body = text.str<Interface>();

	if (_bodyView) {
		/* Recorded, not shown. tickBody decides when a rebuild is worth its cost, and the end of
		the turn flushes whatever is still waiting - so a delta is never lost, only deferred. */
		_bodyDirty = true;
		return;
	}

	_bodyLabel->setString(_body);
	applyWrapWidth();

	// Published here rather than waited for: the frame has to follow the text it holds on the same
	// edit, not on whichever later frame the panel's tick happens to land on.
	refreshHeight();
}

void ChatBubble::tickBody(uint64_t now) {
	if (!_bodyDirty) {
		return;
	}

	// _bodyTime is zero until the first commit, so the first delta of an answer is shown on the
	// frame it arrives and only the ones after it wait.
	if (now - _bodyTime < s_markdownInterval) {
		return;
	}

	_bodyTime = now;
	commitBody();
}

void ChatBubble::flushBody() {
	_bodyTime = 0;
	commitBody();
}

void ChatBubble::commitBody() {
	if (!_bodyDirty || !_bodyView) {
		return;
	}

	_bodyDirty = false;

	_bodyView->setSource(_body);
	applyWrapWidth();
	refreshHeight();
}

StringView ChatBubble::getBody() const { return _body; }

void ChatBubble::setReasoning(StringView text) { setTopText(text, "reasoning"); }

void ChatBubble::setTitle(StringView text) { setTopText(text, "title"); }

void ChatBubble::setTopText(StringView text, StringView styleClass) {
	if (text.empty() && !_reasoningLabel) {
		return;
	}

	if (!_reasoningLabel) {
		_reasoningLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		_reasoningLabel->setType("label");
		_reasoningLabel->addStyleClass(styleClass);
	}

	_reasoningLabel->setString(text);
	applyWrapWidth();
	refreshHeight();
}

void ChatBubble::setMeta(StringView text) {
	if (text.empty() && !_metaLabel) {
		return;
	}

	if (!_metaLabel) {
		_metaLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(3));
		_metaLabel->setType("label");
		_metaLabel->addStyleClass("meta");
	}

	_metaLabel->setString(text);
	applyWrapWidth();
	refreshHeight();
}

void ChatBubble::setError(bool value) {
	if (_error == value) {
		return;
	}

	_error = value;
	if (value) {
		addStyleClass("error");
	} else {
		removeStyleClass("error");
	}
}

void ChatBubble::setWrapWidth(float width) {
	if (_wrapWidth == width) {
		return;
	}

	_wrapWidth = width;
	applyWrapWidth();
	refreshHeight();
}

void ChatBubble::eachRow(const Callback<void(Node *)> &cb) const {
	// Top to bottom, which is also the order the sheet's z-orders put them in: what the model
	// thought, what it answered, and the one line under it.
	if (_reasoningLabel) {
		cb(_reasoningLabel);
	}
	if (_bodyLabel) {
		cb(_bodyLabel);
	}
	if (_bodyView) {
		cb(_bodyView);
	}
	if (_metaLabel) {
		cb(_metaLabel);
	}
}

float ChatBubble::measureLabel(basic2d::Label *label) const {
	/* Shape it NOW. A Label re-wraps its text lazily, on the visit that follows the change, so a
	height read straight after setString is the height of the text BEFORE it - and a card that only
	ever reads that trails one edit behind for as long as an answer keeps arriving. tryUpdateLabel
	does nothing when nothing is dirty, so the streaming case pays for the re-shape it was going to
	pay for anyway, one step earlier. */
	label->tryUpdateLabel();

	/* The label's OWN size, and it is only its own because the bubble is not a flex container:
	inside one, the layout assigns each item the box it decided on - computed before the text was
	re-wrapped at that width - and that assignment overwrites what the formatter measured. Outside
	one, what a Label reports is what its text actually came to. */
	return label->getContentSize().height;
}

float ChatBubble::measureBodyView() {
	/* A document cannot be shaped on demand the way a label can. ui::MarkdownView measures nothing
	about itself on purpose - the width is the question its owner answers and the wrapping is the
	answer - so the height of the answer is whatever the flex column INSIDE it gave the body node,
	which is a result of the layout pass rather than something readable straight after setSource.

	That is one pass of lag on every commit, and it is why the panel ticks every card every frame
	instead of trusting the setter: the number arrives after the edit that produced it. */
	auto content = _bodyView->getContentNode();
	auto height = content ? content->getContentSize().height : 0.0f;

	if (height < s_minBodyViewHeight) {
		height = s_minBodyViewHeight;
	}

	if (_wrapWidth > 0.0f) {
		_bodyView->setContentSize(Size2(_wrapWidth, height));
	}

	return height;
}

void ChatBubble::refreshHeight() {
	auto height = s_bubbleVerticalPadding;
	auto count = 0u;

	eachRow([&](Node *node) {
		if (node == _bodyView) {
			height += measureBodyView();
		} else {
			height += measureLabel(static_cast<basic2d::Label *>(node));
		}
		++count;
	});

	if (count > 1) {
		height += float(count - 1) * s_bubbleRowGap;
	}

	auto rounded = sprt::round(height);
	if (_height == rounded) {
		return;
	}

	_height = rounded;
	ui::setStyleVariable(this, "--bubble-height", toString(uint32_t(rounded), "px"));
	markContentSizeDirty();

	/* THE LOG HAS TO BE TOLD, and telling the row is not telling the log.

	The card's height reaches the row it sits in as an intrinsic hint, and the row's own height is
	not the row's to write: the log is a flex column, so the log owns it. The engine carries a
	content-size change outwards only as long as each container's OWN size changes on the way, and
	that chain ends at the row - which cannot change its size without the log, and the log is never
	asked. The scroll range is computed from the rows, so a log full of rows that never grew has
	nothing to scroll, and an answer taller than the window simply runs off the top of it.

	Walking out to the scroller re-measures every container the card's height can move, and this
	runs only on the frames where the height actually changed - the early return above is what
	makes it affordable while an answer streams. */
	for (auto node = getParent(); node; node = node->getParent()) {
		node->markLayoutChildrenDirty();
		node->markMeasureDirty();
		if (node->getSystemByType<ui::ScrollSystem>()) {
			break;
		}
	}
}

void ChatBubble::handleContentSizeDirty() {
	ui::Panel::handleContentSizeDirty();

	/* Laid out by hand, top down, because the rows are not flex items - see refreshHeight for why
	they must not be. Y is up, so the first row sits at the top of the box. */
	auto y = getContentSize().height - s_bubbleVerticalPadding / 2.0f;

	eachRow([&](Node *node) {
		node->setAnchorPoint(Anchor::TopLeft);
		node->setPosition(Vec2(s_bubblePadding / 2.0f, y));
		y -= node->getContentSize().height + s_bubbleRowGap;
	});
}

void ChatBubble::applyWrapWidth() {
	if (_wrapWidth <= 0.0f) {
		return;
	}

	/* Declared on the node rather than called on the label, and that is the whole trick.

	A width handed to Label::setWidth from code is overwritten by the next layout pass, which calls
	applyMeasuredSize with the box the flex column gave it. And in a COLUMN the label's height is
	the main axis - measured first, before any width is known - so a label that only learns its
	width from that pass reports the height of a single line and the text is never wrapped.

	A custom property is the supported channel for a value that differs per node. It behaves as a
	declaration written for this node, the sheet reads it back with var(), and the width is
	therefore definite BEFORE anything is measured. */
	ui::setStyleVariable(this, "--wrap-width", toString(uint32_t(_wrapWidth), "px"));
	ui::setStyleVariable(this, "--bubble-width",
			toString(uint32_t(_wrapWidth + s_bubblePadding), "px"));

	/* The document takes its width the direct way instead, and not for want of a rule: a
	MarkdownView with a CSS `width` would be sized by the resolver on a node whose interior the
	view itself lays out, and the height half of the same box has no CSS spelling at all - nothing
	in the sheet can say "as tall as the document came out". One writer for both axes, here. */
	if (_bodyView) {
		_bodyView->setContentSize(Size2(_wrapWidth, _bodyView->getContentSize().height));
	}
}

} // namespace stappler::xenolith::examples
