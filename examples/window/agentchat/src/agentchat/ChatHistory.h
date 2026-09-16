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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATHISTORY_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATHISTORY_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

enum class ChatRole {
	User,
	Assistant,
	System,

	// The result of a tool the agent called. It is a message like any other and the endpoint
	// expects it back in the conversation, keyed to the call it answers.
	Tool,
};

StringView getChatRoleName(ChatRole);

/** One call the model asked for. `arguments` is what it sent: a STRING holding JSON, which is how
the protocol carries them - parsing is the caller's job, and a malformed one is an answer to give
back rather than a crash. */
struct ChatToolCall {
	String id;
	String name;
	String arguments;
};

struct ChatMessage {
	ChatRole role = ChatRole::User;

	// The answer proper. Grows one delta at a time while the answer streams.
	String text;

	/* What the model thought before answering, when it reports that separately. A reasoning model
	behind llama.cpp sends it as `delta.reasoning_content`, and a client that only reads
	`delta.content` shows an empty bubble until the thinking is over. It is displayed apart from the
	answer and never sent back: the endpoint asked for a conversation, not for its own notes. */
	String reasoning;

	// Model name, timing, or the reason a turn ended early. One short line under the text.
	String meta;

	// On an assistant message: the calls it asked for. Such a message often has NO text at all,
	// which is why "empty means skip it" stops being a safe rule once tools are in play.
	Vector<ChatToolCall> toolCalls;

	// On a Tool message: which call it answers. The endpoint matches them up by this.
	String toolCallId;

	bool error = false;
};

/** The conversation, as data. App thread only, and deliberately not a Ref: it is a member of the
layout, and the worker gets a COPY of the request body rather than a pointer to this. */
class ChatHistory {
public:
	// The number of past turns sent back with a new question. A local model has a finite context
	// and this example is not the place to implement summarization.
	static constexpr size_t MaxTurns = 24;

	ChatMessage &push(ChatRole, StringView text);

	// Appends to the message on the end of the list - the one currently streaming.
	void appendDelta(StringView text, bool reasoning);

	ChatMessage *getLast();
	SpanView<ChatMessage> getMessages() const { return _messages; }
	bool empty() const { return _messages.empty(); }

	void clear();

	// Appends the result of a call, keyed to it, as its own message.
	void pushToolResult(StringView toolCallId, StringView content);

	/* The request body: model, the `stream` flag, the tool declarations and the trailing MaxTurns
	messages. Built on the app thread and moved into the worker's task, so the worker never reads
	this object. */
	Value encodeRequest(StringView model) const;

	// For the inspector's `chat.state`.
	Value encodeState() const;

private:
	Vector<ChatMessage> _messages;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_CHATHISTORY_H_
