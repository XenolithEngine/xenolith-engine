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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_DROP_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_DROP_H_

#include <sprt/runtime/ref.h>
#include <sprt/cxx/atomic>
#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/input.h>
#include <sprt/cxx/function>

namespace sprt::window {

// What a drop does with the payload; maps onto XdndAction*, DROPEFFECT_*, NSDragOperation* and
// wl_data_device_manager_dnd_action. A mask means "any of these"; a resolved action is one bit.
enum class DragActions : uint32_t {
	None = 0,

	Copy = 1 << 0,
	Move = 1 << 1,
	Link = 1 << 2,

	All = Copy | Move | Link,
};

SPRT_DEFINE_ENUM_AS_MASK(DragActions)

enum class DropPhase : uint32_t {
	Enter,
	Motion,
	Leave,
	Drop,
};

/** A drag from another application, from the moment it enters a window until the drop is done.

The backend creates one per OS drag and reports it with DropEvent. `read`, `status` and `finish` may
be called from any thread; they run on the context thread, where the backend implements the
matching `handle*` method. The type list and the allowed actions are fixed at creation.

`finish` is valid only after the drop (`setDropped`) and runs at most once. A dropped offer nobody
finishes is finished with DragActions::None after FinishTimeout, so a reader that never returns
does not keep the source waiting. */
class SPRT_API DropOffer : public Ref {
public:
	// The callback runs exactly once, on the context thread; the bytes are borrowed for the call
	using ReadCallback = Function<void(Status, BytesView)>;

	static constexpr dispatch::TimeInterval FinishTimeout = dispatch::TimeInterval::seconds(30);

	virtual ~DropOffer();

	virtual bool init(NotNull<dispatch::Looper>, Vector<String> &&types, DragActions allowed);

	// MIME types in the source's order of preference
	SpanView<String> getTypes() const { return SpanView<String>(_types.data(), _types.size()); }
	bool hasType(StringView) const;

	DragActions getAllowedActions() const { return _allowed; }

	// Read the bytes of one type. A type not on offer, or a finished offer, answers an error
	void read(StringView type, ReadCallback &&);

	// What a drop at the last reported position would perform; None refuses it
	void status(DragActions);

	// The drop is done; `performed` is the action applied, None if nothing was
	void finish(DragActions performed);

	// Turn down one step: a status of None, or a finish with None for a drop
	void refuse(DropPhase);

	// Context thread. NativeWindow::handleDropEvent marks the offer when it reports the drop
	void setDropped();

	// Context thread
	bool isDropped() const { return _dropped; }
	bool isFinished() const { return _finished; }

	// Context thread. The last status the application sent: the answer for a platform that asks
	// synchronously (IDropTarget::DragOver, draggingUpdated:)
	DragActions getStatus() const { return _status; }

protected:
	virtual void handleRead(StringView type, ReadCallback &&) = 0;
	virtual void handleStatus(DragActions) { }
	virtual void handleFinish(DragActions) { }

	void performFinish(DragActions);

	Rc<dispatch::Looper> _looper;
	Vector<String> _types;
	DragActions _allowed = DragActions::None;
	DragActions _status = DragActions::None;
	Rc<dispatch::Handle> _timeout;
	bool _dropped = false;
	bool _finished = false;
};

/** An offer with every representation known in advance.

Serves a synthetic drop (a test harness, the inspector) and a backend that has to copy the data
out while the OS still holds it. Reports what the application answered through atomics readable
from any thread. */
class SPRT_API MemoryDropOffer : public DropOffer {
public:
	struct Representation {
		String type;
		Bytes data;
	};

	virtual ~MemoryDropOffer() = default;

	using DropOffer::init;

	virtual bool init(NotNull<dispatch::Looper>, Vector<Representation> &&, DragActions allowed);

	DragActions getLastStatus() const { return DragActions(_lastStatus.load()); }
	DragActions getPerformed() const { return DragActions(_performed.load()); }
	bool hasFinished() const { return _finishedFlag.load(); }
	uint32_t getReadCount() const { return _reads.load(); }

protected:
	virtual void handleRead(StringView type, ReadCallback &&) override;
	virtual void handleStatus(DragActions) override;
	virtual void handleFinish(DragActions) override;

	Vector<Representation> _data;
	sprt::atomic<uint32_t> _lastStatus = 0;
	sprt::atomic<uint32_t> _performed = 0;
	sprt::atomic<bool> _finishedFlag = false;
	sprt::atomic<uint32_t> _reads = 0;
};

// A text/uri-list of `file:` URIs for local paths in the runtime's form, CRLF after each line.
// The inverse is stappler::UrlView::readUriList with readFilePath
SPRT_API String makeFileUriList(SpanView<StringView> paths);

// One step of an OS drag over a window.
struct DropEvent {
	DropPhase phase = DropPhase::Motion;
	Rc<DropOffer> offer;

	// Window surface pixels, bottom-left origin: the space of InputEventData
	Vec2 location;

	// The single action the OS asks for (modifiers, or the compositor's choice); None when it
	// states no preference
	DragActions preferred = DragActions::None;

	InputModifier modifiers = InputModifier::None;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_DROP_H_
