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

#include "agentchat/ChatPanel.h"
#include "agentchat/AgentChatConfig.h"
#include "agentchat/AgentTool.h"
#include "agentchat/SseParser.h"

#include "XLDirector.h"
#include "SPData.h"
#include "SPDataDecodeJson.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* The chat column's rules.

Two of them are load-bearing and both were paid for in debugging:

  * `#chat-log` is the scroller, and what makes it one is `flex-grow:1; flex-basis:0px` inside a
    flex COLUMN. A scroll container does not grow to its content and does not shrink to its parent
    on its own: with `height: fit-content` beside `overflow-y`, the box is always exactly as tall
    as what it holds, and there is never anything to scroll;
  * a row must not declare `width: 100%`. Its width is the column's CROSS axis, where it is
    stretched already, and 100% resolves against the log's full width, padding included - so every
    row comes out wider than the box holding it. The horizontal overflow that creates takes the
    vertical scrolling down with it, because the clip is one rectangle. */
static constexpr auto s_css = StringView(R"css(
#chat-panel { display: flex; flex-direction: column;
              flex-grow: 1; flex-shrink: 1; flex-basis: 0px; }

#chat-bar { order: 0; flex: 0 0 var(--bar-h); -xl-z-order: 3;
            display: flex; flex-direction: row; align-items: center;
            column-gap: 10px; padding: 0px 12px; background-color: #14141a; }
#chat-bar > label.caption { flex: 0 0 46px; color: var(--muted); font-size: 13px; }
#chat-status { flex-grow: 1; color: var(--muted); font-size: 12px; white-space: nowrap; }

select { flex: 0 0 260px; height: var(--field-h);
         display: flex; flex-direction: row; align-items: center; column-gap: 6px;
         padding: 0px 8px; background-color: var(--control);
         outline-width: 1px; outline-color: var(--outline); border-radius: 6px; }
select:hover { outline-color: #6a6a78; }
select:focus, select.open { outline-color: var(--accent); }
select > label { color: var(--text); font-size: 13px; white-space: nowrap; }
select > select-arrow { width: 16px; height: 16px; color: var(--muted); }

#chat-log { order: 1; flex-grow: 1; flex-shrink: 1; flex-basis: 0px; -xl-z-order: 1;
            display: flex; flex-direction: column; row-gap: 10px;
            padding: 14px 16px; overflow-y: auto; }

/* align-items must NOT stay at its default. A row is a flex container, and `stretch` would hand
   the bubble the row's own height on the cross axis - overwriting the height the card publishes
   for itself, and leaving the frame a step behind the text for as long as an answer streams. */
.msg-row { flex: 0 0 auto; display: flex; flex-direction: row; align-items: flex-start; }
.msg-row.user      { justify-content: flex-end; }
.msg-row.assistant { justify-content: flex-start; }
.msg-row.system    { justify-content: center; }
.msg-row.tool      { justify-content: flex-start; }

panel.bubble.user      { background-color: var(--user-bg); }
panel.bubble.assistant { background-color: var(--agent-bg);
                         outline-width: 1px; outline-color: var(--outline); }
panel.bubble.system    { background-color: #26262e;
                         outline-width: 1px; outline-color: var(--outline); }

/* What the agent asked for, and what came back. Two colours, because "the model wants to run this"
   and "this is what running it did" are different things to a reader. */
panel.bubble.tool-call { background-color: #2a2620;
                         outline-width: 1px; outline-color: #6b5a3f; }
panel.bubble.tool      { background-color: #1d2a22;
                         outline-width: 1px; outline-color: #3f6b4f; }
/* The three rows of a chat card: what the model thought, what it answered, and the line under it. */
panel.bubble > label.reasoning { color: rgba(232,232,236,.55); font-size: 12px;
                                 font-style: italic; }
panel.bubble > label.body      { color: var(--text); font-size: 14px; }
panel.bubble > label.meta      { color: rgba(232,232,236,.5); font-size: 11px; }
panel.bubble > label.title     { color: var(--text); font-size: 13px; font-weight: bold; }

panel.bubble.error > label.meta     { color: var(--danger); }

/* The card holding the scene's selection: picked by a tap or the arrow keys, or holding selected
   answer text. After the role rules, which declare an outline of the same specificity. */
panel.bubble:selection-within { outline-width: 2px; outline-color: var(--accent); }

/* ---- the answer, as a document ---------------------------------------- */

/* ui::MarkdownView arrives with a stylesheet of its own so that a document is readable before an
   application says anything. That sheet is written for black text on paper, and every rule in it
   is a BARE TAG selector on purpose: specificity 0,0,1 is the lowest a matching rule can have, so
   anything keyed on the view's class outranks it without a fight. Every rule below is.

   The one thing NOT overridden is `overflow-y`, and leaving it alone is load-bearing. It reads as
   a scroll the card does not want - the card is exactly as tall as its answer, so there is never
   anything to scroll - but that declaration is also what puts the layout inside the view into
   overflow mode on the vertical axis, and in that mode the document is laid out at its NATURAL
   height instead of being squeezed into the box (ui::LayoutSystem::setOverflowAxes). Turn it off
   and every block in the answer is crushed to a fraction of a pixel by the default flex-shrink,
   which is exactly how tall the card believed the answer was. */
markdown-view.answer { font-size: 14px; color: var(--text); }

/* The card has padding of its own, and the document's own would sit inside it twice over. */
.answer .md-body { padding: 0px; }

/* The blocks. Each one is a node typed with its html tag, so a tag name here names a real element;
   the inline constructs below are style RANGES and are matched through a probe the widget makes
   for the length of the cascade query, which is why the same descendant selectors reach both. */
.answer p, .answer li, .answer dd, .answer dt, .answer .md-text { color: var(--text);
                                                                  font-size: 14px; }
.answer p { margin-bottom: 6px; }
.answer ul, .answer ol, .answer dl { margin-bottom: 6px; }
.answer li-marker { color: var(--muted); }

.answer h1, .answer h2, .answer h3, .answer h4, .answer h5, .answer h6 {
	color: var(--text); margin-top: 10px; margin-bottom: 6px; }
.answer h1 { font-size: 19px; }
.answer h2 { font-size: 17px; }
.answer h3 { font-size: 15px; }
.answer h4, .answer h5, .answer h6 { font-size: 14px; color: var(--muted); }

/* THE INLINE CONSTRUCTS, and why each one has to name a colour even when it is not changing one.

An inline is a style range, not a node: the widget manufactures a PROBE for the length of the
cascade query and asks what the sheet says about `strong` here (ui::MarkdownInlineResolver). What
comes back is applied as a DELTA over the block's own style, and a probe that resolves `color` to
something other than the block's contributes that colour to the range whether or not the rule meant
to. On paper the difference is #1a1a1a against black and nobody sees it. On this ground the block is
near-white and the probe is not, and every bold word in an answer comes out unreadable. */
.answer strong, .answer b, .answer em, .answer i,
.answer del, .answer s, .answer sub, .answer sup { color: #e8e8ec; }

.answer a   { color: #7fb3ef; }
.answer ins { color: #7fcf8a; }
.answer mark { color: #f0d060; }

/* `code` is both the inline construct and the block inside a fence, which is why the fenced one
   has to name its colour again - the built-in sheet does the same thing for the same reason. */
.answer code          { color: #e0c48a; }
.answer pre           { background-color: #101016; border-radius: 4px; padding: 8px 10px;
                        margin-bottom: 8px; }
.answer pre code      { color: #d8d8e0; font-size: 12px; }

.answer blockquote      { margin-bottom: 8px; }
.answer blockquote-bar  { background-color: var(--outline); }
.answer blockquote-body { background-color: #1a1a22; }

.answer hr { background-color: var(--outline); }

/* A table is a painted panel, not a bare node - it is what draws the collapsed borders - so it has
   a background whether the sheet names one or not, and the one it is born with is white. Named
   rather than made transparent: alpha on a colour here becomes the node's OPACITY, and that
   multiplies down over every cell in the table. */
.answer table          { background-color: #26262e; }
.answer th, .answer td { border-color: var(--outline); color: #e8e8ec; }

.answer li-checkbox         { background-color: var(--control); outline-color: var(--outline); }
.answer li-checkbox:checked { background-color: var(--accent); outline-color: var(--accent); }

/* The card a turn that asked for a tool leaves behind: the same document widget, one hue warmer. */
panel.bubble.tool-call > label.body { color: #e0c48a; font-size: 12px; }
.bubble.tool-call .answer p    { color: #e0c48a; font-size: 12px; }
.bubble.tool-call .answer code { color: #f0d8a8; }
.bubble.tool-call .answer pre  { background-color: #1d1a14; }

#chat-input-bar { order: 2; flex: 0 0 auto; -xl-z-order: 2;
                  display: flex; flex-direction: row; align-items: center;
                  column-gap: 8px; padding: 10px 12px; background-color: var(--panel); }

text-input { flex-grow: 1; height: var(--field-h);
             background-color: var(--control);
             outline-width: 1px; outline-color: var(--outline); border-radius: 6px;
             padding: 0px 10px; color: var(--text); font-size: 14px;
             --caret-color: #fcb400; --selection-color: rgba(252,180,0,.35); }
text-input:focus { outline-color: var(--accent); }
label.xl-ui-text-input-placeholder { color: #6a6a72; }

button#send { flex: 0 0 96px; height: var(--field-h);
              display: flex; flex-direction: row; align-items: center; justify-content: center;
              background-color: var(--accent); border-radius: 6px; }
button#send:hover    { background-color: #4a8ddc; }
button#send:disabled { background-color: #33333c; }
button#send > label  { color: #ffffff; font-size: 13px; white-space: nowrap; }
button#send.stopping       { background-color: #5a4326; }
button#send.stopping:hover { background-color: #6d5230; }
)css");

// A bubble is bounded at this share of the log, and the log has padding on each side. What is left
// is what the text may occupy; the rest is the bubble's own horizontal padding.
static constexpr float s_bubbleWidthRatio = 0.78f;
static constexpr float s_bubbleChrome = 24.0f + 32.0f;
static constexpr float s_minWrapWidth = 140.0f;

} // namespace

StringView getChatPanelStylesheet() { return s_css; }

bool ChatPanel::init(Rc<SavedMessages> &&saved) {
	if (!Node::init()) {
		return false;
	}

	_saved = sp::move(saved);
	setName("chat-panel");

	buildBar();
	buildLog();
	buildInputBar();

	return true;
}

void ChatPanel::buildBar() {
	_bar = addChild(Rc<Node>::create(), ZOrder(1));
	_bar->setName("chat-bar");

	auto caption = _bar->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	caption->setType("label");
	caption->addStyleClass("caption");
	caption->setString("Model");

	_modelSelect = _bar->addChild(Rc<ui::Select>::create(), ZOrder(2));
	_modelSelect->setName("model-select");
	_modelSelect->setPlaceholder("connecting...");

	/* Keep the list inside the scene. A native popup is a window of its own, which a headless
	session cannot see and a screenshot does not include - and this example is meant to be driven
	from a script as much as by hand. */
	ui::MenuConfig config;
	config.preferNative = false;
	_modelSelect->setPopupConfig(sp::move(config));

	_modelSelect->setChangeCallback([this](StringView id) {
		setStatus(toString("model: ", makeModelTitle(id)));
		updateSendState();
	});

	_statusLabel = _bar->addChild(Rc<basic2d::Label>::create(), ZOrder(3));
	_statusLabel->setType("label");
	_statusLabel->setName("chat-status");
}

void ChatPanel::buildLog() {
	/* The scroller. It gets its height from being a stretched item of this column, which is what
	lets `overflow-y: auto` have anything to scroll - and the ui::ScrollSystem itself is created by
	the resolver, from that one declaration, rather than added here. */
	_log = addChild(Rc<Node>::create(), ZOrder(2));
	_log->setName("chat-log");
}

void ChatPanel::buildInputBar() {
	_inputBar = addChild(Rc<Node>::create(), ZOrder(3));
	_inputBar->setName("chat-input-bar");

	_input = _inputBar->addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_input->setName("prompt");
	_input->setPlaceholder("Ask the agent...");
	_input->setEnterCallback([this] { sendPrompt(StringView()); });

	_sendButton = _inputBar->addChild(Rc<ui::Button>::create(), ZOrder(2));
	_sendButton->setName("send");
	_sendButton->setString("Send");
	_sendButton->setCallback([this] {
		if (isBusy()) {
			stopStream();
		} else {
			sendPrompt(StringView());
		}
	});
	_sendButton->setEnabled(false);
}

// ---- lifecycle ---------------------------------------------------------------------------

void ChatPanel::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	/* Scheduled HERE and not in init(): a node schedules against the scene it is in, and in init()
	there is none yet - the request is made against nothing and quietly does not happen. */
	scheduleUpdate();

	_commands.attach(scene);
	registerCommands();

	// The app thread is what the client posts its work to, and it is reachable only once this node
	// is in a scene.
	if (!_client) {
		_client = Rc<AgentClient>::create(getDirector()->getApplication());
	}

	setStatus(toString("endpoint: ", getAgentEndpoint()));
	refreshModels();

	_input->focus();
}

void ChatPanel::handleExit() {
	/* An answer may still be in flight, and the worker holds the session rather than this node.
	Cancelling stops the transfer; dropping the callbacks makes whatever is already queued a
	no-op. Without the second half, a delta posted a moment ago would call into a destroyed node. */
	if (_session) {
		_session->cancelled.store(true);
		_session->onDelta = nullptr;
		_session->onDone = nullptr;
		_session = nullptr;
	}

	_waiters.clear();
	_toolTurn = 0;
	_commands.detach();

	Node::handleExit();
}

void ChatPanel::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	// The panel itself is sized by the dock frame that holds it; only the bubbles need telling.
	applyWrapWidth();
}

void ChatPanel::update(const UpdateTime &time) {
	Node::update(time);

	/* The scroll range is a RESULT of the layout pass, so it lags whatever was just added by a
	frame - and while an answer streams it grows again on the next one. Sticking to the tail is
	therefore a standing intent re-applied every frame, not a one-off scroll after each delta.

	What ends it is the user scrolling up: the callback below re-reads the intent from where the
	view actually is, so our own scroll keeps it and theirs drops it. */
	if (auto scroll = getScrollSystem()) {
		if (!_scrollCallbackSet) {
			_scrollCallbackSet = true;

			/* Wherever the view ends up, that is the intent from now on - our own catch-up lands on
			the tail and keeps it, a wheel notch lands short of it and drops it. */
			scroll->setScrollCallback([this](Vec2) { _stickToBottom = isAtBottom(); });
		}

		/* The view is moved only when the log GREW, and only while the tail is being followed.
		Re-applying the tail on every frame instead is what made the wheel useless: a notch starts
		an easing, and the next frame would put the view straight back. Between messages the view
		belongs to whoever is reading it.

		Growth rather than a frame count, because a card measures itself over the passes that
		follow the text it was given: the range keeps creeping up after the message itself is long
		delivered, and a fixed number of catch-up frames stops halfway through. */
		auto range = scroll->getScrollRange().height;
		const bool grew = range > _lastRange + 0.5f;
		_lastRange = range;

		if (_scrollPendingFrames > 0) {
			--_scrollPendingFrames;
		}

		if (_stickToBottom && (grew || _scrollPendingFrames > 0)) {
			scrollToBottom();
		}
	}

	/* Two things per card, and only the second is a safety net.

	tickBody is the mechanism for a Markdown answer: setBody only records a delta, and this is what
	decides that enough of them have piled up to be worth rebuilding the document for.

	refreshHeight republishes a height the card already knows; a card does that from its own setters
	too, and this only catches a re-shape nobody asked for - a font that finished loading, a density
	change, or the layout pass that finally measured a document committed a frame ago. */
	for (auto &it : _bubbles) {
		it->tickBody(time.app);
		it->refreshHeight();
	}
}

// ---- the log -----------------------------------------------------------------------------

ChatBubble *ChatPanel::appendCard(StringView styleClass, bool markdown) {
	auto row = _log->addChild(Rc<Node>::create(), ZOrder(int16_t(_bubbles.size() + 1)));
	row->addStyleClass("msg-row");
	row->addStyleClass(styleClass);

	auto bubble = row->addChild(Rc<ChatBubble>::create(styleClass, markdown), ZOrder(1));
	bubble->setWrapWidth(_wrapWidth);

	_bubbles.emplace_back(bubble);
	requestScrollToBottom();

	return bubble;
}

// The role decides the slot: what the model writes is Markdown, everything else is flat text.
ChatBubble *ChatPanel::appendBubble(ChatRole role) {
	return appendCard(getChatRoleName(role), role == ChatRole::Assistant);
}

// ---- one exchange ------------------------------------------------------------------------

void ChatPanel::ensureSystemPrompt() {
	if (_systemPromptInstalled) {
		return;
	}

	_systemPromptInstalled = true;

	/* First message of the history and before the first question. It gets no bubble: half a screen
	of instructions is noise on a chat log, and `chat.tools` shows it verbatim to anyone who asks.
	*/
	_history.push(ChatRole::System, getAgentSystemPrompt());
}

bool ChatPanel::sendPrompt(StringView text) {
	if (isBusy()) {
		return false;
	}

	auto prompt = text.empty() ? _input->getText() : text;
	prompt.trimChars<StringView::WhiteSpace>();
	if (prompt.empty()) {
		return false;
	}

	if (_modelSelect->getValue().empty()) {
		setStatus("no model to ask - the endpoint listed none");
		return false;
	}

	_lastError.clear();
	_toolTurn = 0;
	_toolCalls = 0;

	ensureSystemPrompt();

	_history.push(ChatRole::User, prompt);
	appendBubble(ChatRole::User)->setBody(prompt);

	// Assigning to a TextInput is a REQUEST to the platform; the text comes back by echo. Clearing
	// it here is the ordinary path, and what the field shows next is what the platform says.
	_input->setText(StringView());

	startTurn();
	return true;
}

void ChatPanel::startTurn() {
	auto model = _modelSelect->getValue();

	// The placeholder the answer streams into. It exists before a single byte is sent, so there is
	// somewhere for the first delta to land and something on screen saying the question was heard.
	_history.push(ChatRole::Assistant, StringView());
	_streamingBubble = appendBubble(ChatRole::Assistant);
	_streamingBubble->setMeta(makeModelTitle(model));

	// Built here, on the app thread, and moved into the worker: the history itself never leaves
	// this thread.
	auto request = _history.encodeRequest(model);

	_session = _client->startCompletion(this, model, sp::move(request));
	_session->onDelta = [this](StringView delta, bool reasoning) {
		handleDelta(delta, reasoning);
	};
	_session->onDone = [this](Status status, StringView error) { handleStreamDone(status, error); };

	setBusy(true);
	setStatus(_toolTurn > 0 ? StringView("sending the tool result back...")
							: StringView("waiting for the first token..."));
}

bool ChatPanel::stopStream() {
	if (!_session) {
		return false;
	}

	// The worker sees this on its next chunk and aborts the transfer from inside the write
	// callback; the completion still runs, and it is what tidies the UI up.
	_session->cancelled.store(true);
	return true;
}

void ChatPanel::handleDelta(StringView text, bool reasoning) {
	if (!_streamingBubble) {
		return;
	}

	_history.appendDelta(text, reasoning);

	auto message = _history.getLast();
	if (!message) {
		return;
	}

	if (reasoning) {
		_streamingBubble->setReasoning(message->reasoning);
	} else {
		_streamingBubble->setBody(message->text);
	}

	requestScrollToBottom();
}

void ChatPanel::handleStreamDone(Status st, StringView error) {
	auto session = _session;
	_session = nullptr;

	auto message = _history.getLast();

	// The last deltas of an answer are still behind the rebuild throttle, and nothing is coming
	// after them to push them through.
	if (_streamingBubble) {
		_streamingBubble->flushBody();
	}

	if (_streamingBubble && session) {
		String meta;

		if (st == Status::Declined) {
			meta = toString(makeModelTitle(session->model), " - stopped");
		} else if (!sprt::status::isSuccessful(st)) {
			_lastError = error.str<Interface>();
			_streamingBubble->setError(true);
			if (message) {
				message->error = true;
			}
			// An empty bubble with a red outline says nothing; the reason goes where the answer
			// would have been.
			if (_streamingBubble->getBody().empty()) {
				_streamingBubble->setBody(error);
			}
			meta = error.str<Interface>();
		} else {
			meta = makeModelTitle(session->model);
			if (session->tokensPerSecond > 0.0) {
				meta = toString(meta, " - ", uint32_t(session->tokensPerSecond), " tok/s");
			}
			if (!session->finishReason.empty() && session->finishReason != "stop") {
				meta = toString(meta, " - ", session->finishReason);
			}
		}

		_streamingBubble->setMeta(meta);
		if (message) {
			message->meta = meta;
		}
	}

	/* The turn asked for a tool, so the exchange is NOT over: the calls are run and the same
	conversation goes out again with their results in it. Whoever is waiting on `chat.wait` is left
	waiting, which is the point - a script wants the end of the exchange, not the end of a leg. */
	if (sprt::status::isSuccessful(st) && session && !session->toolCalls.empty()
			&& session->finishReason == "tool_calls") {
		handleToolCalls(session->toolCalls);
		return;
	}

	_streamingBubble = nullptr;

	if (st == Status::Declined) {
		setStatus("stopped");
	} else if (!sprt::status::isSuccessful(st)) {
		setStatus(error);
	} else if (session && session->jsonErrors > 0) {
		// Survivable, but not something to hide: a server whose events do not parse is a server
		// worth looking at.
		setStatus(toString("done - ", session->events, " events, ", session->jsonErrors,
				" unparsed"));
	} else if (session) {
		setStatus(toString("done - ", session->events, " events, ", session->bytes, " bytes"));
	}

	finishExchange(st, error);
}

void ChatPanel::handleToolCalls(SpanView<ChatToolCall> calls) {
	/* The assistant turn that asked for the calls is a real turn of the conversation and has to go
	back in the next request: the endpoint matches each tool result to the call that asked for it,
	and with that turn missing there is nothing for the results to answer. */
	if (auto message = _history.getLast()) {
		message->toolCalls = calls.vec<Interface>();
	}

	/* No separate bubble for the request itself. The bubble this turn streamed into is already
	there and is usually empty - a model that decides to call a tool often writes nothing first -
	so it becomes the record of the call instead of hanging around as an unexplained blank box. */
	if (_streamingBubble && _streamingBubble->getBody().empty()) {
		_streamingBubble->addStyleClass("tool-call");

		/* Written as Markdown, because that bubble IS one: the arguments are JSON and a fence is
		what keeps them readable - and keeps a stray underscore in them from turning the rest of
		the line italic. */
		_streamingBubble->setBody(toString("calls `", calls.front().name, "`\n\n```json\n",
				calls.front().arguments, "\n```"));
		_streamingBubble->flushBody();
	}
	_streamingBubble = nullptr;

	for (auto &call : calls) {
		auto result = invokeTool(*_saved, call.name, call.arguments);
		++_toolCalls;

		// Two destinations, two shapes: the model gets JSON as the content of a `tool` message, a
		// person watching gets one line.
		_history.pushToolResult(call.id, data::toString<mem_std::Interface>(result.payload));

		auto bubble = appendCard("tool");
		bubble->setTitle(toString(call.name, result.ok ? "" : " - failed"));
		bubble->setBody(result.summary);
		bubble->setError(!result.ok);

		setStatus(result.summary);
	}

	++_toolTurn;

	if (_toolTurn >= MaxToolTurns) {
		/* The ceiling is a decision of this client, not a failure of the server, so it is said in
		the log rather than reported as an error. */
		appendBubble(ChatRole::System)
				->setBody(toString("tool loop stopped after ", _toolTurn, " rounds"));
		setStatus("tool loop stopped");
		finishExchange(Status::Ok, StringView());
		return;
	}

	// Round two: the whole history, now including the calls and their results.
	startTurn();
}

void ChatPanel::finishExchange(Status st, StringView error) {
	setBusy(false);
	resolveWaiters(st, error);
}

void ChatPanel::clearChat() {
	_history.clear();
	_bubbles.clear();
	_streamingBubble = nullptr;
	_lastError.clear();
	_systemPromptInstalled = false;
	_toolTurn = 0;
	_toolCalls = 0;

	// The rows are the bubbles' parents, and dropping them takes the bubbles with them.
	_log->removeAllChildren();

	_stickToBottom = true;
}

void ChatPanel::refreshModels() {
	if (!_client) {
		return;
	}

	setStatus(toString("asking ", getAgentEndpoint(), " for its models..."));

	_client->fetchModels(this, [this](Status st, Vector<String> &&models, String &&error) {
		if (!sprt::status::isSuccessful(st)) {
			_lastError = sp::move(error);
			setStatus(toString("no models: ", _lastError));
			updateSendState();
			return;
		}

		_models = sp::move(models);
		_modelSelect->setOptions(ui::makeSelectOptions(_models));

		/* Titles are shortened for display, ids are not: what goes back to the endpoint has to be
		what it gave us. setOptions took id==title, so the titles are rewritten in place. */
		auto options = _modelSelect->getOptions().vec<Interface>();
		for (auto &it : options) { it.title = makeModelTitle(it.id); }
		_modelSelect->setOptions(options);

		// The model the user named, when the endpoint actually serves it; otherwise the first one.
		auto hint = getAgentModelHint();
		if (hint.empty() || !_modelSelect->setValue(hint)) {
			_modelSelect->setValue(_models.front());
		}

		setStatus(toString(_models.size(), " model(s) at ", getAgentEndpoint()));
		updateSendState();
	});
}

// ---- presentation ------------------------------------------------------------------------

void ChatPanel::setStatus(StringView text) { _statusLabel->setString(text); }

void ChatPanel::setBusy(bool busy) {
	if (busy) {
		_sendButton->setString("Stop");
		_sendButton->addStyleClass("stopping");
	} else {
		_sendButton->setString("Send");
		_sendButton->removeStyleClass("stopping");
	}

	_input->setReadOnly(busy);
	updateSendState();
}

void ChatPanel::updateSendState() {
	// Stop is always available while busy; Send needs something to ask.
	_sendButton->setEnabled(isBusy() || !_modelSelect->getValue().empty());
}

void ChatPanel::applyWrapWidth() {
	auto width = getContentSize().width * s_bubbleWidthRatio - s_bubbleChrome;
	if (width < s_minWrapWidth) {
		width = s_minWrapWidth;
	}

	if (_wrapWidth == width) {
		return;
	}

	_wrapWidth = width;
	for (auto &it : _bubbles) { it->setWrapWidth(_wrapWidth); }
}

ui::ScrollSystem *ChatPanel::getScrollSystem() const {
	// Created by the style resolver from `overflow-y: auto`, so it does not exist until the log has
	// been styled once.
	return _log ? _log->getSystemByType<ui::ScrollSystem>() : nullptr;
}

bool ChatPanel::isAtBottom() const {
	auto scroll = getScrollSystem();
	if (!scroll) {
		return true;
	}
	return scroll->getScrollRange().height - scroll->getScrollPosition().y < 1.0f;
}

void ChatPanel::scrollToBottom() {
	auto scroll = getScrollSystem();
	if (!scroll) {
		return;
	}

	auto position = scroll->getScrollPosition();
	auto end = scroll->getScrollRange().height;

	if (end - position.y < 1.0f) {
		return;
	}

	scroll->setScrollPosition(Vec2(position.x, end));
}

void ChatPanel::requestScrollToBottom() {
	// Three passes: one for the node just added, one for the range that grows around it, one spare.
	_scrollPendingFrames = 3;
}

// ---- inspector ---------------------------------------------------------------------------

Value ChatPanel::encodeState() const {
	Value state;
	state.setString(getAgentEndpoint(), "endpoint");
	state.setBool(!getAgentApiKey().empty(), "apiKeySet");
	state.setString(_modelSelect ? _modelSelect->getValue() : StringView(), "model");
	state.setBool(_session != nullptr, "busy");
	state.setBool(_systemPromptInstalled, "toolsInstalled");
	state.setInteger(int64_t(_toolTurn), "toolTurn");
	state.setInteger(int64_t(_toolCalls), "toolCalls");
	state.setInteger(int64_t(_saved->size()), "savedCount");

	Value models;
	for (auto &it : _models) { models.addString(it); }
	state.setValue(sp::move(models), "models");

	state.setValue(_history.encodeState(), "messages");

	if (!_lastError.empty()) {
		state.setString(_lastError, "lastError");
	}

	if (auto scroll = getScrollSystem()) {
		state.setDouble(scroll->getScrollRange().height, "scrollRange");
		state.setDouble(scroll->getScrollPosition().y, "scrollPosition");
	}

	state.setDouble(_wrapWidth, "wrapWidth");
	state.setBool(_stickToBottom, "stickToBottom");

	return state;
}

void ChatPanel::resolveWaiters(Status st, StringView error) {
	if (_waiters.empty()) {
		return;
	}

	// Taken first: a waiter is free to send the next question from its own callback.
	auto waiters = sp::move(_waiters);
	_waiters.clear();

	Value result;
	result.setBool(sprt::status::isSuccessful(st), "ok");
	result.setString(toString(st), "status");
	result.setInteger(int64_t(_toolCalls), "toolCalls");
	result.setInteger(int64_t(_toolTurn), "toolTurns");
	result.setInteger(int64_t(_saved->size()), "saved");
	if (!error.empty()) {
		result.setString(error, "error");
	}

	if (auto message = const_cast<ChatHistory &>(_history).getLast()) {
		result.setString(message->text, "text");
		if (!message->reasoning.empty()) {
			result.setString(message->reasoning, "reasoning");
		}
	}

	for (auto &it : waiters) { it(Value(result)); }
}

void ChatPanel::registerCommands() {
	_commands.add("chat.state", "Endpoint, model list, conversation, tool state and scrolling",
			[this](const Value &, Function<void(Value &&)> &&done) { done(encodeState()); });

	_commands.add("chat.models", "Re-read GET /models and refill the picker",
			[this](const Value &, Function<void(Value &&)> &&done) {
		/* Answered when the request comes back, not when it is sent. An inspector command may
		finish later and from another thread hop, which is what makes a network call scriptable at
		all. */
		_client->fetchModels(this,
				[this, done = sp::move(done)](Status st, Vector<String> &&models,
						String &&error) mutable {
			if (sprt::status::isSuccessful(st)) {
				_models = models;
				_modelSelect->setOptions(ui::makeSelectOptions(_models));
				auto options = _modelSelect->getOptions().vec<Interface>();
				for (auto &it : options) { it.title = makeModelTitle(it.id); }
				_modelSelect->setOptions(options);
				if (_modelSelect->getValue().empty() && !_models.empty()) {
					_modelSelect->setValue(_models.front());
				}
				updateSendState();
			}

			Value result;
			result.setBool(sprt::status::isSuccessful(st), "ok");
			Value list;
			for (auto &it : models) { list.addString(it); }
			result.setValue(sp::move(list), "models");
			if (!error.empty()) {
				result.setString(error, "error");
			}
			done(sp::move(result));
		});
	});

	_commands.add("chat.select", "Choose a model by id: {\"model\": \"...\"}",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		auto ok = _modelSelect->setValue(args.getString("model"));
		updateSendState();

		Value result;
		result.setBool(ok, "ok");
		result.setString(_modelSelect->getValue(), "model");
		done(sp::move(result));
	});

	_commands.add("chat.send", "Ask the agent: {\"text\": \"...\"}. Returns at once; see chat.wait",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		auto started = sendPrompt(args.getString("text"));

		Value result;
		result.setBool(started, "started");
		result.setBool(_session != nullptr, "busy");
		if (!started && !_lastError.empty()) {
			result.setString(_lastError, "error");
		}
		done(sp::move(result));
	});

	_commands.add("chat.wait", "Wait for the whole exchange, tool rounds included",
			[this](const Value &, Function<void(Value &&)> &&done) {
		if (!_session) {
			// Nothing running: answer with what the last turn produced rather than hanging.
			Value result;
			result.setBool(true, "ok");
			result.setBool(false, "waited");
			result.setInteger(int64_t(_toolCalls), "toolCalls");
			result.setInteger(int64_t(_saved->size()), "saved");
			if (auto message = _history.getLast()) {
				result.setString(message->text, "text");
			}
			done(sp::move(result));
			return;
		}

		_waiters.emplace_back(sp::move(done));
	});

	_commands.add("chat.stop", "Cancel the answer being streamed",
			[this](const Value &, Function<void(Value &&)> &&done) {
		auto wasBusy = stopStream();

		Value result;
		result.setBool(true, "ok");
		result.setBool(wasBusy, "wasBusy");
		done(sp::move(result));
	});

	_commands.add("chat.clear", "Drop the conversation and the bubbles",
			[this](const Value &, Function<void(Value &&)> &&done) {
		stopStream();
		clearChat();

		Value result;
		result.setBool(true, "ok");
		done(sp::move(result));
	});

	_commands.add("chat.scroll", "Move the log: {\"to\": \"bottom\"|\"top\"|<offset>}",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		Value result;

		auto scroll = getScrollSystem();
		if (!scroll) {
			result.setBool(false, "ok");
			done(sp::move(result));
			return;
		}

		const Value &to = args.getValue("to");
		if (to.isString() && to.getString() == "top") {
			scroll->setScrollPosition(Vec2(scroll->getScrollPosition().x, 0.0f));
		} else if (to.isBasicType() && !to.isString()) {
			scroll->setScrollPosition(Vec2(scroll->getScrollPosition().x, float(to.getDouble())));
		} else {
			scrollToBottom();
		}

		_stickToBottom = isAtBottom();

		result.setBool(true, "ok");
		result.setDouble(scroll->getScrollRange().height, "range");
		result.setDouble(scroll->getScrollPosition().y, "position");
		done(sp::move(result));
	});

	_commands.add("chat.tools", "What the agent is told about its tools, verbatim",
			[this](const Value &, Function<void(Value &&)> &&done) {
		Value result;
		result.setBool(true, "ok");
		result.setBool(_systemPromptInstalled, "installed");
		result.setInteger(int64_t(MaxToolTurns), "maxToolTurns");
		result.setString(getAgentSystemPrompt(), "instructions");
		result.setValue(makeToolDeclarations(), "tools");
		done(sp::move(result));
	});

	_commands.add("chat.request", "The request body as it would be sent right now, without sending",
			[this](const Value &, Function<void(Value &&)> &&done) {
		/* The whole protocol in one answer: the tool declaration, the system message first, and -
		once a tool has been called - what an assistant turn with calls and a `tool` reply look
		like on the wire. No network involved. */
		Value result;
		result.setBool(true, "ok");
		result.setValue(_history.encodeRequest(_modelSelect->getValue()), "request");
		done(sp::move(result));
	});

	_commands.add("chat.call-tool",
			"Run a tool directly: {\"name\", \"arguments\"} with arguments as a JSON string",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		auto name = args.getString("name");
		auto result = invokeTool(*_saved, name.empty() ? SaveMessageToolName : StringView(name),
				args.getString("arguments"));

		Value value;
		value.setBool(result.ok, "ok");
		value.setString(result.summary, "summary");
		value.setValue(result.payload, "payload");
		value.setInteger(int64_t(_saved->size()), "savedCount");
		done(sp::move(value));
	});

	_commands.add("chat.sse-selftest", "Run the SSE reader against its scripted stream. No network",
			[](const Value &, Function<void(Value &&)> &&done) {
		auto result = runSseSelfTest();

		Value value;
		value.setInteger(int64_t(result.first), "checks");
		value.setInteger(int64_t(result.second), "failures");
		value.setBool(result.second == 0, "ok");
		done(sp::move(value));
	});

	_commands.add("chat.fake-stream",
			"Feed raw SSE bytes to the UI: {\"chunks\": [...]}. No network, no model",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		/* The whole display path - parse, delta, bubble, autoscroll - driven from a script on a
		machine with no agent to talk to. Only the socket is missing, and the bytes below are the
		shape a socket would have delivered. */
		if (!_streamingBubble) {
			_history.push(ChatRole::Assistant, StringView());
			_streamingBubble = appendBubble(ChatRole::Assistant);
			_streamingBubble->setMeta("fake stream");
		}

		SseParser parser;
		size_t deltas = 0;
		auto finished = false;

		auto sink = [&](StringView, StringView payload) {
			if (payload == "[DONE]") {
				// Ends the message, the way a real stream does - so a script can push several
				// messages into the log one after another.
				finished = true;
				return;
			}

			const Value value = data::json::read<mem_std::Interface>(payload);
			const Value &choices = value.getValue("choices");
			if (!choices.isArray() || choices.size() == 0) {
				return;
			}

			const Value &delta = choices.getValue(0).getValue("delta");
			auto &reasoning = delta.getString("reasoning_content");
			if (!reasoning.empty()) {
				handleDelta(reasoning, true);
				++deltas;
			}
			auto &content = delta.getString("content");
			if (!content.empty()) {
				handleDelta(content, false);
				++deltas;
			}
		};
		const SseParser::EventCallback callback(sink);

		const Value &chunks = args.getValue("chunks");
		if (chunks.isArray()) {
			for (auto &it : chunks.asArray()) { parser.consume(it.getString(), callback); }
		}
		parser.finish(callback);

		if (finished) {
			// The same thing the end of a real turn does: a Markdown answer is committed on a
			// throttle, and nothing is coming after these deltas to push the last of them through.
			_streamingBubble->flushBody();
			_streamingBubble = nullptr;
		}

		Value result;
		result.setBool(true, "ok");
		result.setBool(finished, "finished");
		result.setInteger(int64_t(deltas), "deltas");
		if (auto message = _history.getLast()) {
			result.setString(message->text, "text");
		}
		done(sp::move(result));
	});
}

} // namespace stappler::xenolith::examples
