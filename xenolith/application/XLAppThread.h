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

// Base application thread: pure thread/extension/update machinery. It holds NO reference to a
// Context (the full Context* lives only on the server subclass). The few context-derived services
// the base needs are reached through the protected virtual hooks below; window management, the
// remote listener, clipboard/screen/URL, and the lifecycle source differ per subclass.
//
// Concrete subclasses: ServerAppThread (owns a Context, windows, the listener) and ClientAppThread
// (owns a standalone ClientContext). The base is abstract.
class SP_PUBLIC AppThread : public sprt::dispatch::Thread {
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

	virtual void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref * = nullptr) = 0;

	virtual void openUrl(StringView) = 0;

	// Config source for the base thread machinery (looper/timer) and external consumers (network).
	virtual const ContextInfo *getContextInfo() const = 0;

	// Local GPU loop, server-only; nullptr on a client (no local rendering). Used by the graphics
	// path (Director / ResourceCache / 2D renderer) that still reaches the loop directly.
	virtual core::Loop *getGlLoop() const { return nullptr; }

	sprt::dispatch::Looper *getLooper() const { return _appLooper; }

	NetworkFlags getNetworkFlags() const { return _networkFlags; }
	const ThemeInfo &getThemeInfo() const { return _themeInfo; }

	bool addListener(NotNull<Ref>, Function<void(const UpdateTime &, bool)> &&);
	bool removeListener(NotNull<Ref>);

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

	virtual bool shareWindow(AppWindow *);
	virtual bool shareQueue(core::Queue *);

	// Remote auth/compression config (server-side; set by the local-only Director API). The bearer
	// key a connecting client must present (empty ⇒ reject all) and the server's LZ4 dictionary
	// (priority over the client's suggestion). No-ops on the base / client.
	virtual bool setBearerKey(BytesView);
	virtual bool setCompressionDictionary(BytesView);

protected:
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
};

template <typename T>
auto AppThread::addExtension(Rc<T> &&t) -> T * {
	auto it = _extensions.find(sprt::type_index(typeid(T)));
	if (it == _extensions.end()) {
		auto ref = t.get();
		it = _extensions.emplace(sprt::type_index(typeid(*t.get())), move(t)).first;
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
