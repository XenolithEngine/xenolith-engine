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

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// The bubble's horizontal padding, which its width has to hold on top of the text.
static constexpr float s_bubblePadding = 24.0f;

// Vertical padding plus the gaps between the labels, as declared in the stylesheet.
static constexpr float s_bubbleVerticalPadding = 16.0f;
static constexpr float s_bubbleRowGap = 4.0f;

} // namespace

bool ChatBubble::init(ChatRole role) { return init(getChatRoleName(role)); }

bool ChatBubble::init(StringView styleClass) {
	if (!ui::Panel::init()) {
		return false;
	}

	addStyleClass("bubble");
	addStyleClass(styleClass);

	// Z-order is document order here, and document order is what the flex column lays out by. The
	// reasoning label is built later than the body but has to appear above it, so it is given the
	// lower order up front rather than sorted in afterwards.
	/* A Label is not born with a CSS type: `setType` is what makes a `label` tag selector match it,
	and without it every rule in the sheet naming one is inert - the text keeps the engine's default
	colour and size, and nothing says why. Every widget that owns a label does this (ui::TextInput
	does it for both of its own). */
	_bodyLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(2));
	_bodyLabel->setType("label");
	_bodyLabel->addStyleClass("body");

	return true;
}



void ChatBubble::setBody(StringView text) {
	_bodyLabel->setString(text);
	applyWrapWidth();

	// Published here rather than waited for: the frame has to follow the text it holds on the same
	// edit, not on whichever later frame the panel's tick happens to land on.
	refreshHeight();
}

StringView ChatBubble::getBody() const { return _bodyLabel->getString8(); }

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

void ChatBubble::refreshHeight() {
	auto height = s_bubbleVerticalPadding;
	auto count = 0u;

	for (auto label : {_reasoningLabel, _bodyLabel, _metaLabel}) {
		if (!label) {
			continue;
		}

		/* Shape it NOW. A Label re-wraps its text lazily, on the visit that follows the change, so
		a height read straight after setString is the height of the text BEFORE it - and a card
		that only ever reads that trails one edit behind for as long as an answer keeps arriving.
		tryUpdateLabel does nothing when nothing is dirty, so the streaming case pays for the
		re-shape it was going to pay for anyway, one step earlier. */
		label->tryUpdateLabel();

		/* The label's OWN size, and it is only its own because the bubble is not a flex container:
		inside one, the layout assigns each item the box it decided on - computed before the text
		was re-wrapped at that width - and that assignment overwrites what the formatter measured.
		Outside one, what a Label reports is what its text actually came to. */
		height += label->getContentSize().height;
		++count;
	}

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
}

void ChatBubble::handleContentSizeDirty() {
	ui::Panel::handleContentSizeDirty();

	/* Laid out by hand, top down, because the labels are not flex items - see refreshHeight for why
	they must not be. Y is up, so the first row sits at the top of the box. */
	auto y = getContentSize().height - s_bubbleVerticalPadding / 2.0f;

	for (auto label : {_reasoningLabel, _bodyLabel, _metaLabel}) {
		if (!label) {
			continue;
		}
		label->setAnchorPoint(Anchor::TopLeft);
		label->setPosition(Vec2(s_bubblePadding / 2.0f, y));
		y -= label->getContentSize().height + s_bubbleRowGap;
	}
}

void ChatBubble::applyWrapWidth() const {
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
	auto self = const_cast<ChatBubble *>(this);
	ui::setStyleVariable(self, "--wrap-width", toString(uint32_t(_wrapWidth), "px"));
	ui::setStyleVariable(self, "--bubble-width",
			toString(uint32_t(_wrapWidth + s_bubblePadding), "px"));
}

} // namespace stappler::xenolith::examples
