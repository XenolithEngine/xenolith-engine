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

// The single place the MIME preference rule lives: matching is by PREFIX, and the first entry of
// `preference` that matches anything wins. That is what makes a caller asking for "text/plain" also
// accept "text/plain;charset=utf-8", which is what half the world actually puts on a clipboard.
//
// It is a free function because both sides of the boundary need it and only one of them has a
// DragData: a paste's type selector is handed a bare list of strings, on an unknown thread.
//
// The returned view points INTO `available`, and that is load-bearing rather than incidental: a
// selector must hand the platform back one of the strings the platform offered, not a string it
// spelled itself. Wayland compares by identity and answers a near-miss with silence.
SP_PUBLIC StringView preferMimeType(SpanView<StringView> available,
		SpanView<StringView> preference);

/* ONE PAYLOAD, IN EVERY REPRESENTATION IT HAS.

The payload half of DragOffer, standing on its own so that a source which can be copied and a source
which can be dragged describe themselves the same way. What reaches the OS is the same
`sprt::window::ClipboardData` in both cases.

ORDER IS PREFERENCE. `types` is what the platform advertises and what a reader negotiates against,
so the specific type goes first and `text/plain` last: a foreign application asking only for text
still gets something readable, and a peer asking for the specific one never has to parse the
fallback.

TWO WAYS TO SUPPLY BYTES, and they compose. `addRepresentation` is for bytes that already exist -
they are COPIED into the offer, so the caller's buffer may die immediately. `setEncoder` is for
bytes worth producing only if someone asks: the callback may run minutes later, in another process's
paste, ON AN UNKNOWN THREAD - so it must capture copies, never a scene node, and never touch the
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

	// Builds the object the clipboard takes and an OS drag will carry, MOVING `encode` out of this
	// offer. `owner` is what keeps the encoder's captures alive for as long as the platform holds
	// the data.
	Rc<sprt::window::ClipboardData> takeClipboardData(Ref *owner = nullptr);

protected:
	struct Representation {
		sprt::window::String type;
		sprt::window::Bytes data;
	};

	// malloc-backed, because it travels into the encode callback and from there to whatever thread
	// the platform asks on
	sprt::window::Vector<Representation> _eager;
};

/* ONE CONSUMER'S TYPED EXCHANGE WITH THE SYSTEM CLIPBOARD.

What this buys over calling AppThread directly, in the order the reasons were found:

1. EXACTLY ONE ANSWER, on the app thread. The transport underneath is neither exactly-once nor
   guaranteed: wayland answers a type it did not offer with SILENCE, the base controller calls back
   AND returns a failure, and Windows has no read at all. Half of the fix is in ServerAppThread,
   which now reports a start that failed; the other half is here, where the first answer wins and
   the rest are dropped. Neither half is sufficient alone.

2. THE STALENESS SERIAL, which used to be a field in each widget. Every read supersedes the one
   before it on this session, and cancel() drops the answer to what is in flight - which is what a
   widget losing focus needs, and what no widget actually did.

3. TYPE NEGOTIATION stated as a LIST and resolved by one rule, with the chosen type taken from what
   the platform offered rather than spelled by the caller. That makes wayland's silence unreachable
   instead of merely handled.

4. AN ANSWER THAT SAYS WHAT ARRIVED, and a refusal that says what WAS there - so a consumer can
   report "this is a graph fragment, not a component one" instead of guessing.

WHAT IT DOES NOT DO: policy. A masked field does not copy, and this seam never learns that rule - it
carries bytes, it does not decide who may. And write() is not a receipt: no platform gives one. */
class SP_PUBLIC ClipboardSession : public Ref {
public:
	// One answer. Every view in here is BORROWED for the duration of the call - copy what must
	// outlive it.
	struct Result {
		Status status = Status::Declined;

		// The representation that actually arrived: one of the caller's preferences, resolved by
		// prefix. Empty when nothing was taken.
		StringView type;
		BytesView data;

		// What the clipboard was holding, when the refusal was ours because nothing matched. This
		// is how a caller names the reason instead of reporting a bare failure.
		SpanView<StringView> available;

		StringView text() const {
			return StringView(reinterpret_cast<const char *>(data.data()), data.size());
		}

		// sprt::status:: spelled out: the member below is called `status` and would shadow the
		// namespace if it were not.
		bool ok() const { return sprt::status::isSuccessful(status); }
		explicit operator bool() const { return ok(); }
	};

	using ReadCallback = Function<void(const Result &)>;
	using ProbeCallback = Function<void(Status, SpanView<StringView>)>;

	virtual ~ClipboardSession();

	virtual bool init(NotNull<AppThread>);

	// Read the first of `preference` the clipboard can produce. Supersedes whatever this session
	// had in flight: that answer is dropped, not applied.
	//
	// The callback runs EXACTLY ONCE, on the app thread, unless cancel() or destruction intervenes
	// - in which case it does not run at all. `target` is retained until then, and is what makes a
	// callback capturing a raw `this` safe; it defaults to the session itself.
	//
	// Returns the serial of the read, or 0 if it could not be started.
	uint64_t read(SpanView<StringView> preference, ReadCallback &&, Ref *target = nullptr);
	uint64_t readText(ReadCallback &&, Ref *target = nullptr);

	// Drop the answer to whatever is in flight and release its target. THIS is the staleness serial
	// the widgets used to carry: a blur, a focus the platform revoked, a document closed under an
	// editor.
	void cancel();

	bool isPending() const { return _pending != nullptr; }
	uint64_t getSerial() const { return _serial; }

	// What the clipboard can produce right now. Answers exactly once on the app thread, INCLUDING
	// on the platforms whose probe is not implemented - they answer ErrorNotImplemented, which is
	// how a Paste item greys itself out honestly rather than by pretending.
	void probe(ProbeCallback &&, Ref *target = nullptr);

	// Put one payload, with all of its representations, on the clipboard.
	//
	// Ok means the offer reached the transport carrying at least one representation. It is NOT a
	// receipt - see the class comment. An offer with no types is REFUSED rather than sent: Android
	// reads empty types as "clear the clipboard" and everything else as a no-op, so sending one
	// would mean destroying the user's clipboard on one platform and doing nothing on the rest.
	Status write(ClipboardOffer &&, Ref *owner = nullptr);
	Status writeText(StringView utf8, StringView label = StringView());

	// Whether this process can reach a clipboard at all. False on a remote client, where a write
	// would be discarded in silence.
	bool isAvailable() const;

protected:
	struct Pending;

	// Held as Rc<Ref> rather than Rc<Pending>: the read in flight is this unit's business and stays
	// defined in the .cc, and an Rc of an incomplete type cannot be destroyed at a construction
	// site in another translation unit. Re-typed by pending() below, which is the only reader.
	Pending *pending() const;

	Rc<AppThread> _app;
	Rc<Ref> _pending;
	uint64_t _serial = 0;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLCLIPBOARD_H_
