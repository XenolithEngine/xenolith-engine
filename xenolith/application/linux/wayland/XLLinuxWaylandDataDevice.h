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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDDATADEVICE_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDDATADEVICE_H_

#include "SPBuffer.h"
#include "SPEventLooper.h"
#include "SPEventPollHandle.h"
#include "XLLinuxWaylandSeat.h"
#include "linux/wayland/XLLinuxWaylandLibrary.h"

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct WaylandDisplay;
struct WaylandDataDevice;

struct SP_PUBLIC WaylandDataDeviceManager : public Ref {
	virtual ~WaylandDataDeviceManager();

	bool init(NotNull<WaylandDisplay>, wl_registry *, uint32_t name, uint32_t version);

	Rc<WaylandLibrary> wayland;
	WaylandDisplay *root;
	wl_data_device_manager *manager = nullptr;
};

struct SP_PUBLIC WaylandDataOffer : public Ref {
	virtual ~WaylandDataOffer();

	bool init(NotNull<WaylandLibrary>, wl_data_offer *);

	Rc<WaylandLibrary> wayland;
	wl_data_offer *offer = nullptr;

	uint32_t actions = 0;
	uint32_t selectedAction = 0;

	bool attached = false;
	uint32_t serial = 0;
	struct wl_surface *surface = nullptr;
	wl_fixed_t x;
	wl_fixed_t y;

	Vector<String> types;
};

struct SP_PUBLIC WaylandDataInputTransfer : public Ref {
	virtual ~WaylandDataInputTransfer();

	bool init(StringView type, NotNull<WaylandDataOffer>, Rc<ClipboardRequest> &&req);

	void schedule(NotNull<event::Looper>);
	void commit();
	void cancel();

	String type;
	Rc<WaylandDataOffer> offer;
	Rc<ClipboardRequest> request;
	int pipefd[2] = {-1, -1};
	StackBuffer<256_KiB> buffer;
	Rc<event::PollHandle> handle;
	Vector<Bytes> chunks;
};

struct SP_PUBLIC WaylandDataOutputTransfer : public Ref {
	static void create(Bytes &&, int fd);

	virtual ~WaylandDataOutputTransfer();

	bool init(Bytes &&, int fd);

	bool write();

	Bytes data;
	size_t offset = 0;
	size_t blockSize = 64_KiB;
	int targetFd = -1;

	Rc<event::PollHandle> handle;
};

struct SP_PUBLIC WaylandDataSource : public Ref {
	virtual ~WaylandDataSource();

	bool init(NotNull<WaylandDataDevice>, Rc<ClipboardData> &&req);

	void send(StringView type, int32_t fd);
	void cancel();

	Rc<WaylandLibrary> wayland;
	WaylandDataDevice *device = nullptr;
	wl_data_source *source = nullptr;
	Rc<ClipboardData> data;
};

struct SP_PUBLIC WaylandDataDevice : public Ref {
	virtual ~WaylandDataDevice();

	bool init(NotNull<WaylandDataDeviceManager>, NotNull<WaylandSeat>);

	void setSelection(NotNull<WaylandDataOffer>);

	void enter(NotNull<WaylandDataOffer>);
	void leave();
	void drop();

	Status readFromClipboard(Rc<ClipboardRequest> &&req);
	Status probeClipboard(Rc<ClipboardProbe> &&probe);
	Status writeToClipboard(Rc<ClipboardData> &&);

	WaylandLibrary *wayland = nullptr;
	WaylandDataDeviceManager *manager = nullptr;
	WaylandSeat *seat = nullptr;
	wl_data_device *device = nullptr;

	Rc<WaylandDataOffer> selectionOffer;
	Rc<WaylandDataSource> selectionSource;
	Rc<WaylandDataOffer> dnd;
};

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDDATADEVICE_H_
