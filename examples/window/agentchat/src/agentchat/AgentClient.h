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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCLIENT_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCLIENT_H_

#include "agentchat/ChatHistory.h"
#include "XLCommon.h"
#include "XLAppThread.h"

#include <sprt/cxx/atomic>

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** One answer being streamed, and the only object both threads touch.

THE THREADING CONTRACT, because everything about this class depends on it:

  * `cancelled` is written by the app thread and read by the worker on every chunk. It is the one
    field that is genuinely concurrent, and it is atomic for exactly that reason;
  * `onDelta` and `onDone` are installed, called and cleared on the APP THREAD only. The worker
    never reads them - it posts a task that does;
  * the counters and the result fields are written by the worker BEFORE it returns, and read by the
    app thread AFTER the completion callback starts. Handing the task back is what orders those two,
    so no lock is needed and no lock would help.

A session is kept alive by the task that runs it, so it outlives a layout that goes away mid-answer.
That layout clears the callbacks in handleExit, and what is left calls into nothing. */
class StreamSession : public Ref {
public:
	// Set by the app thread, polled by the worker. The transfer stops on the next chunk.
	sprt::atomic<bool> cancelled = false;

	// App thread only. `reasoning` marks the model's thinking, which is displayed apart.
	Function<void(StringView text, bool reasoning)> onDelta;

	// App thread only. Status::Ok means the stream ended the way it should have.
	Function<void(Status, StringView error)> onDone;

	String model;

	// Written by the worker before it returns; read on the app thread afterwards.
	long responseCode = 0;

	/* What the model asked to run, assembled from the stream. A turn that ends in calls carries
	`finishReason == "tool_calls"`; the caller executes them and sends the results back. */
	Vector<ChatToolCall> toolCalls;
	String errorText;
	String finishReason;
	double tokensPerSecond = 0.0;
	size_t events = 0;
	size_t jsonErrors = 0;
	size_t bytes = 0;
};

/** The HTTP side of the example: an OpenAI-compatible endpoint, reached with the curl-backed
network::Handle from stappler_network.

Both calls are made from the app thread and return at once - the blocking work happens on a worker
from AppThread::perform, and every result comes back through a callback on the app thread. */
class AgentClient : public Ref {
public:
	using ModelsCallback = Function<void(Status, Vector<String> &&models, String &&error)>;

	virtual ~AgentClient() = default;

	virtual bool init(NotNull<AppThread> app);

	// GET {endpoint}/models. `owner` keeps the caller alive until the callback runs.
	void fetchModels(Ref *owner, ModelsCallback &&);

	/* POST {endpoint}/chat/completions with "stream": true. The session comes back before anything
	is sent, so the caller installs its callbacks on it and can cancel it immediately. */
	Rc<StreamSession> startCompletion(Ref *owner, StringView model, Value &&request);

protected:
	Rc<AppThread> _app;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCLIENT_H_
