/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef XENOLITH_APPLICATION_XLCLIPBOARD_H_
#define XENOLITH_APPLICATION_XLCLIPBOARD_H_

#include "XLCommon.h" // IWYU pragma: keep

#include <sprt/runtime/window/clipboard.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;

// MIME preference rule: matching is by prefix, and the first entry of `preference` that matches
// wins (so "text/plain" accepts "text/plain;charset=utf-8"). A free function, since a paste type
// selector has only a list of strings, on an unknown thread. The result points into `available`:
// the platform must get back one of its own strings (Wayland compares by identity).
SP_PUBLIC StringView preferMimeType(SpanView<StringView> available,
		SpanView<StringView> preference);

/* One payload in all its representations: the payload half of DragOffer, so copy and drag sources
describe themselves the same way; both produce `sprt::window::ClipboardData`.

Order is preference: put the specific type first and `text/plain` last.

`addRepresentation` copies bytes that exist now. `setEncoder` produces bytes on demand; the
callback may run much later on an unknown thread, so it must capture copies and never touch the
scene graph. */
struct SP_PUBLIC ClipboardOffer {
	String label;
	Vector<String> types;
	Function<sprt::window::Bytes(StringView)> encode;

	ClipboardOffer &setLabel(StringView);

	// One representation whose bytes are known now. Call once per type; a repeated type replaces
	// the bytes and keeps its position, because position is preference.
	ClipboardOffer &addRepresentation(StringView type, BytesView data);
	ClipboardOffer &addText(StringView utf8, StringView type = StringView("text/plain"));

	// The lazy half: types declared now, bytes on demand. Types already carrying eager bytes are
	// served from those; the callback is asked for the rest.
	ClipboardOffer &setEncoder(SpanView<StringView> types,
			Function<sprt::window::Bytes(StringView)> &&);

	bool empty() const { return types.empty(); }

	// Builds the object the clipboard or an OS drag takes, moving `encode` out of this offer.
	// `owner` keeps the encoder's captures alive while the platform holds the data.
	Rc<sprt::window::ClipboardData> takeClipboardData(Ref *owner = nullptr);

protected:
	struct Representation {
		sprt::window::String type;
		sprt::window::Bytes data;
	};

	// malloc-backed: it travels into the encode callback, to whatever thread the platform uses
	sprt::window::Vector<Representation> _eager;
};

/* One consumer's typed exchange with the system clipboard:

1. Exactly one answer, on the app thread; the first answer wins, even when the transport answers
   twice or not at all.
2. A staleness serial: every read supersedes the previous one, and cancel() drops the answer in
   flight.
3. Type negotiation from a preference list, with the chosen type taken from what the platform
   offered.
4. The answer reports what arrived, and a refusal reports what was available.

It carries no policy (e.g. masked fields not copying), and write() is not a receipt. */
class SP_PUBLIC ClipboardSession : public Ref {
public:
	// One answer. Every view in here is borrowed for the duration of the call; copy what must
	// outlive it.
	struct Result {
		Status status = Status::Declined;

		// The representation that actually arrived: one of the caller's preferences, resolved by
		// prefix. Empty when nothing was taken.
		StringView type;
		BytesView data;

		// What the clipboard held, when the refusal was ours because nothing matched.
		SpanView<StringView> available;

		StringView text() const {
			return StringView(reinterpret_cast<const char *>(data.data()), data.size());
		}

		// sprt::status:: spelled out: the `status` member would shadow the namespace.
		bool ok() const { return sprt::status::isSuccessful(status); }
		explicit operator bool() const { return ok(); }
	};

	using ReadCallback = Function<void(const Result &)>;
	using ProbeCallback = Function<void(Status, SpanView<StringView>)>;

	virtual ~ClipboardSession();

	virtual bool init(NotNull<AppThread>);

	// Read the first of `preference` the clipboard can produce, superseding any read in flight (its
	// answer is dropped). The callback runs exactly once on the app thread, unless cancel() or
	// destruction intervenes. `target` is retained until then (defaults to the session).
	// Returns the serial of the read, or 0 if it could not be started.
	uint64_t read(SpanView<StringView> preference, ReadCallback &&, Ref *target = nullptr);
	uint64_t readText(ReadCallback &&, Ref *target = nullptr);

	// Drop the answer in flight and release its target (e.g. on blur or document close).
	void cancel();

	bool isPending() const { return _pending != nullptr; }
	uint64_t getSerial() const { return _serial; }

	// What the clipboard can produce now. Answers exactly once on the app thread, with
	// ErrorNotImplemented on platforms without a probe.
	void probe(ProbeCallback &&, Ref *target = nullptr);

	// Put one payload, with all its representations, on the clipboard. Ok means the offer reached
	// the transport, not a receipt. An offer with no types is refused: Android treats it as
	// clearing the clipboard.
	Status write(ClipboardOffer &&, Ref *owner = nullptr);
	Status writeText(StringView utf8, StringView label = StringView());

	// Whether this process can reach a clipboard at all. False on a remote client, where a write is
	// silently discarded.
	bool isAvailable() const;

protected:
	struct Pending;

	// Held as Rc<Ref>: Pending is defined in the .cc and an Rc of an incomplete type can not be
	// destroyed elsewhere. pending() re-types it.
	Pending *pending() const;

	Rc<AppThread> _app;
	Rc<Ref> _pending;
	uint64_t _serial = 0;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLCLIPBOARD_H_
