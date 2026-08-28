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

#include "XLClipboard.h"
#include "XLAppThread.h"

/* WHAT EACH PLATFORM ACTUALLY DOES, because the guarantees above are written against this table and
not against the API's prose. Established by reading the backends, not by testing them:

  headless   read/probe/write, in process. Both callbacks fire INSIDE the call. Last write wins.
  linux xcb  read is asynchronous (a TARGETS round trip, then a second conversion, INCR for large
             payloads); UTF8_STRING/STRING are aliased to text/plain in both directions.
  wayland    the selector runs synchronously, and the type it returns must be an EXACT member of
             what was offered - otherwise the request is dropped WITHOUT calling dataCallback.
  windows    no read, no probe; write returns Ok having discarded the payload. The WinRT
             implementation exists in the source, commented out.
  macos      lazy, multi-representation write; read requires an exact match but reports the refusal.
             No probe.
  android    multi-representation through a ContentProvider; may deliver from a worker thread. An
             EMPTY type list means CLEAR THE CLIPBOARD - which is why write() refuses one here.
  base       read calls back AND returns a failure; probe returns a failure and never calls back.

Two of those rows are why "exactly one answer" needs both halves: wayland can answer zero times and
the base controller can answer twice. */

namespace STAPPLER_VERSIONIZED stappler::xenolith {

StringView preferMimeType(SpanView<StringView> available, SpanView<StringView> preference) {
	for (auto &want : preference) {
		for (auto &it : available) {
			if (it.starts_with(want)) {
				return it;
			}
		}
	}
	return StringView();
}

// ---- ClipboardOffer ------------------------------------------------------------------------------

ClipboardOffer &ClipboardOffer::setLabel(StringView value) {
	label = value.str<Interface>();
	return *this;
}

ClipboardOffer &ClipboardOffer::addRepresentation(StringView type, BytesView data) {
	auto bytes = sprt::window::Bytes(data.data(), data.data() + data.size());

	for (auto &it : _eager) {
		if (StringView(it.type) == type) {
			// Position is preference, so a repeated type keeps its place rather than moving to the
			// end of the list
			it.data = sp::move(bytes);
			return *this;
		}
	}

	_eager.emplace_back(Representation{type.str<sprt::window::String>(), sp::move(bytes)});
	types.emplace_back(type.str<Interface>());
	return *this;
}

ClipboardOffer &ClipboardOffer::addText(StringView utf8, StringView type) {
	return addRepresentation(type,
			BytesView(reinterpret_cast<const uint8_t *>(utf8.data()), utf8.size()));
}

ClipboardOffer &ClipboardOffer::setEncoder(SpanView<StringView> t,
		Function<sprt::window::Bytes(StringView)> &&cb) {
	for (auto &it : t) {
		bool known = false;
		for (auto &existing : types) {
			if (StringView(existing) == it) {
				known = true;
				break;
			}
		}
		if (!known) {
			types.emplace_back(it.str<Interface>());
		}
	}
	encode = sp::move(cb);
	return *this;
}

Rc<sprt::window::ClipboardData> ClipboardOffer::takeClipboardData(Ref *owner) {
	auto data = Rc<sprt::window::ClipboardData>::create();
	data->label = StringView(label).str<sprt::window::String>();
	for (auto &it : types) { data->types.emplace_back(StringView(it).str<sprt::window::String>()); }

	// The eager table travels INTO the callback rather than beside it: the platform may ask on any
	// thread, at any later time, and a lambda owning its copy is the only arrangement that does not
	// depend on this offer still existing
	if (!_eager.empty()) {
		data->encodeCallback = [eager = sp::move(_eager), chained = sp::move(encode)](
									   StringView type) -> sprt::window::Bytes {
			for (auto &it : eager) {
				if (StringView(it.type) == type) {
					return it.data;
				}
			}
			return chained ? chained(type) : sprt::window::Bytes();
		};
	} else if (encode) {
		data->encodeCallback = sp::move(encode);
	}

	data->owner = owner;
	return data;
}

// ---- the read in flight --------------------------------------------------------------------------

/* Split off from the session because the type selector runs on an OS thread and must not be able to
reach a session that may already be gone. The session holds one of these at a time; the callbacks
hold it too, so it outlives a cancel. */
struct ClipboardSession::Pending : public Ref {
	uint64_t serial = 0;

	// Read by the selector on an unknown thread and never mutated after read() hands it over
	sprt::window::Vector<sprt::window::String> preference;

	// Written by the selector on an unknown thread, read by the delivery on the app thread. The
	// flag is the fence: released after the list is filled, acquired before it is read.
	sprt::atomic<bool> selectorRan = false;
	sprt::window::Vector<sprt::window::String> available;

	// App thread only. Both the backend's answer and ServerAppThread's "it never started" land
	// here, and the base controller produces BOTH - so the first one to arrive takes the claim and
	// the second is dropped.
	bool claimed = false;
	bool cancelled = false;

	Rc<Ref> target;
	ReadCallback callback;
};

// ---- ClipboardSession ----------------------------------------------------------------------------

ClipboardSession::Pending *ClipboardSession::pending() const {
	return static_cast<Pending *>(_pending.get());
}

ClipboardSession::~ClipboardSession() { cancel(); }

bool ClipboardSession::init(NotNull<AppThread> app) {
	_app = app.get();
	return true;
}

bool ClipboardSession::isAvailable() const { return _app && _app->hasClipboard(); }

uint64_t ClipboardSession::read(SpanView<StringView> preference, ReadCallback &&cb, Ref *target) {
	if (!_app || !cb || preference.empty()) {
		return 0;
	}

	// A read that supersedes another drops it here rather than letting two answers race for the
	// same widget
	cancel();

	auto pending = Rc<Pending>::alloc();
	pending->serial = ++_serial;
	pending->callback = sp::move(cb);
	pending->target = target ? target : this;
	for (auto &it : preference) {
		pending->preference.emplace_back(it.str<sprt::window::String>());
	}

	_pending = pending;

	// Delivery is app-thread-only, so the claim needs no atomic: ServerAppThread hops the backend's
	// answer here, and its own "the read never started" answer is posted here too
	auto deliver = [pending](Status st, BytesView data, StringView type) {
		if (pending->claimed || pending->cancelled) {
			return;
		}
		pending->claimed = true;

		Result result;
		result.status = st;
		result.data = data;
		result.type = type;

		Vector<StringView> availableViews;
		if (pending->selectorRan.load(sprt::memory_order_acquire)) {
			availableViews.reserve(pending->available.size());
			for (auto &it : pending->available) { availableViews.emplace_back(StringView(it)); }
			result.available = availableViews;
		}

		// A backend that ignored the selection is refused rather than parsed: what came back has to
		// be something the caller asked for
		if (result.ok() && !type.empty()) {
			Vector<StringView> want;
			want.reserve(pending->preference.size());
			for (auto &it : pending->preference) { want.emplace_back(StringView(it)); }
			auto single = makeSpanView(&type, 1);
			if (preferMimeType(single, want).empty()) {
				result.status = Status::ErrorInvalidArguemnt;
				result.data = BytesView();
				result.type = StringView();
			}
		}

		auto callback = sp::move(pending->callback);
		pending->callback = nullptr;
		if (callback) {
			callback(result);
		}
		pending->target = nullptr;
	};

	// Runs on an UNKNOWN thread: it may look at strings and at this object, and at nothing else
	auto select = [pending](SpanView<StringView> available) -> StringView {
		for (auto &it : available) {
			pending->available.emplace_back(it.str<sprt::window::String>());
		}

		Vector<StringView> want;
		want.reserve(pending->preference.size());
		for (auto &it : pending->preference) { want.emplace_back(StringView(it)); }

		auto chosen = preferMimeType(available, want);

		// Published after the list is complete, so a delivery that arrives inline (headless, macOS,
		// the Android text path) sees either all of it or none
		pending->selectorRan.store(true, sprt::memory_order_release);

		// The view points into `available`, which is what the platform handed us - see
		// preferMimeType. Returning a string of our own is what wayland answers with silence
		return chosen;
	};

	const auto serial = pending->serial;
	_app->readFromClipboard(sp::move(deliver), sp::move(select), pending);

	// The read may already be over: headless, macOS and the Android text path answer inside the
	// call above. Reading _pending rather than `pending` is deliberate - a superseding read from
	// inside the callback has to win
	if (this->pending() == pending && pending->claimed) {
		_pending = nullptr;
	}
	return serial;
}

uint64_t ClipboardSession::readText(ReadCallback &&cb, Ref *target) {
	auto want = StringView("text/plain");
	return read(makeSpanView(&want, 1), sp::move(cb), target);
}

void ClipboardSession::cancel() {
	if (!_pending) {
		return;
	}

	// Held across the reset: the backend's lambdas may already be gone, which would make the
	// session's own reference the last one
	Rc<Ref> held = _pending;
	auto p = static_cast<Pending *>(held.get());
	_pending = nullptr;

	p->cancelled = true;
	p->callback = nullptr;
	p->target = nullptr;
}

void ClipboardSession::probe(ProbeCallback &&cb, Ref *target) {
	if (!_app || !cb) {
		return;
	}
	_app->probeClipboard(sp::move(cb), target ? target : this);
}

Status ClipboardSession::write(ClipboardOffer &&offer, Ref *owner) {
	if (!_app) {
		return Status::ErrorInvalidArguemnt;
	}

	// Refused rather than sent: on Android an empty type list means "clear the clipboard", so
	// forwarding it would destroy the user's clipboard on one platform and do nothing on the rest
	if (offer.empty()) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!_app->hasClipboard()) {
		// Reported rather than pretended: a transport that discards writes must not answer Ok, or
		// a caller can never find out why nothing was pasted
		return Status::ErrorNotImplemented;
	}

	_app->writeToClipboard(offer.takeClipboardData(owner ? owner : this));
	return Status::Ok;
}

Status ClipboardSession::writeText(StringView utf8, StringView label) {
	ClipboardOffer offer;
	offer.setLabel(label).addText(utf8);
	return write(sp::move(offer));
}

} // namespace stappler::xenolith
