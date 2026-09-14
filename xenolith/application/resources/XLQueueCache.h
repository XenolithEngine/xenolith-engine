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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLQUEUECACHE_H_
#define XENOLITH_APPLICATION_RESOURCES_XLQUEUECACHE_H_

#include "XLApplicationExtension.h"
#include "XLCoreQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
class RenderServerChannel;
}

// Named, already-compiled render graphs for scenes that adopt one instead of building their own,
// so frequently opened popups and dialogs compile their queue once.
//
// A compiled Queue holds nothing window-specific (extent comes from FrameConstraints, framebuffers
// live in the loop-global FrameCache, the format from Loop::getCommonFormat(), the scene is pinned
// per frame on the FrameRequest), so it can be built before its window exists.
//
// Key: the application-chosen queue name, which must identify the graph (buildQueueResources is
// arbitrary, so there is nothing to hash). Scenes sharing a queue share its MaterialAttachment and
// texture-set array. The cache registers the queue's internal resource in the ResourceCache for the
// entry's lifetime; an adopting Scene must not (see Scene::_ownsQueue).
//
// App-thread only, no locking.
class SP_PUBLIC QueueCache : public ApplicationExtension {
public:
	enum class State {
		Building, // compileRenderQueue is in flight
		Ready,
		Failed,
	};

	virtual ~QueueCache();

	bool init(AppThread *);

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;
	virtual void update(AppThread *, const UpdateTime &, bool) override;

	// Build (once) and compile the queue named `name`. `build` runs synchronously, only on a miss.
	// `complete` runs on this thread when compiled, or immediately if already Ready. Calls during a
	// build queue behind it: one compileRenderQueue per name.
	//
	// `channel` only reaches the render loop, which belongs to the Context, so any live window
	// works (e.g. prewarm a popup's queue from the root window).
	void acquire(StringView name, NotNull<core::RenderServerChannel> channel,
			const Callback<bool(core::Queue::Builder &)> &build,
			Function<void(Rc<core::Queue> &&)> && = nullptr);

	// The compiled queue for `name`, or null unless it is Ready.
	Rc<core::Queue> get(StringView name) const;

	State getState(StringView name) const;
	bool has(StringView name) const;

	// Drop the cache's own reference; adopting Scenes and in-flight frames hold their own.
	void release(StringView name);

	// Drop every entry nothing else references. Returns how many went.
	uint32_t trim();

	uint32_t getSize() const { return uint32_t(_entries.size()); }

protected:
	struct Entry {
		State state = State::Building;
		Rc<core::Queue> queue;
		// Callers that asked while the build was in flight.
		Vector<Function<void(Rc<core::Queue> &&)>> pending;
	};

	void finishEntry(StringView name, bool success);

	AppThread *_application = nullptr;
	Map<String, Entry> _entries;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_RESOURCES_XLQUEUECACHE_H_
