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

#include "XLCorePlaneSource.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool PlaneSlotTable::init(uint32_t slotCount) {
	_pinned.resize(slotCount, false);
	_views.resize(slotCount);
	return true;
}

bool PlaneSlotTable::pin(uint32_t slot) {
	sprt::unique_lock lock(_mutex);
	if (slot >= _pinned.size() || _pinned[slot]) {
		return false;
	}
	_pinned[slot] = true;
	return true;
}

void PlaneSlotTable::unpin(uint32_t slot) {
	sprt::unique_lock lock(_mutex);
	if (slot < _pinned.size()) {
		_pinned[slot] = false;
	}
}

bool PlaneSlotTable::isPinned(uint32_t slot) const {
	sprt::unique_lock lock(_mutex);
	return slot < _pinned.size() && _pinned[slot];
}

Vector<uint32_t> PlaneSlotTable::getPinned() const {
	sprt::unique_lock lock(_mutex);
	Vector<uint32_t> ret;
	for (uint32_t i = 0; i < _pinned.size(); ++i) {
		if (_pinned[i]) {
			ret.emplace_back(i);
		}
	}
	return ret;
}

uint32_t PlaneSlotTable::getSlotCount() const {
	sprt::unique_lock lock(_mutex);
	return uint32_t(_pinned.size());
}

void PlaneSlotTable::retire() {
	// A view in use is held by the frame that bound it, so the table may let go of it now.
	Vector<Rc<ImageView>> views;
	sprt::unique_lock lock(_mutex);
	_retired = true;
	views = sp::move(_views);
}

bool PlaneSlotTable::isRetired() const {
	sprt::unique_lock lock(_mutex);
	return _retired;
}

Rc<ImageView> PlaneSlotTable::getView(uint32_t slot) const {
	sprt::unique_lock lock(_mutex);
	return slot < _views.size() ? _views[slot] : nullptr;
}

void PlaneSlotTable::setView(uint32_t slot, Rc<ImageView> &&view) {
	sprt::unique_lock lock(_mutex);
	if (!_retired && slot < _views.size()) {
		_views[slot] = sp::move(view);
	}
}

PlaneFrameLock::~PlaneFrameLock() {
	if (!_frame) {
		return;
	}

	if (!_loop || _loop->isOnThisThread()) {
		_frame->unlock(_access);
	} else {
		_loop->performOnThread(
				[frame = sp::move(_frame), access = _access] { frame->unlock(access); });
	}
}

bool PlaneFrameLock::init(NotNull<PlaneFrame> frame, PlaneFrameAccess access, NotNull<Loop> loop) {
	_frame = frame.get();
	_loop = loop.get();
	_access = access;
	return true;
}

PlaneFrame::~PlaneFrame() {
	if (_release) {
		_release();
	}
}

bool PlaneFrame::init(uint64_t serial, uint32_t slot, Rc<ImageObject> &&image,
		Rc<PlaneSlotTable> &&slots, ReleaseCallback &&release) {
	_serial = serial;
	_slot = slot;
	_image = sp::move(image);
	_slots = sp::move(slots);
	_release = sp::move(release);
	return _image != nullptr;
}

Extent2 PlaneFrame::getExtent() const {
	auto extent = _image->getInfo().extent;
	return Extent2(extent.width, extent.height);
}

void PlaneFrame::lock(PlaneFrameAccess access, NotNull<Loop> loop, LockCallback &&cb) {
	Vector<LockRequest> granted;
	{
		sprt::unique_lock lock(_lockMutex);
		_lockQueue.emplace_back(LockRequest{access, loop.get(), sp::move(cb)});
		granted = acquireGranted();
	}
	for (auto &it : granted) { it.callback(Rc<PlaneFrameLock>::create(this, it.access, it.loop)); }
}

void PlaneFrame::unlock(PlaneFrameAccess access) {
	Vector<LockRequest> granted;
	{
		sprt::unique_lock lock(_lockMutex);
		if (access == PlaneFrameAccess::Exclusive) {
			_exclusiveLock = false;
		} else if (_sharedLocks > 0) {
			--_sharedLocks;
		}
		granted = acquireGranted();
	}
	for (auto &it : granted) { it.callback(Rc<PlaneFrameLock>::create(this, it.access, it.loop)); }
}

Vector<PlaneFrame::LockRequest> PlaneFrame::acquireGranted() {
	Vector<LockRequest> ret;
	auto it = _lockQueue.begin();
	while (it != _lockQueue.end() && !_exclusiveLock) {
		if (it->access == PlaneFrameAccess::Exclusive) {
			if (_sharedLocks > 0) {
				break;
			}
			_exclusiveLock = true;
		} else {
			++_sharedLocks;
		}
		ret.emplace_back(sp::move(*it));
		++it;
	}
	_lockQueue.erase(_lockQueue.begin(), it);
	return ret;
}

bool PlaneSource::init() { return true; }

Rc<PlaneFrame> PlaneSource::getLatest() const {
	sprt::unique_lock lock(_mutex);
	return _latest;
}

void PlaneSource::setListener(Listener &&listener) { _listener = sp::move(listener); }

void PlaneSource::publish(Rc<PlaneFrame> &&frame) {
	if (!frame) {
		return;
	}

	// Both the frame being replaced and a frame that lost the race are released outside the lock:
	// a release posts work, and nothing it reaches may need this lock.
	Rc<PlaneFrame> released;
	{
		sprt::unique_lock lock(_mutex);
		if (_latest && frame->getSerial() <= _latest->getSerial()) {
			released = sp::move(frame);
		} else {
			released = sp::move(_latest);
			_latest = frame;
			++_published;
		}
	}

	if (frame && _listener) {
		_listener(frame);
	}
}

void PlaneSource::clear() {
	Rc<PlaneFrame> released;
	{
		sprt::unique_lock lock(_mutex);
		released = sp::move(_latest);
	}
}

uint64_t PlaneSource::acquireSerial() {
	sprt::unique_lock lock(_mutex);
	return _nextSerial++;
}

void PlaneSource::setSlotTable(Rc<PlaneSlotTable> &&slots) {
	sprt::unique_lock lock(_mutex);
	_slots = sp::move(slots);
}

Rc<PlaneSlotTable> PlaneSource::getSlotTable() const {
	sprt::unique_lock lock(_mutex);
	return _slots;
}

uint64_t PlaneSource::getPublishedCount() const {
	sprt::unique_lock lock(_mutex);
	return _published;
}

} // namespace stappler::xenolith::core
