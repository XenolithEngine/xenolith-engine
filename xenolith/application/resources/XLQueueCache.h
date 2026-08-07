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

// Named, already-compiled render graphs, kept for scenes that want to adopt one instead of
// building their own.
//
// What it buys: a popup or a dialog opens dozens of times over a session, and every open used to
// run the whole render-queue compiler - render passes, pipelines, texture-set layout, the internal
// resource. Prewarm the graph once and every later open is a Rc copy.
//
// Why it is safe to build a queue before the window that will use it exists: a compiled Queue holds
// nothing window-specific. Extent is not baked (FrameQueue::setup rewrites every image attachment's
// extent from the frame's FrameConstraints), framebuffers live in the loop-global FrameCache, the
// attachment format comes from Loop::getCommonFormat() and is the same for every window on the
// loop, and the queue is bound to a frame per-frame through FrameRequest::setQueue. The one thing
// that used to weld a queue to a scene - the begin/end callbacks capturing a Scene* - is gone; the
// per-frame pin now lives on the FrameRequest.
//
// Key: the queue name, chosen by the application. Not a derived hash - buildQueueResources is a
// virtual an application fills with arbitrary content, so there is no canonical form to hash. The
// contract that comes with that: THE NAME IDENTIFIES THE GRAPH. Two scenes asking for the same
// name must want the same graph.
//
// Consequence to know about: scenes sharing a queue share its MaterialAttachment, so their
// materials land in one MaterialSet and one texture-set descriptor array. Ids cannot collide
// (getNextMaterialId is atomic), but the array grows with the union of what every sharer brings.
// The queue's internal resource is registered in the ResourceCache by this cache, once, for as long
// as the entry lives - an adopting Scene must not register it (see Scene::_ownsQueue).
//
// App-thread only, so nothing here locks.
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

	// Build (once) and compile the queue named `name`.
	//
	// `build` runs synchronously, on this thread, and ONLY on a miss. `complete` runs on this
	// thread once the queue is compiled - or on the next call, immediately, if it already was.
	// Calls that arrive while a build is in flight queue behind it: there is exactly one
	// compileRenderQueue per name, ever.
	//
	// `channel` is used only to reach the render loop, and the loop belongs to the Context, not to
	// a window - so ANY live window will do. That is what makes it legal to prewarm a popup's queue
	// from the root window, before the popup exists.
	void acquire(StringView name, NotNull<core::RenderServerChannel> channel,
			const Callback<bool(core::Queue::Builder &)> &build,
			Function<void(Rc<core::Queue> &&)> && = nullptr);

	// The compiled queue for `name`, or null unless it is Ready.
	Rc<core::Queue> get(StringView name) const;

	State getState(StringView name) const;
	bool has(StringView name) const;

	// Drop the cache's own reference. Any Scene that adopted the queue, and any frame still in
	// flight, holds one of its own, so this can never free a queue out from under live work.
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
