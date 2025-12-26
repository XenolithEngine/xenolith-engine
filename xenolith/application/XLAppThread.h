/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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
#include "XLEventListener.h"
#include "XLLiveReload.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;

class SP_PUBLIC AppThread : public thread::Thread {
public:
	static EventHeader onNetworkState;
	static EventHeader onThemeInfo;

	using Task = thread::Task;

	using ExecuteCallback = Function<bool(const Task &)>;
	using CompleteCallback = Function<void(const Task &, bool)>;

	virtual ~AppThread() = default;

	virtual bool init(NotNull<Context>);

	virtual void run();

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void stop() override;

	virtual void wakeup();

	virtual void handleNetworkStateChanged(NetworkFlags);
	virtual void handleThemeInfoChanged(const ThemeInfo &);

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

	// Read data from OS clipboard
	//
	// - dataCallback will receive data with selected type in this thread
	// - selectCallback will be called in unknown OS thread and should select one of available data
	// types by return it, or return StringView() to discard request
	// - ref is preserved for all operation direction
	void readFromClipboard(Function<void(Status, BytesView, StringView)> &&dataCallback,
			Function<StringView(SpanView<StringView>)> &&selectCallback, Ref *ref = nullptr);

	// Test, which data is available to read from clipboard (if any)
	//
	// - cb will receive a list of types, available to read in this thread
	// - ref is preserved for all operation direction
	void probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb, Ref *ref = nullptr);

	// Provide static data for OS clipboard with specific type
	// - ref is preserved until clibpoard data remains actial for OS
	void writeToClipboard(BytesView data, StringView contentType = StringView("text/plain"),
			Ref *ref = nullptr, StringView label = StringView());

	// Provide data for OS clipboard via callback
	//
	// - dataCallback will be called in unknown OS thread to access actual data with specific type.
	// Callback is preserved until clibpoard data remains actial for OS
	// - types - list of types, that can be accessed with provided callback
	// - ref is preserved until clibpoard data remains actial for OS
	void writeToClipboard(Function<Bytes(StringView)> &&dataCallback, SpanView<StringView> types,
			Ref *ref = nullptr, StringView label = StringView());

	void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref * = nullptr);

	Context *getContext() const { return _context; }
	event::Looper *getLooper() const { return _appLooper; }

	NetworkFlags getNetworkFlags() const { return _networkFlags; }
	const ThemeInfo &getThemeInfo() const { return _themeInfo; }

	bool addListener(NotNull<Ref>, Function<void(const UpdateTime &, bool)> &&);
	bool removeListener(NotNull<Ref>);

	template <typename T>
	auto addExtension(Rc<T> &&) -> T *;

	template <typename T>
	T *getExtension() const;

	virtual Rc<Director> handleAppWindowCreated(NotNull<AppWindow>,
			const core::FrameConstraints &c);
	virtual void handleAppWindowDestroyed(NotNull<AppWindow>, Rc<Director> &&);

	virtual void openUrl(StringView);

protected:
	virtual void performAppUpdate(const UpdateTime &, bool wakeup);
	virtual void performUpdate(bool wakeup);

	virtual void loadExtensions();
	virtual void initializeExtensions();
	virtual void finalizeExtensions();

	virtual bool shouldPreserveDirector(NotNull<AppWindow>, NotNull<Director>);
	virtual void preserveDirector(NotNull<AppWindow>, Rc<Director> &&);

	virtual bool hasPreservedDirector(NotNull<AppWindow>);
	virtual Rc<Director> acquirePreservedDirector(NotNull<AppWindow>);

	virtual Rc<Director> makeDirector(NotNull<AppWindow>, const core::FrameConstraints &);
	virtual Rc<Scene> makeScene(NotNull<AppWindow> w, const core::FrameConstraints &c);

	virtual void performLiveReload(NotNull<LiveReloadLibrary> lib);

	Context *_context = nullptr;
	event::Looper *_appLooper = nullptr;
	Rc<event::TimerHandle> _timer;
	UpdateTime _time;
	uint64_t _clock = 0;
	uint64_t _startTime = 0;
	uint64_t _lastUpdate = 0;

	bool _extensionsInitialized = false;

	NetworkFlags _networkFlags = NetworkFlags::None;
	ThemeInfo _themeInfo;

	Rc<ResourceCache> _resourceCache;

	HashMap<std::type_index, Rc<ApplicationExtension>> _extensions;
	Map<Rc<Ref>, Function<void(const UpdateTime &, bool)>> _listeners;

	Set<AppWindow *> _windows;
	HashMap<String, Rc<Director>> _preservedDirectors;

	Rc<EventDelegate> _liveReloadListener;
};

template <typename T>
auto AppThread::addExtension(Rc<T> &&t) -> T * {
	auto it = _extensions.find(std::type_index(typeid(T)));
	if (it == _extensions.end()) {
		auto ref = t.get();
		it = _extensions.emplace(std::type_index(typeid(*t.get())), move(t)).first;
		if (_extensionsInitialized) {
			ref->initialize(this);
		}
	}
	return it->second.get_cast<T>();
}

template <typename T>
auto AppThread::getExtension() const -> T * {
	auto it = _extensions.find(std::type_index(typeid(T)));
	if (it != _extensions.end()) {
		return reinterpret_cast<T *>(it->second.get());
	}
	return nullptr;
}


} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLAPPLICATION_H_ */
