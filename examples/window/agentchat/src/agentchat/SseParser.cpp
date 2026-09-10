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

#include "agentchat/SseParser.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

void SseParser::reset() {
	_buffer.clear();
	_pos = 0;
	_event.clear();
	_data.clear();
	_stray.clear();
	_hasData = false;
}

void SseParser::consume(StringView chunk, const EventCallback &cb) {
	if (chunk.empty()) {
		return;
	}

	_buffer.append(chunk.data(), chunk.size());

	while (_pos < _buffer.size()) {
		auto nl = _buffer.find('\n', _pos);
		if (nl == String::npos) {
			// An unfinished line. It stays in the buffer until the bytes that complete it arrive.
			// Cutting only at '\n' is also what keeps a split multi-byte character intact: no
			// continuation byte can be mistaken for the separator.
			break;
		}

		StringView line(_buffer.data() + _pos, nl - _pos);
		_pos = nl + 1;

		// The spec's separator is CRLF or LF; strip the CR that the first form leaves behind.
		while (!line.empty() && line.back() == '\r') {
			line = StringView(line.data(), line.size() - 1);
		}

		handleLine(line, cb);
	}

	// Compaction: erasing each line off the front instead would copy the remaining buffer once per
	// line, which is quadratic over a long answer.
	if (_pos > 0 && _pos * 2 > _buffer.size()) {
		_buffer.erase(0, _pos);
		_pos = 0;
	}
}

void SseParser::handleLine(StringView line, const EventCallback &cb) {
	if (line.empty()) {
		dispatch(cb);
		return;
	}

	if (line.front() == ':') {
		// A comment. Proxies send `: ping` to hold the connection open.
		return;
	}

	auto colon = line.find(':');

	/* A field, but only if it is one of the four the protocol defines. Anything else is not SSE at
	all, and that includes a line that merely CONTAINS a colon: an error body is
	`{"error":{"message":"..."}}`, which splits into a plausible-looking `{"error"` field and would
	otherwise be swallowed as an unknown one, leaving nothing for the caller to report. */
	StringView field;
	if (colon != maxOf<size_t>()) {
		field = StringView(line.data(), colon);
	}

	if (field != "data" && field != "event" && field != "id" && field != "retry") {
		_stray.append(line.data(), line.size());
		_stray.push_back('\n');
		return;
	}

	StringView value(line.data() + colon + 1, line.size() - colon - 1);

	// Exactly one leading space is part of the framing; a second one is part of the value.
	if (!value.empty() && value.front() == ' ') {
		value = StringView(value.data() + 1, value.size() - 1);
	}

	if (field == "data") {
		_data.append(value.data(), value.size());
		_data.push_back('\n');
		_hasData = true;
	} else if (field == "event") {
		_event.assign(value.data(), value.size());
	}
	// `id` and `retry` are framing this example has no use for.
}

void SseParser::dispatch(const EventCallback &cb) {
	if (!_hasData) {
		// A blank line with nothing before it separates nothing.
		_event.clear();
		return;
	}

	// Each data line contributed a '\n'; the last one is framing, not content.
	if (!_data.empty() && _data.back() == '\n') {
		_data.pop_back();
	}

	cb(StringView(_event), StringView(_data));

	_event.clear();
	_data.clear();
	_hasData = false;
}

void SseParser::finish(const EventCallback &cb) {
	// Whatever is left is one last line without its terminator.
	if (_pos < _buffer.size()) {
		StringView line(_buffer.data() + _pos, _buffer.size() - _pos);
		_pos = _buffer.size();
		while (!line.empty() && line.back() == '\r') {
			line = StringView(line.data(), line.size() - 1);
		}
		if (!line.empty()) {
			handleLine(line, cb);
		}
	}

	dispatch(cb);
}

namespace {

/* One scripted stream, exercising everything the reader has to survive: a comment, a bare data
event, a named event whose data spans two lines, the terminator OpenAI sends, and a final event with
no blank line after it (a server that just closes the socket). */
static constexpr auto s_selfTestStream = StringView(": ping\n"
												   "data: {\"a\":1}\n"
												   "\n"
												   "event: update\r\n"
												   "data: line one\r\n"
												   "data: line two\r\n"
												   "\r\n"
												   "data: [DONE]\n"
												   "\n"
												   "data: tail");

using EventList = Vector<Pair<String, String>>;

static EventList runScript(StringView input, size_t chunkSize) {
	SseParser parser;
	EventList events;

	/* The functor is a NAMED local. A Callback is a view of a functor and owns nothing, so one built
	from a temporary lambda dangles at the end of the declaration - and nothing diagnoses it. */
	auto sink = [&](StringView event, StringView data) {
		events.emplace_back(event.str<Interface>(), data.str<Interface>());
	};
	const SseParser::EventCallback cb(sink);

	for (size_t offset = 0; offset < input.size(); offset += chunkSize) {
		auto size = sprt::min(chunkSize, input.size() - offset);
		parser.consume(StringView(input.data() + offset, size), cb);
	}
	parser.finish(cb);

	return events;
}

} // namespace

Pair<size_t, size_t> runSseSelfTest() {
	size_t checks = 0;
	size_t failures = 0;

	auto expect = [&](bool condition, StringView message) {
		++checks;
		if (!condition) {
			++failures;
			log::source().error("AgentChatExample", "sse self-test FAILED: ", message);
		}
	};

	const EventList expected{
		Pair<String, String>(String(), String("{\"a\":1}")),
		Pair<String, String>(String("update"), String("line one\nline two")),
		Pair<String, String>(String(), String("[DONE]")),
		Pair<String, String>(String(), String("tail")),
	};

	/* Every chunk size from one byte upwards, so every possible split lands inside every possible
	construct: mid-field, mid-value, between CR and LF, inside `[DONE]`. If the reader kept its tail
	in a local, chunk size 1 alone would produce nothing at all. */
	for (size_t chunkSize = 1; chunkSize <= s_selfTestStream.size(); ++chunkSize) {
		auto events = runScript(s_selfTestStream, chunkSize);

		expect(events.size() == expected.size(),
				toString("event count at chunk size ", chunkSize, ": got ", events.size(),
						", expected ", expected.size()));

		auto count = sprt::min(events.size(), expected.size());
		for (size_t i = 0; i < count; ++i) {
			expect(events[i].first == expected[i].first,
					toString("event name ", i, " at chunk size ", chunkSize, ": got '",
							events[i].first, "'"));
			expect(events[i].second == expected[i].second,
					toString("event data ", i, " at chunk size ", chunkSize, ": got '",
							events[i].second, "'"));
		}
	}

	// An error body is not SSE: no event comes out of it, and all of it is readable afterwards.
	{
		static constexpr auto errorBody =
				StringView("{\"error\":{\"message\":\"model not found\",\"code\":404}}");

		SseParser parser;
		size_t dispatched = 0;
		auto sink = [&](StringView, StringView) { ++dispatched; };
		const SseParser::EventCallback cb(sink);

		parser.consume(errorBody, cb);
		parser.finish(cb);

		expect(dispatched == 0, "an error body must not produce events");
		expect(parser.getStray().starts_with("{\"error\""),
				toString("error body must be readable as stray, got '", parser.getStray(), "'"));
	}

	return Pair<size_t, size_t>(checks, failures);
}

} // namespace stappler::xenolith::examples
