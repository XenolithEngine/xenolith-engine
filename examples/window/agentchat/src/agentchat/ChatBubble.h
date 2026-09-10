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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATBUBBLE_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATBUBBLE_H_

#include "agentchat/ChatHistory.h"
#include "XLUiPanel.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** One message on screen: a panel with up to three labels in a column.

WHY THE WIDTH COMES FROM THE LAYOUT AND ARRIVES AS A CUSTOM PROPERTY. Three engine facts decide it:

  * a Label wraps at the width it was GIVEN, and the width the style resolver gives it comes from an
    explicit CSS `width` that is neither `auto` nor `fit-content` (XLUiStyleResolver.cc:1502);
  * a width written with Label::setWidth from code does not survive: the next layout pass calls
    applyMeasuredSize with the box the flex column decided on and overwrites it. And in a COLUMN the
    label's height is the MAIN axis, measured before any width is known - so a label that learns its
    width only from that pass reports the height of one line and the paragraph is never wrapped;
  * a Panel is a sprite, not a measurable box, so a bubble whose width is `auto` collapses to zero
    however much text it holds. It needs a definite one.

A per-node custom property is the supported channel for a value that differs per node
(ui::setStyleVariable). It reads as a declaration written for this node, the sheet picks it up with
var(), and both widths are therefore definite before anything is measured. The bubble is one share
of the log rather than the width of its own text, because measuring the text would need the
formatted layout, and that only exists after the pass that needed the answer. */
class ChatBubble : public ui::Panel {
public:
	virtual ~ChatBubble() = default;

	/* The style class the bubble carries, which is what the sheet keys off. A chat message passes
	its role; a saved entry passes its own name, so the same card serves both columns. */
	virtual bool init(StringView styleClass);
	virtual bool init(ChatRole role);

	// The answer. Called once for a question, and on every delta while an answer streams.
	void setBody(StringView);

	// The model's thinking, shown above the answer in a muted style. The label is built on the
	// first non-empty call and not before: most models never send one.
	void setReasoning(StringView);

	// The same slot, styled as a heading: what a saved entry puts above its text.
	void setTitle(StringView);

	// One short line under the text: the model that answered, why a turn ended, what went wrong.
	void setMeta(StringView);

	// Paints the bubble as a failure - a style class, because the CSS subset has no pseudo-class
	// for it.
	void setError(bool);

	void setWrapWidth(float);

	/* The height a wrapped label ends up with is only known once it has been formatted, which is
	after whatever call changed the text. The layout calls this from its tick, and the bubble
	republishes its own height when the total changed. */
	void refreshHeight();

	StringView getBody() const;

	virtual void handleContentSizeDirty() override;

protected:
	using ui::Panel::init;

	void setTopText(StringView text, StringView styleClass);

	void applyWrapWidth() const;

	basic2d::Label *_reasoningLabel = nullptr;
	basic2d::Label *_bodyLabel = nullptr;
	basic2d::Label *_metaLabel = nullptr;

	float _wrapWidth = 0.0f;
	float _height = 0.0f;
	bool _error = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATBUBBLE_H_
