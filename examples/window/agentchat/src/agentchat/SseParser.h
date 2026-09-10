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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SSEPARSER_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SSEPARSER_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** An incremental Server-Sent Events reader.

WHY IT IS ITS OWN CLASS. The bytes arrive from curl in chunks of whatever size the network felt
like, and an SSE event boundary has nothing to do with a chunk boundary: one chunk can carry three
events, and an event can be split down the middle of `data: [DON` / `E]`. A parser that keeps its
half-finished line in a LOCAL variable therefore loses text at every chunk edge, silently and only
under load. The tail lives in _buffer, which is why this is an object and not a function.

Knowing nothing about OpenAI is the other half of its job: it hands out `event` and `data` verbatim,
and the caller is what decides that `[DONE]` ends a stream and that everything else is JSON. That
also makes it testable with no socket at all - see runSseSelfTest().

WHAT IS NOT AN EVENT. A server that answers a request with an error does not answer in SSE: it sends
`{"error":{...}}` as plain JSON, with the same 200-less status code the transport reports later. Any
line that is not a `field: value` pair and not a comment therefore goes to _stray, and the caller
reads it when the response code turns out not to be a success. Fishing the error text out of the
body needs no extra branch anywhere else. */
class SseParser {
public:
	// event name (empty when the server sent none) and the event's data with its line breaks kept
	using EventCallback = Callback<void(StringView event, StringView data)>;

	// Feed one chunk. Complete events are dispatched, an unfinished tail is kept for the next call.
	void consume(StringView chunk, const EventCallback &);

	/* Dispatch what is left when the connection closed without the final blank line. Some servers
	do that, and the last event would otherwise be dropped. */
	void finish(const EventCallback &);

	// Everything that did not look like SSE, in arrival order. Usually empty; an error body when not.
	StringView getStray() const { return StringView(_stray); }

	void reset();

private:
	void handleLine(StringView line, const EventCallback &);
	void dispatch(const EventCallback &);

	// Raw tail. _pos is how much of it is already parsed - the buffer is compacted rather than
	// erased line by line, which would be quadratic over a long answer.
	String _buffer;
	size_t _pos = 0;

	String _event;
	String _data;
	String _stray;
	bool _hasData = false;
};

/** Runs the parser against a fixed script cut into chunks at every possible offset, and returns
{checks, failures}. Needs no network, no window and no model, which is what makes the streaming half
of this example verifiable on a machine that has no agent to talk to. */
Pair<size_t, size_t> runSseSelfTest();

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SSEPARSER_H_
