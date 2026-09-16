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

#include "agentchat/AgentClient.h"
#include "agentchat/AgentChatConfig.h"
#include "agentchat/SseParser.h"

#include "SPNetworkHandle.h"
#include "SPData.h"
#include "SPDataDecodeJson.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* curl's sentinel for "the write callback failed", which is the only way to abort a transfer from
inside it. Spelled out here rather than pulled in with curl's headers, which stappler_network keeps
to itself - the same trick, for the same reason, as xenolith/resources/assets/XLAsset.cc. */
#ifndef CURL_WRITEFUNC_ERROR
#define CURL_WRITEFUNC_ERROR 0xFFFF'FFFF
#endif

// Enough to see what a server is complaining about, without pasting a whole HTML error page into a
// chat bubble.
static constexpr size_t s_maxErrorTextSize = 400;

// One in-flight /models request: the buffer the worker fills, and what the completion callback
// reads back on the app thread.
struct ModelsRequest : public Ref {
	String body;
	Value result;
	String error;
};

static void setupCommonRequest(mem_std::NetworkHandle &handle) {
	auto key = getAgentApiKey();
	if (!key.empty()) {
		// addHeader lowercases the name and drops anything with a CR or LF in it, so a key pasted
		// with a stray newline fails as a missing header rather than as a split request.
		handle.addHeader("Authorization", toString("Bearer ", key));
	}

	// An agent that is not running should say so in a couple of seconds, not in a minute.
	handle.setConnectTimeout(10);
}

/* An error a user can read. A refused request is answered with plain JSON rather than an event
stream, so the SSE reader has collected the whole body as stray text by the time this runs. */
static String describeHttpError(long code, StringView body) {
	const Value value = data::read<mem_std::Interface>(body);
	auto &message = value.getValue("error").getValue("message");
	if (message.isString() && !message.getString().empty()) {
		return toString("HTTP ", code, ": ", message.getString());
	}

	auto text = body;
	text.trimChars<StringView::WhiteSpace>();
	if (text.empty()) {
		return toString("HTTP ", code);
	}
	if (text.size() > s_maxErrorTextSize) {
		text = StringView(text.data(), s_maxErrorTextSize);
	}
	return toString("HTTP ", code, ": ", text);
}

} // namespace

bool AgentClient::init(NotNull<AppThread> app) {
	_app = app;
	return true;
}

void AgentClient::fetchModels(Ref *owner, ModelsCallback &&callback) {
	auto url = makeAgentUrl("models");
	auto request = Rc<ModelsRequest>::alloc();

	/* A plain request and response: no streaming, no cancelling, a body small enough to hold whole.
	It doubles as the app's connectivity check - if this fails, nothing else is worth trying. */
	_app->perform(
			[url, request](const AppThread::Task &) -> bool {
		mem_std::NetworkHandle handle;
		if (!handle.init(network::Method::Get, url)) {
			request->error = toString("cannot address ", url);
			return false;
		}

		setupCommonRequest(handle);
		handle.setReceiveCallback([request](char *data, size_t size) -> size_t {
			request->body.append(data, size);
			return size;
		});

		if (!handle.perform()) {
			auto text = handle.getError();
			request->error = text.empty() ? String("request failed") : text.str<Interface>();
			return false;
		}

		auto code = handle.getResponseCode();
		if (code < 200 || code >= 300) {
			request->error = describeHttpError(code, request->body);
			return false;
		}

		request->result = data::read<mem_std::Interface>(request->body);
		return true;
	},
			[request, callback = sp::move(callback)](const AppThread::Task &,
					bool success) mutable {
		if (!success) {
			callback(Status::ErrorNotPermitted, Vector<String>(), sp::move(request->error));
			return;
		}

		/* Two shapes come back from the same endpoint: the OpenAI `data[]` array and, on llama.cpp,
		an Ollama-flavoured `models[]` beside it. `data[].id` is the one every implementation
		agrees on. */
		Vector<String> models;

		// Const, so a body that is missing `data` reads as empty rather than asserting.
		const Value &parsed = request->result;
		const Value &list = parsed.getValue("data");
		if (list.isArray()) {
			for (auto &it : list.asArray()) {
				auto &id = it.getString("id");
				if (!id.empty()) {
					models.emplace_back(id);
				}
			}
		}

		if (models.empty()) {
			callback(Status::ErrorInvalidArguemnt, sp::move(models),
					String("the endpoint listed no models"));
			return;
		}

		callback(Status::Ok, sp::move(models), String());
	},
			owner);
}

Rc<StreamSession> AgentClient::startCompletion(Ref *owner, StringView model, Value &&request) {
	auto session = Rc<StreamSession>::alloc();
	session->model = model.str<Interface>();

	auto url = makeAgentUrl("chat/completions");

	/* JSON, said out loud. setSendData(const Value &) takes an EncodeFormat whose default is CBOR,
	which an OpenAI-compatible endpoint answers with a 400 that says nothing useful. Encoding here
	also keeps the worker's job to bytes in, bytes out. */
	auto body = data::toString<mem_std::Interface>(request);

	auto app = _app;

	_app->perform(
			[session, url, body, app](const AppThread::Task &) -> bool {
		mem_std::NetworkHandle handle;
		if (!handle.init(network::Method::Post, url)) {
			session->errorText = toString("cannot address ", url);
			return false;
		}

		setupCommonRequest(handle);
		handle.addHeader("Accept", "text/event-stream");
		handle.addHeader("Cache-Control", "no-cache");
		handle.setSendData(StringView(body), "application/json");

		/* THE ONE SETTING THIS WOULD NOT WORK WITHOUT. The defaults abort a transfer that stays
		under 10 KiB/s for 120 seconds, which is a fair description of a model thinking before its
		first token and of a long answer arriving one token at a time. */
		handle.setLowSpeedLimit(0, 0);

		SseParser parser;
		size_t events = 0;
		size_t jsonErrors = 0;
		size_t bytes = 0;

		// Deltas are batched per chunk rather than per token: one chunk usually carries several
		// events, and coalescing them is one thread hop instead of five, with no lock anywhere.
		String pendingText;
		String pendingReasoning;

		/* A call arrives in pieces, and only the FIRST piece carries the id and the function name -
		everything after it is another fragment of the argument string. The index is what ties the
		pieces together, so the calls are assembled into a slot per index rather than appended. */
		Vector<ChatToolCall> toolCalls;

		auto flush = [&] {
			if (pendingText.empty() && pendingReasoning.empty()) {
				return;
			}
			app->performOnAppThread(
					[session, text = sp::move(pendingText),
							reasoning = sp::move(pendingReasoning)]() mutable {
				if (session->cancelled.load() || !session->onDelta) {
					return;
				}
				// Thinking first: it is what the model produced first.
				if (!reasoning.empty()) {
					session->onDelta(reasoning, true);
				}
				if (!text.empty()) {
					session->onDelta(text, false);
				}
			},
					session);
			pendingText.clear();
			pendingReasoning.clear();
		};

		/* A NAMED functor, because a Callback owns nothing: built from a temporary lambda it would
		dangle at the end of this declaration, and no compiler says a word about it. */
		auto sink = [&](StringView, StringView payload) {
			// The reader knows nothing about OpenAI; this is where the protocol starts.
			if (payload == "[DONE]") {
				return;
			}

			++events;

			const Value value = data::json::read<mem_std::Interface>(payload);
			if (!value.isDictionary()) {
				// One malformed event is not a reason to drop a conversation. The count is
				// reported through the inspector, so it is visible rather than merely survivable.
				++jsonErrors;
				return;
			}

			const Value &choices = value.getValue("choices");
			if (!choices.isArray() || choices.size() == 0) {
				return;
			}

			const Value &choice = choices.getValue(0);
			const Value &delta = choice.getValue("delta");

			/* Two fields, not one. A reasoning model behind llama.cpp streams its thinking as
			`reasoning_content` and the answer as `content`, and the first chunk carries a null
			`content` besides the role. Reading only `content` shows an empty bubble until the
			thinking is over. */
			auto &reasoning = delta.getString("reasoning_content");
			if (!reasoning.empty()) {
				pendingReasoning.append(reasoning);
			}

			auto &content = delta.getString("content");
			if (!content.empty()) {
				pendingText.append(content);
			}

			const Value &calls = delta.getValue("tool_calls");
			if (calls.isArray()) {
				for (auto &call : calls.asArray()) {
					auto index = size_t(call.getInteger("index"));
					if (toolCalls.size() <= index) {
						toolCalls.resize(index + 1);
					}

					auto &target = toolCalls[index];

					auto &id = call.getString("id");
					if (!id.empty()) {
						target.id = id;
					}

					const Value &function = call.getValue("function");

					auto &name = function.getString("name");
					if (!name.empty()) {
						target.name = name;
					}

					auto &arguments = function.getString("arguments");
					if (!arguments.empty()) {
						target.arguments.append(arguments);
					}
				}
			}

			auto &finish = choice.getString("finish_reason");
			if (!finish.empty()) {
				session->finishReason = finish;
			}

			// llama.cpp puts generation speed in the last chunk. Nothing else offers it, and its
			// absence is not an error.
			const Value &timings = value.getValue("timings");
			if (timings.isDictionary()) {
				session->tokensPerSecond = timings.getDouble("predicted_per_second");
			}
		};
		const SseParser::EventCallback callback(sink);

		handle.setReceiveCallback([&](char *data, size_t size) -> size_t {
			if (session->cancelled.load()) {
				// The only way out. Returning anything else than `size` is how curl is told the
				// write failed, and this constant is what says it was deliberate.
				return size_t(CURL_WRITEFUNC_ERROR);
			}

			bytes += size;
			parser.consume(StringView(data, size), callback);
			flush();
			return size;
		});

		auto performed = handle.perform();

		// Some servers close without the final blank line; the last event is still owed to us.
		parser.finish(callback);
		flush();

		session->responseCode = handle.getResponseCode();
		session->toolCalls = sp::move(toolCalls);
		session->events = events;
		session->jsonErrors = jsonErrors;
		session->bytes = bytes;

		if (session->cancelled.load()) {
			return false;
		}

		if (!performed) {
			auto text = handle.getError();
			session->errorText = text.empty() ? String("connection failed") : text.str<Interface>();
			return false;
		}

		if (session->responseCode < 200 || session->responseCode >= 300) {
			// Not an event stream at all - the reader kept the body aside for exactly this.
			session->errorText = describeHttpError(session->responseCode, parser.getStray());
			return false;
		}

		return true;
	},
			[session](const AppThread::Task &, bool success) {
		if (!session->onDone) {
			return;
		}

		// Taken, not called in place: the handler is free to start the next request.
		auto callback = sp::move(session->onDone);
		session->onDone = nullptr;

		if (session->cancelled.load()) {
			callback(Status::Declined, StringView());
			return;
		}

		callback(success ? Status::Ok : Status::ErrorNotPermitted, session->errorText);
	},
			owner);

	return session;
}

} // namespace stappler::xenolith::examples
