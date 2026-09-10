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

#include "agentchat/ChatHistory.h"
#include "agentchat/AgentTool.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

StringView getChatRoleName(ChatRole role) {
	switch (role) {
	case ChatRole::User: return StringView("user"); break;
	case ChatRole::Assistant: return StringView("assistant"); break;
	case ChatRole::System: return StringView("system"); break;
	case ChatRole::Tool: return StringView("tool"); break;
	}
	return StringView("user");
}

ChatMessage &ChatHistory::push(ChatRole role, StringView text) {
	auto &msg = _messages.emplace_back();
	msg.role = role;
	msg.text = text.str<Interface>();
	return msg;
}

void ChatHistory::appendDelta(StringView text, bool reasoning) {
	if (_messages.empty() || text.empty()) {
		return;
	}

	auto &msg = _messages.back();
	auto &target = reasoning ? msg.reasoning : msg.text;
	target.append(text.data(), text.size());
}

ChatMessage *ChatHistory::getLast() {
	if (_messages.empty()) {
		return nullptr;
	}
	return &_messages.back();
}

void ChatHistory::pushToolResult(StringView toolCallId, StringView content) {
	auto &msg = push(ChatRole::Tool, content);
	msg.toolCallId = toolCallId.str<Interface>();
}

void ChatHistory::clear() { _messages.clear(); }

Value ChatHistory::encodeRequest(StringView model) const {
	Value messages;

	auto first = _messages.size() > MaxTurns ? _messages.size() - MaxTurns : 0;
	for (size_t i = first; i < _messages.size(); ++i) {
		auto &msg = _messages[i];

		/* A failed turn is not part of the conversation: sending "connection refused" back as an
		assistant reply teaches the model to answer in error messages. The empty placeholder that
		the answer streams into is skipped for the same reason - on the way out it holds nothing.

		A message carrying CALLS is not empty even when its text is, and dropping it would leave
		the tool results below it answering nothing. */
		if (msg.error || (msg.text.empty() && msg.toolCalls.empty())) {
			continue;
		}

		Value entry;
		entry.setString(getChatRoleName(msg.role), "role");
		entry.setString(msg.text, "content");

		if (msg.role == ChatRole::Tool) {
			entry.setString(msg.toolCallId, "tool_call_id");
		}

		if (!msg.toolCalls.empty()) {
			Value calls;
			for (auto &call : msg.toolCalls) {
				Value function;
				function.setString(call.name, "name");
				function.setString(call.arguments, "arguments");

				Value value;
				value.setString(call.id, "id");
				value.setString("function", "type");
				value.setValue(sp::move(function), "function");
				calls.addValue(sp::move(value));
			}
			entry.setValue(sp::move(calls), "tool_calls");
		}

		messages.addValue(sp::move(entry));
	}

	Value request;
	if (!model.empty()) {
		request.setString(model, "model");
	}
	request.setBool(true, "stream");
	request.setValue(sp::move(messages), "messages");

	/* The tool declaration goes with EVERY request, not only the first. The endpoint is stateless:
	a turn sent without it is a turn where the agent has no tools at all, and the model would then
	answer a request to save something with an apology. */
	request.setValue(makeToolDeclarations(), "tools");
	return request;
}

Value ChatHistory::encodeState() const {
	Value messages;
	for (auto &msg : _messages) {
		Value entry;
		entry.setString(getChatRoleName(msg.role), "role");
		entry.setString(msg.text, "text");
		if (!msg.toolCalls.empty()) {
			Value calls;
			for (auto &call : msg.toolCalls) {
				Value value;
				value.setString(call.name, "name");
				value.setString(call.arguments, "arguments");
				calls.addValue(sp::move(value));
			}
			entry.setValue(sp::move(calls), "toolCalls");
		}
		if (!msg.reasoning.empty()) {
			entry.setString(msg.reasoning, "reasoning");
		}
		if (!msg.meta.empty()) {
			entry.setString(msg.meta, "meta");
		}
		if (msg.error) {
			entry.setBool(true, "error");
		}
		messages.addValue(sp::move(entry));
	}
	return messages;
}

} // namespace stappler::xenolith::examples
