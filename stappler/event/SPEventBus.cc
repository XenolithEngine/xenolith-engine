/**
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

#include "SPEventBus.h"
#include "SPEventLooper.h"
#include "SPMemInterface.h"
#include <mutex>

namespace stappler::event {

BusEvent::BusEvent(BusEventCategory category) : _category(category) { }

BusDelegate::~BusDelegate() {
	if (_bus) {
		_bus->removeListener(this);
		_bus = nullptr;
	}
}

bool BusDelegate::init(NotNull<Looper> looper, SpanView<BusEventCategory> cat, NotNull<Ref> ref,
		BusEventCallback &&cb) {
	_looper = looper;
	_owner = ref;
	_categories = cat.vec<memory::StandartInterface>();
	_callback = std::move(cb);
	return true;
}

bool BusDelegate::init(NotNull<Looper> looper, BusEventCategory cat, NotNull<Ref> ref,
		BusEventCallback &&cb) {
	return init(looper, sprt::makeSpanView(&cat, 1), ref, sp::move(cb));
}

// should be called when owner is disabled
void BusDelegate::invalidate() {
	if (!_looper) {
		return;
	}

	if (_looper->isOnThisThread()) {
		if (_state == Pending) {
			finalize();
		} else {
			_state = Invalidated;
		}
	} else {
		_looper->performOnThread([this] {
			if (_state == Pending) {
				finalize();
			} else {
				_state = Invalidated;
			}
		}, this);
	}
}

void BusDelegate::handleEvent(Bus &bus, const BusEvent &event) {
	if (!_looper->isOnThisThread()) {
		log::source().error("event::BusDelegate", "BusEvent '",
				bus.getCategoryName(event.getCategory()), "' should be handled in looper context");
		return;
	}

	if (_owner && _callback) {
		auto refId = retain();
		_state = Active;
		_callback(bus, event, *this);
		if (_state == Invalidated) {
			finalize();
		} else {
			_state = Pending;
		}
		release(refId);
	}
}

void BusDelegate::handleAdded(Bus *bus) { _bus = bus; }

void BusDelegate::handleRemoved(Bus *) { _bus = nullptr; }

void BusDelegate::finalize() {
	_owner = nullptr;
	_callback = nullptr;
	_state = Finalized;
}

Bus::~Bus() {
	auto it = _loopers.begin();
	while (it != _loopers.end()) {
		it->first->detachBus(this);
		it = _loopers.erase(it);
	}
}

BusEventCategory Bus::allocateCategory(StringView name) {
	std::unique_lock lock(_mutex);

	_categories.emplace_back(name.str<memory::StandartInterface>());
	return BusEventCategory(static_cast<uint32_t>(_categories.size()));
}

StringView Bus::getCategoryName(BusEventCategory id) const {
	std::unique_lock lock(_mutex);
	if (id.get() > _categories.size() || id.get() == 0) {
		return StringView();
	}
	return _categories.at(id.get() - 1);
}

void Bus::addListener(NotNull<BusDelegate> delegate) {
	std::unique_lock lock(_mutex);
	doAddListener(delegate, lock);
}

void Bus::removeListener(NotNull<BusDelegate> delegate) {
	std::unique_lock lock(_mutex);
	doRemoveListener(delegate, lock);
}

void Bus::dispatchEvent(NotNull<BusEvent> ev) {
	mem_std::Map<Looper *, mem_std::Vector<Rc<BusDelegate>>> loopers;

	_mutex.lock();
	auto it = _listenersByCategories.find(ev->getCategory());
	if (it != _listenersByCategories.end()) {
		for (auto &iit : it->second) {
			auto lIt = loopers.find(iit->getLooper());
			if (lIt == loopers.end()) {
				lIt = loopers.emplace(iit->getLooper(), mem_std::Vector<Rc<BusDelegate>>()).first;
			}
			lIt->second.emplace_back(iit);
		}
	}
	_mutex.unlock();

	for (auto &it : loopers) {
		it.first->performOnThread(
				[listeners = sp::move(it.second), event = Rc<BusEvent>(ev), this]() {
			for (auto &it : listeners) { it->handleEvent(*this, *event); }
		}, this);
	}
}

void Bus::invalidateLooper(Looper *looper) {
	std::unique_lock lock(_mutex);

	auto it = _loopers.find(looper);
	if (it != _loopers.end()) {
		// remove looper data to prevent infinite recursion
		auto v = sp::move(it->second);
		_loopers.erase(it);

		for (auto &it : v) { doRemoveListener(it, lock); }
	}
}

void Bus::doAddListener(BusDelegate *delegate, std::unique_lock<std::mutex> &) {
	if (delegate->getBus()) {
		log::source().error("event::BusDelegate", "BusDelegate already attached to bus");
		return;
	}

	for (auto &it : delegate->getCategories()) {
		auto cIt = _listenersByCategories.find(it);
		if (cIt == _listenersByCategories.end()) {
			cIt = _listenersByCategories.emplace(it, mem_std::HashSet<BusDelegate *>()).first;
		}
		cIt->second.emplace(delegate);
	}

	auto cIt = _loopers.find(delegate->getLooper());
	if (cIt == _loopers.end()) {
		cIt = _loopers.emplace(delegate->getLooper(), mem_std::HashSet<BusDelegate *>()).first;
		delegate->getLooper()->attachBus(this);
	}
	cIt->second.emplace(delegate);

	_listeners.emplace(delegate);
	delegate->handleAdded(this);
}

void Bus::doRemoveListener(BusDelegate *delegate, std::unique_lock<std::mutex> &) {
	if (delegate->getBus() != this) {
		log::source().error("event::BusDelegate", "BusDelegate is not attached to this bus");
		return;
	}

	delegate->handleRemoved(this);

	auto refId = delegate->retain();
	for (auto &it : delegate->getCategories()) {
		auto cIt = _listenersByCategories.find(it);
		if (cIt != _listenersByCategories.end()) {
			cIt->second.erase(delegate);
			if (cIt->second.empty()) {
				_listenersByCategories.erase(cIt);
			}
		}
	}

	auto lIt = _loopers.find(delegate->getLooper());
	if (lIt != _loopers.end()) {
		lIt->second.erase(delegate);
		if (lIt->second.empty()) {
			lIt->first->detachBus(this);
			_loopers.erase(lIt);
		}
	}

	_listeners.erase(delegate);

	delegate->release(refId);
}

} // namespace stappler::event
