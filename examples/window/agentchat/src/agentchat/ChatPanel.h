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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATPANEL_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATPANEL_H_

#include "agentchat/AgentClient.h"
#include "agentchat/ChatBubble.h"
#include "agentchat/ChatHistory.h"
#include "agentchat/InspectorCommands.h"
#include "agentchat/SavedMessages.h"
#include "XLUiSelect.h"
#include "XLUiTextInput.h"
#include "XLUiButton.h"
#include "XLUiScrollSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// The rules for the chat column, added to the sheet the scene content carries.
StringView getChatPanelStylesheet();

/** The right-hand column, and everything the conversation is made of: the model picker, the log of
bubbles, the prompt, the history, the HTTP client and the tool loop.

WHY ALL OF IT IS HERE rather than split between the panel and the layout. A dock panel is built by a
lazy builder and held by the panel registry, which outlives the layout's own destructor body. A
panel that reached back into the layout for its client or its history would be reaching at exactly
the wrong moment. It owns what it uses instead, and the one thing it shares with the other column -
the list of saved notes - is counted, so neither column has to know the other exists.

THE TOOL LOOP is what makes this an agent rather than a chat. A turn that comes back asking for a
tool is not the end of the exchange: the call is executed, its result goes into the history as a
message of its own, and the same request goes out again. See handleToolCalls.

THREADING. App thread only. The one object shared with the worker is the StreamSession, and the
rules for it are on that class. handleExit cancels it and drops its callbacks, so an answer still in
flight when the window closes finds nothing to call. */
class ChatPanel : public Node {
public:
	/* Three rounds is the ceiling. One tool and one model do not make a problem where a fourth
	round adds anything, and a ceiling is what stands between showing the mechanism and letting a
	model talk to itself on somebody else's bill. */
	static constexpr size_t MaxToolTurns = 3;

	virtual ~ChatPanel() = default;

	virtual bool init(Rc<SavedMessages> &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;
	virtual void update(const UpdateTime &) override;

	// Sends what the prompt field holds, or `text` when one is given. False when the panel is busy,
	// has no model, or the text is empty.
	bool sendPrompt(StringView text);

	// Cancels the answer being streamed. False when nothing was running.
	bool stopStream();

	void clearChat();

	// Re-reads GET /models and refills the picker.
	void refreshModels();

	bool isBusy() const { return _session != nullptr; }

protected:
	using Node::init;

	// --- building --------------------------------------------------------
	void buildBar();
	void buildLog();
	void buildInputBar();

	ChatBubble *appendBubble(ChatRole role);
	ChatBubble *appendCard(StringView styleClass);

	// --- one request -----------------------------------------------------
	/* Sends the history as it stands and hangs a fresh assistant bubble under it. Called for a new
	question and again for every tool round - the difference between the two is only that the
	second adds no message from the user. */
	void startTurn();

	void handleDelta(StringView text, bool reasoning);
	void handleStreamDone(Status, StringView error);
	void handleToolCalls(SpanView<ChatToolCall>);

	// The one place that clears the busy state and wakes whoever waited for the whole exchange.
	void finishExchange(Status, StringView error);

	/* Put in front of the first question and never after: the tool declaration says a tool exists,
	this says when calling it is the right move. */
	void ensureSystemPrompt();

	// --- presentation ----------------------------------------------------
	void setStatus(StringView);
	void setBusy(bool);
	void updateSendState();
	void applyWrapWidth();

	bool isAtBottom() const;
	void scrollToBottom();
	void requestScrollToBottom();
	ui::ScrollSystem *getScrollSystem() const;

	// --- inspector -------------------------------------------------------
	void registerCommands();
	Value encodeState() const;
	void resolveWaiters(Status, StringView error);

	Rc<SavedMessages> _saved;

	Node *_bar = nullptr;
	ui::Select *_modelSelect = nullptr;
	basic2d::Label *_statusLabel = nullptr;

	Node *_log = nullptr;
	Node *_inputBar = nullptr;
	ui::TextInput *_input = nullptr;
	ui::Button *_sendButton = nullptr;

	Rc<AgentClient> _client;
	Rc<StreamSession> _session;

	ChatHistory _history;
	Vector<ChatBubble *> _bubbles;

	// The bubble the current answer is streaming into, or null.
	ChatBubble *_streamingBubble = nullptr;

	Vector<String> _models;
	String _lastError;

	bool _systemPromptInstalled = false;

	// Rounds spent in the current exchange, and calls made across all of them.
	size_t _toolTurn = 0;
	size_t _toolCalls = 0;

	float _wrapWidth = 0.0f;

	// Recomputed from where the view actually is, the way ui::Console does it: the log follows the
	// tail only while it is already at the tail, so a user who scrolled up to read something is not
	// yanked back down by the next token.
	bool _stickToBottom = true;
	bool _scrollCallbackSet = false;

	/* Frames still owed to a change in the log. The scroll range is a RESULT of the layout pass, so
	one catch-up is not enough: the box that was just added is measured on the next pass and the
	range grows again on the one after. Counted down instead of re-applied forever, because between
	messages the view has to be the user's to move. */
	uint32_t _scrollPendingFrames = 0;

	// The scroll range as of the previous frame. A range that GREW is a message that arrived, or a
	// card that finished measuring itself - either way the tail moved and has to be followed again.
	float _lastRange = 0.0f;

	Vector<Function<void(Value &&)>> _waiters;

	CommandScope _commands;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATPANEL_H_
