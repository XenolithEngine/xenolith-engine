/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CLIPBOARD_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CLIPBOARD_H_

#include <sprt/runtime/window/types.h>
#include <sprt/runtime/platform.h>

namespace sprt::window {

struct ClipboardRequest : public Ref {
	// receives data from clipboard
	Function<void(Status, BytesView, StringView)> dataCallback;

	// Select one of suggested types to receive
	// You should not assume on what thread this function would be called,
	// so, assume the worst case
	Function<StringView(SpanView<StringView>)> typeCallback;

	// Target to keep
	Rc<Ref> target;
};

struct ClipboardData : public Ref {
	time_t initial = platform::clock(platform::ClockType::Realtime);

	// User-friendly description
	String label;

	// Supported types for the data
	Vector<String> types;

	// Function to convert clipboard's data into target format
	// You should not assume on what thread this function would be called,
	// so, assume the worst case
	Function<Bytes(StringView)> encodeCallback;

	// Data owner
	Rc<Ref> owner;
};

struct ClipboardProbe : public Ref {
	// Function to receive available types
	Function<void(Status, SpanView<StringView>)> typeCallback;

	// Target to keep
	Rc<Ref> target;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CLIPBOARD_H_
