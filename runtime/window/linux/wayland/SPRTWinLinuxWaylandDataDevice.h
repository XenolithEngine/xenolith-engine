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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDDATADEVICE_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDDATADEVICE_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/ref.h>
#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/runtime/utils/stack_buffer.h>
#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/clipboard.h>
#include <sprt/runtime/window/interface.h>
#include <sprt/runtime/window/drop.h>

#include <wayland-client.h>

namespace sprt::window {

struct WaylandDisplay;
struct WaylandDataDevice;
struct WaylandSeat;

class WaylandWindow;

class WaylandLibrary;

struct SPRT_API WaylandDataDeviceManager : public Ref {
	virtual ~WaylandDataDeviceManager();

	bool init(NotNull<WaylandDisplay>, wl_registry *, uint32_t name, uint32_t version);

	Rc<WaylandLibrary> wayland;
	WaylandDisplay *root;
	wl_data_device_manager *manager = nullptr;
};

struct SPRT_API WaylandDataOffer : public Ref {
	virtual ~WaylandDataOffer();

	bool init(NotNull<WaylandLibrary>, wl_data_offer *);

	Rc<WaylandLibrary> wayland;
	wl_data_offer *offer = nullptr;

	uint32_t actions = 0;
	uint32_t selectedAction = 0;

	uint32_t serial = 0;
	struct wl_surface *surface = nullptr;
	wl_fixed_t x = 0;
	wl_fixed_t y = 0;

	Vector<String> types;
};

struct SPRT_API WaylandDataInputTransfer : public Ref {
	virtual ~WaylandDataInputTransfer();

	bool init(StringView type, NotNull<WaylandDataOffer>, Rc<ClipboardRequest> &&req);

	void schedule(NotNull<dispatch::Looper>);
	void commit();
	void cancel();

	String type;
	Rc<WaylandDataOffer> offer;
	Rc<ClipboardRequest> request;
	int pipefd[2] = {-1, -1};
	StackBuffer<256_KiB> buffer;
	Rc<dispatch::Handle> handle;
	Vector<Bytes> chunks;
	size_t receivedSize = 0; // running total, capped at MaxClipboardTransferSize
};

struct SPRT_API WaylandDataOutputTransfer : public Ref {
	static void create(Bytes &&, int fd);

	virtual ~WaylandDataOutputTransfer();

	bool init(Bytes &&, int fd);

	bool write();

	Bytes data;
	size_t offset = 0;
	size_t blockSize = 64_KiB;
	int targetFd = -1;

	Rc<dispatch::Handle> handle;
};

struct SPRT_API WaylandDataSource : public Ref {
	virtual ~WaylandDataSource();

	bool init(NotNull<WaylandDataDevice>, Rc<ClipboardData> &&req);

	void send(StringView type, int32_t fd);
	void cancel();

	Rc<WaylandLibrary> wayland;
	WaylandDataDevice *device = nullptr;
	wl_data_source *source = nullptr;
	Rc<ClipboardData> data;
};

/** A drag from another client over one of our windows, answered through its wl_data_offer.

The offer outlives the drag on the device: after the drop the application may still read it, and
only finish() releases the proxy. */
class SPRT_API WaylandDropOffer : public DropOffer {
public:
	virtual ~WaylandDropOffer() = default;

	virtual bool init(NotNull<dispatch::Looper>, NotNull<WaylandDataDevice>,
			NotNull<WaylandDataOffer>);

protected:
	virtual void handleRead(StringView type, ReadCallback &&) override;
	virtual void handleStatus(DragActions) override;
	virtual void handleFinish(DragActions) override;

	void flush();

	Rc<WaylandDataDevice> _device;
	Rc<WaylandDataOffer> _offer;

	bool _statusSent = false;
	DragActions _sentStatus = DragActions::None;
};

struct SPRT_API WaylandDataDevice : public Ref {
	virtual ~WaylandDataDevice();

	bool init(NotNull<WaylandDataDeviceManager>, NotNull<WaylandSeat>);

	void setSelection(NotNull<WaylandDataOffer>);
	void clearSelection();

	// `window` is null when the surface is not one of our windows
	void enter(NotNull<WaylandDataOffer>, WaylandWindow *window);
	void motion(wl_fixed_t x, wl_fixed_t y);
	void leave();
	void drop();

	// The window is going away: a drag over it reports nothing more
	void clearWindow(WaylandWindow *);

	Status readFromClipboard(Rc<ClipboardRequest> &&req);
	Status probeClipboard(Rc<ClipboardProbe> &&probe);
	Status writeToClipboard(Rc<ClipboardData> &&);

	WaylandLibrary *wayland = nullptr;
	WaylandDataDeviceManager *manager = nullptr;
	WaylandSeat *seat = nullptr;
	wl_data_device *device = nullptr;

	// An offer the compositor announced but has not yet used for a selection or a drag. Replaced
	// by the next announcement, which destroys an offer nothing claimed
	Rc<WaylandDataOffer> pendingOffer;

	Rc<WaylandDataOffer> selectionOffer;
	Rc<WaylandDataSource> selectionSource;
	Rc<WaylandDataOffer> dnd;

	// The drag over one of our windows, reported to that window
	Rc<WaylandDropOffer> dropOffer;
	WaylandWindow *dropWindow = nullptr;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDDATADEVICE_H_
