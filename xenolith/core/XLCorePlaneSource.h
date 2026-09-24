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

#ifndef XENOLITH_CORE_XLCOREPLANESOURCE_H_
#define XENOLITH_CORE_XLCOREPLANESOURCE_H_

#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

/* A WINDOW'S IMAGES AS A PLANE: what a compositor in the same process reads.

A virtual window renders into a pseudo-swapchain; every image it presents is PUBLISHED here as a
PlaneFrame, and the slot it lives in stays out of the ring for as long as anybody holds that frame.
That is the whole contract:

* `PlaneSource::getLatest()` is the newest finished frame, and holding the Rc is what keeps its
  slot from being drawn into again. A compositor takes it for a frame of its own and lets it go
  when that frame is done - several compositor frames can hold different images at once.
* A frame is published only when it is readable: on Vulkan, after its layout moved from
  PresentSrc to ShaderReadOnly (a device task), on the CPU backend at once.
* Letting go is what returns the slot: the backend moves the image back to PresentSrc (Vulkan) and
  only then unpins it, so the application never draws into an image that is still being read.

The source belongs to the window, not to a swapchain: a resize replaces the swapchain and its slot
table, and a frame of the old generation is still valid - it holds its image - until released. */

// Which slots of ONE swapchain generation are held by readers. Thread-safe: slots are pinned by the
// presentation thread and unpinned from wherever the last reader let go.
class SP_PUBLIC PlaneSlotTable : public Ref {
public:
	virtual ~PlaneSlotTable() = default;

	bool init(uint32_t slotCount);

	// False if the slot is out of range or already pinned.
	bool pin(uint32_t slot);
	void unpin(uint32_t slot);

	bool isPinned(uint32_t slot) const;
	Vector<uint32_t> getPinned() const;

	uint32_t getSlotCount() const;

	// The swapchain this table belongs to is gone. A frame released afterwards returns nothing to a
	// ring - there is none - and its image is simply dropped.
	void retire();
	bool isRetired() const;

protected:
	mutable sprt::mutex _mutex;
	Vector<bool> _pinned;
	bool _retired = false;
};

// One published image. The backend's release runs when the last reference goes away, on whatever
// thread that is; it is expected to hop to the presentation thread itself.
class SP_PUBLIC PlaneFrame : public Ref {
public:
	using ReleaseCallback = Function<void()>;

	virtual ~PlaneFrame();

	bool init(uint64_t serial, uint32_t slot, Rc<ImageObject> &&, ReleaseCallback &&);

	// Increases with every published frame of the window, across swapchain generations.
	uint64_t getSerial() const { return _serial; }

	// The slot of the swapchain generation the frame was drawn in.
	uint32_t getSlot() const { return _slot; }

	ImageObject *getImage() const { return _image; }
	Extent2 getExtent() const;

protected:
	uint64_t _serial = 0;
	uint32_t _slot = 0;
	Rc<ImageObject> _image;
	ReleaseCallback _release;
};

// A window's published frames: the latest one, and a notification when a newer one is ready.
class SP_PUBLIC PlaneSource : public Ref {
public:
	// Called on the presentation thread with each newly published frame.
	using Listener = Function<void(NotNull<PlaneFrame>)>;

	virtual ~PlaneSource() = default;

	bool init();

	// The newest readable frame, or null before the first one. Any thread.
	Rc<PlaneFrame> getLatest() const;

	// Set and called on the presentation thread.
	void setListener(Listener &&);

	// Presentation thread. A frame older than the latest one - device tasks can finish out of
	// order - is dropped at once, which releases it.
	void publish(Rc<PlaneFrame> &&);

	// Drop the latest frame (the window is going away).
	void clear();

	// The serial the next published frame gets. Presentation thread.
	uint64_t acquireSerial();

	// The slots of the current swapchain generation. Set by the swapchain as it is created.
	void setSlotTable(Rc<PlaneSlotTable> &&);
	Rc<PlaneSlotTable> getSlotTable() const;

	uint64_t getPublishedCount() const;

protected:
	mutable sprt::mutex _mutex;
	Rc<PlaneFrame> _latest;
	Rc<PlaneSlotTable> _slots;
	Listener _listener;
	uint64_t _nextSerial = 1;
	uint64_t _published = 0;
};

} // namespace stappler::xenolith::core

#endif // XENOLITH_CORE_XLCOREPLANESOURCE_H_
