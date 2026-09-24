/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef XENOLITH_APPLICATION_XLAPPLICATION_H_
#define XENOLITH_APPLICATION_XLAPPLICATION_H_

#include "XLContextInfo.h"
#include "XLEvent.h"
#include "XLRemotePeerInfo.h"
#include "XLRemoteProtocol.h"
#include "XLRemoteReplyTable.h"
#include "XLRemotePeer.h"
#include "XLResourceCache.h"
#include "XLScene.h"
#include "XLTemporaryResource.h" // IWYU pragma: keep
#include "XLApplicationExtension.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
class Loop;
} // namespace core

class Director;
class AppWindow;
class BlockTransferManager;

// The client font controller (xenolith_font, downstream) sends Domain::Font messages through this
// thread's remoteSend* facade; forward-declared only to befriend it.
namespace font {
class FontControllerRemote;
} // namespace font

// Base application thread: thread, extension and update machinery. It holds no Context reference;
// context-derived services are reached through the protected virtual hooks below. Concrete
// subclasses: ServerAppThread (owns a Context, windows, the listener) and ClientAppThread (owns a
// standalone ClientContext).
class SP_PUBLIC AppThread : public sprt::dispatch::Thread, public RemotePeer {
public:
	static EventHeader onNetworkState;
	static EventHeader onThemeInfo;

	using Task = sprt::dispatch::Task;

	using ExecuteCallback = Function<bool(const Task &)>;
	using CompleteCallback = Function<void(const Task &, bool)>;

	virtual ~AppThread();

	virtual void run();

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void stop() override;

	virtual void wakeup(Function<void()> &&fn = nullptr);

	virtual void handleNetworkStateChanged(NetworkFlags);
	virtual void handleThemeInfoChanged(const ThemeInfo &);
	virtual void handleMatrialsUpdated(NotNull<core::MaterialSet>);

	/* If current thread is main thread: executes function/task
	   If not: adds function/task to main thread queue */
	void performOnAppThread(Function<void()> &&func, Ref *target = nullptr,
			bool onNextFrame = false, StringView tag = SP_FUNC);

	/* If current thread is main thread: executes function/task
	   If not: adds function/task to main thread queue */
	void performOnAppThread(Rc<Task> &&task, bool onNextFrame = false);

	/* Performs action in this thread, task will be constructed in place */
	void perform(ExecuteCallback &&, CompleteCallback && = nullptr, Ref * = nullptr) const;

	/* Performs task in thread, identified by id */
	void perform(Rc<Task> &&task) const;

	/* Performs task in thread, identified by id */
	void perform(Rc<Task> &&task, bool performFirst) const;

	// Platform-services interface (clipboard / screen-info / URL). On the server these delegate to
	// the OS via the Context; on the client they are routed to the remote server (stubbed for now).

	// Whether this process can reach a system clipboard at all. False on a remote client, where the
	// calls below are safe but writes go nowhere. This is about the transport; platform clipboard
	// support is reported by WindowCapabilities.
	virtual bool hasClipboard() const { return true; }

	// Read data from OS clipboard
	//
	// - dataCallback will receive data with selected type in this thread
	// - selectCallback will be called in unknown OS thread and should select one of available data
	// types by return it, or return StringView() to discard request
	// - ref is preserved for all operation direction
	virtual void readFromClipboard(Function<void(Status, BytesView, StringView)> &&dataCallback,
			Function<StringView(SpanView<StringView>)> &&selectCallback, Ref *ref = nullptr) = 0;

	// Test, which data is available to read from clipboard (if any)
	//
	// - cb will receive a list of types, available to read in this thread
	// - ref is preserved for all operation direction
	virtual void probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb,
			Ref *ref = nullptr) = 0;

	// Provide static data for OS clipboard with specific type
	// - ref is preserved until clibpoard data remains actial for OS
	virtual void writeToClipboard(BytesView data, StringView contentType = StringView("text/plain"),
			Ref *ref = nullptr, StringView label = StringView()) = 0;

	// Provide data for OS clipboard via callback
	//
	// - dataCallback will be called in unknown OS thread to access actual data with specific type.
	// Callback is preserved until clibpoard data remains actial for OS
	// - types - list of types, that can be accessed with provided callback
	// - ref is preserved until clibpoard data remains actial for OS
	virtual void writeToClipboard(
			sprt::window::Function<sprt::window::Bytes(StringView)> &&dataCallback,
			SpanView<StringView> types, Ref *ref = nullptr, StringView label = StringView()) = 0;

	// Provide already-assembled clipboard data (the same object an OS drag carries).
	// `data->owner` keeps the encode callback's captures alive.
	virtual void writeToClipboard(Rc<sprt::window::ClipboardData> &&data) = 0;

	virtual void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref * = nullptr) = 0;

	virtual void openUrl(StringView) = 0;

	// Config source for the base thread machinery (looper/timer) and external consumers (network).
	virtual const ContextInfo *getContextInfo() const = 0;

	// Local GPU loop, server-only; nullptr on a client (no local rendering).
	virtual core::Loop *getGlLoop() const { return nullptr; }

	// Who owns the window this thread draws into: the OS, the window system and the gAPI. Locally
	// this process; on a client, the server. Null while unknown (client before ServerInfo, server
	// before its gAPI loop exists); callers should then keep their previous behaviour.
	virtual const remote::PeerInfo *getServerInfo() const { return nullptr; }

	sprt::dispatch::Looper *getLooper() const { return _appLooper; }

	NetworkFlags getNetworkFlags() const { return _networkFlags; }
	const ThemeInfo &getThemeInfo() const { return _themeInfo; }

	bool addListener(NotNull<Ref>, Function<void(const UpdateTime &, bool)> &&);
	bool removeListener(NotNull<Ref>);

	/* Send the font controller's pending glyph batch now, rather than on the next update().
	Glyph requests made during the visit gate the frame being built, so frame producers call this
	once the frame is out (remote: before FrameInput; local: after commit). No-op without the font
	module or controller. */
	void flushPendingFontGlyphs();

	template <typename T>
	auto addExtension(Rc<T> &&) -> T *;

	template <typename T>
	T *getExtension() const;

	// Window lifecycle seams (called by AppWindow through an AppThread*). Meaningful only on the
	// server, which owns windows; the base defaults are no-ops so a client carries no window state.
	virtual Rc<Director> handleAppWindowCreated(NotNull<AppWindow>,
			const core::FrameConstraints &c);
	virtual void handleAppWindowDestroyed(NotNull<AppWindow>, Rc<Director> &&);

	// Server-side listener seams (called by the local-only Director API through an AppThread*).
	// No-ops on the base / client; the server subclass drives an actual listener.
	virtual bool isServerThread() const; // can listen for connections
	virtual bool isListening() const;
	virtual bool setListenAddress(StringView);

	virtual bool shareWindow(AppWindow *, SpanView<core::Queue *>,
			const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> & = {});

	// Remote auth/compression config (server-side): the bearer key a client must present (empty
	// rejects all) and the server's LZ4 dictionary (overrides the client's suggestion). No-ops on
	// the base / client.
	virtual bool setBearerKey(BytesView);
	virtual bool setCompressionDictionary(BytesView);

	// Register a reply waiter for `serial`. `timeoutUs` is the relative reply deadline
	// (microseconds): on expiry failTimedOutRequests() completes the waiter with a local protocol
	// error, and a `fatal` one resets the connection. 0 means no deadline.
	virtual void waitForReply(uint32_t,
			Function<void(const remote::MessageHeader &, BytesView payload)> &&, uint64_t timeoutUs,
			bool fatal = true);

	/* Abandon every Domain::Data block still streaming from this side: sends Cancel to the peer and
	fails each waiting caller. Returns the number cancelled. App thread only. */
	virtual size_t cancelOutgoingTransfers();

	virtual AppThread *getPeerThread() const override { return const_cast<AppThread *>(this); }

protected:
	// The client font controller drives the remoteSend* facade below.
	friend class font::FontControllerRemote;

	virtual bool startListening();
	virtual bool stopListening();

	// Context-bridge hooks (the decoupling seam). The base calls these; subclasses route them to
	// their own context (server -> Context, client -> ClientContext).
	virtual void handleThreadInitialized();
	virtual void handleThreadDisposed();
	virtual void handleThreadUpdated(const UpdateTime &);

	virtual void performAppUpdate(const UpdateTime &, bool wakeup);
	virtual void performUpdate(bool wakeup);

	virtual void loadExtensions();
	virtual void initializeExtensions();
	virtual void finalizeExtensions();

	virtual bool dispatchMessage(const remote::MessageHeader &, BytesView payload);

	// Watchdog over pending reply waiters, run on the keepalive cadence. Expired requests are
	// completed with a synthesized local protocol-error header and dropped. Returns true if any
	// timed out; the caller should then reset the connection.
	bool failTimedOutRequests();

	// Run `cb` on this thread when a transport has work: readiness of `handle` when it is valid,
	// otherwise a change of `wait`. Null when there is neither; the update tick then services it.
	Rc<sprt::dispatch::Handle> watchTransport(sprt::dispatch::NativeHandle handle,
			remote::TransportWaitAddress wait, Function<void()> &&cb);

	// RemotePeer: the base has no connection and returns false; the client routes to its
	// connection. A server sends through its sessions instead. remoteSendCborWithReply registers the
	// reply waiter via waitForReply.
	virtual bool remoteSendCbor(remote::Domain, uint8_t code, const Value &,
			uint32_t *outSerial = nullptr) override;
	virtual bool remoteSendRaw(remote::Domain, uint8_t code, BytesView,
			uint32_t *outSerial = nullptr) override;
	virtual bool remoteSendCborReply(uint32_t serial, remote::Domain, uint8_t code,
			const Value &) override;
	virtual bool remoteSendError(remote::Domain, uint8_t code, uint32_t serial) override;
	virtual bool remoteSendCborWithReply(remote::Domain, uint8_t code, const Value &,
			ReplyCallback &&, uint64_t timeoutUs) override;

	sprt::dispatch::Looper *_appLooper = nullptr;
	Rc<sprt::dispatch::TimerHandle> _timer;
	UpdateTime _time;
	uint64_t _clock = 0;
	uint64_t _startTime = 0;
	uint64_t _lastUpdate = 0;

	bool _extensionsInitialized = false;

	NetworkFlags _networkFlags = NetworkFlags::None;
	ThemeInfo _themeInfo;

	Rc<ResourceCache> _resourceCache;

	HashMap<sprt::type_index, Rc<ApplicationExtension>> _extensions;
	Map<Rc<Ref>, Function<void(const UpdateTime &, bool)>> _listeners;

	// Requests waiting for a response from the remote side, keyed by message serial.
	remote::ReplyTable _replies;

	// Bidirectional block transfer (remote::Domain::Data), constructed in threadInit.
	Rc<BlockTransferManager> _blockTransfer;
};

template <typename T>
auto AppThread::addExtension(Rc<T> &&t) -> T * {
	auto it = _extensions.find(sprt::type_index(typeid(T)));
	if (it == _extensions.end()) {
		auto ref = t.get();
		// Key on the declared type T, not the dynamic type, so a leaf (e.g. FontControllerLocal)
		// can be registered and retrieved under its abstract base (font::FontController).
		it = _extensions.emplace(sprt::type_index(typeid(T)), move(t)).first;
		if (_extensionsInitialized) {
			ref->initialize(this);
		}
	}
	return it->second.get_cast<T>();
}

template <typename T>
auto AppThread::getExtension() const -> T * {
	auto it = _extensions.find(sprt::type_index(typeid(T)));
	if (it != _extensions.end()) {
		return reinterpret_cast<T *>(it->second.get());
	}
	return nullptr;
}


} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLAPPLICATION_H_ */
