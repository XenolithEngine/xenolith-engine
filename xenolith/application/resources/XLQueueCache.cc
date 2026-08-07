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

#include "XLQueueCache.h"
#include "XLResourceCache.h"
#include "XLAppThread.h"
#include "XLCoreRenderSession.h"
#include "XLCoreResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

QueueCache::~QueueCache() { }

bool QueueCache::init(AppThread *app) {
	_application = app;
	return true;
}

void QueueCache::initialize(AppThread *) { }

void QueueCache::invalidate(AppThread *app) {
	// Runs at app-thread teardown, which happens before the render loop stops - the queues have to
	// let go of their GPU objects while there is still a device to hand them back to.
	for (auto &it : _entries) {
		if (it.second.queue && it.second.state == State::Ready) {
			if (auto res = it.second.queue->getInternalResource()) {
				if (auto cache = app ? app->getExtension<ResourceCache>() : nullptr) {
					cache->removeResource(res->getName());
				}
			}
		}
		// Answer anyone still waiting on a build that will never finish now.
		for (auto &cb : it.second.pending) { cb(nullptr); }
		it.second.pending.clear();
	}
	_entries.clear();
	_application = nullptr;
}

void QueueCache::update(AppThread *, const UpdateTime &, bool) { }

void QueueCache::acquire(StringView name, NotNull<core::RenderServerChannel> channel,
		const Callback<bool(core::Queue::Builder &)> &build,
		Function<void(Rc<core::Queue> &&)> &&complete) {
	if (name.empty()) {
		log::source().error("QueueCache", "acquire: a queue name is required");
		if (complete) {
			complete(nullptr);
		}
		return;
	}

	auto it = _entries.find(name);
	if (it != _entries.end()) {
		switch (it->second.state) {
		case State::Ready:
			if (complete) {
				complete(Rc<core::Queue>(it->second.queue));
			}
			return;
		case State::Building:
			// One compile per name: get in line behind the one already running.
			if (complete) {
				it->second.pending.emplace_back(sp::move(complete));
			}
			return;
		case State::Failed:
			if (complete) {
				complete(nullptr);
			}
			return;
		}
	}

	auto nameStr = name.str<Interface>();

	core::Queue::Builder builder(name);
	if (!build(builder)) {
		log::source().error("QueueCache", "acquire: builder declined for '", name, "'");
		auto &entry = _entries.emplace(nameStr, Entry()).first->second;
		entry.state = State::Failed;
		if (complete) {
			complete(nullptr);
		}
		return;
	}

	auto queue = Rc<core::Queue>::create(sp::move(builder));
	if (!queue) {
		auto &entry = _entries.emplace(nameStr, Entry()).first->second;
		entry.state = State::Failed;
		if (complete) {
			complete(nullptr);
		}
		return;
	}

	auto &entry = _entries.emplace(nameStr, Entry()).first->second;
	entry.state = State::Building;
	entry.queue = queue;
	if (complete) {
		entry.pending.emplace_back(sp::move(complete));
	}

	log::source().debug("QueueCache", "compiling '", name, "'");

	// The compile completion fires on the render loop's thread, not this one — so hop back before
	// touching `_entries` or the ResourceCache, both of which are app-thread-only. (Without the
	// hop this looks fine and works: the test's own "prewarm complete" line just prints from the
	// wrong thread, which is what gave it away.)
	//
	// `this` is safe to capture: the cache is an AppThread extension, and the app thread is kept
	// alive across the hop by the target below.
	channel->compileRenderQueue(queue, [this, nameStr](bool success) mutable {
		_application->performOnAppThread([this, nameStr = sp::move(nameStr), success]() mutable {
			finishEntry(nameStr, success);
		}, _application);
	});
}

void QueueCache::finishEntry(StringView name, bool success) {
	auto it = _entries.find(name);
	if (it == _entries.end()) {
		// invalidate() got here first.
		return;
	}

	auto &entry = it->second;
	entry.state = success ? State::Ready : State::Failed;

	if (!success) {
		log::source().error("QueueCache", "failed to compile '", name, "'");
		entry.queue = nullptr;
	} else if (entry.queue) {
		// The queue's internal resource is registered here, once, and stays registered while the
		// entry lives. A Scene that adopts the queue must not do this itself - ResourceCache is
		// name-keyed with no refcount, so the first adopter to finish would erase it for the rest.
		if (auto res = entry.queue->getInternalResource()) {
			if (auto cache = _application ? _application->getExtension<ResourceCache>() : nullptr) {
				cache->addResource(res);
			}
		}
	}

	// Move the list out first: a callback that acquires another queue would otherwise rehash the
	// map underneath the iteration.
	auto pending = sp::move(entry.pending);
	entry.pending.clear();

	for (auto &cb : pending) { cb(Rc<core::Queue>(entry.queue)); }
}

Rc<core::Queue> QueueCache::get(StringView name) const {
	auto it = _entries.find(name);
	if (it == _entries.end() || it->second.state != State::Ready) {
		return nullptr;
	}
	return it->second.queue;
}

auto QueueCache::getState(StringView name) const -> State {
	auto it = _entries.find(name);
	return it == _entries.end() ? State::Failed : it->second.state;
}

bool QueueCache::has(StringView name) const { return _entries.find(name) != _entries.end(); }

void QueueCache::release(StringView name) {
	auto it = _entries.find(name);
	if (it == _entries.end()) {
		return;
	}
	if (it->second.queue) {
		if (auto res = it->second.queue->getInternalResource()) {
			if (auto cache = _application ? _application->getExtension<ResourceCache>() : nullptr) {
				cache->removeResource(res->getName());
			}
		}
	}
	_entries.erase(it);
}

uint32_t QueueCache::trim() {
	uint32_t removed = 0;
	auto it = _entries.begin();
	while (it != _entries.end()) {
		// getReferenceCount() == 1 means this cache holds the only reference: no Scene adopted it
		// and no frame is in flight on it.
		if (it->second.state == State::Ready && it->second.queue
				&& it->second.queue->getReferenceCount() == 1) {
			if (auto res = it->second.queue->getInternalResource()) {
				if (auto cache = _application ? _application->getExtension<ResourceCache>() : nullptr) {
					cache->removeResource(res->getName());
				}
			}
			it = _entries.erase(it);
			++removed;
		} else {
			++it;
		}
	}
	return removed;
}

} // namespace stappler::xenolith
