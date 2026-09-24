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

#define __SPRT_BUILD 1

#include "SPRTWinLinuxWaylandDataDevice.h"
#include "SPRTWinLinuxWaylandLibrary.h"
#include "SPRTWinLinuxWaylandDisplay.h"
#include "SPRTWinLinuxWaylandSeat.h"
#include "SPRTWinLinuxWaylandWindow.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace sprt::window {

// clang-format off

static struct wl_data_source_listener s_dataSourceListener{
	.target = [](void *data, struct wl_data_source *wl_data_source, const char *mime_type) {

	},

	.send = [](void *data, struct wl_data_source *wl_data_source, const char *mime_type, int32_t fd) {
		auto source = reinterpret_cast<WaylandDataSource *>(data);
		source->send(StringView(mime_type), fd);
	},

	.cancelled = [](void *data, struct wl_data_source *wl_data_source) {
		auto source = reinterpret_cast<WaylandDataSource *>(data);
		source->cancel();
	},

	.dnd_drop_performed = [](void *data, struct wl_data_source *wl_data_source) {

	},

	.dnd_finished = [](void *data, struct wl_data_source *wl_data_source) {

	},

	.action = [](void *data, struct wl_data_source *wl_data_source, uint32_t dnd_action) {

	}
};

static struct wl_data_offer_listener s_dataOfferListener {
	.offer = [](void *data, struct wl_data_offer *wl_data_offer, const char *mime_type) {
		reinterpret_cast<WaylandDataOffer *>(data)->types.emplace_back(mime_type);
		//log::source().debug("WaylandDataDevice", "offer: ", mime_type);
	},

	.source_actions = [](void *data, struct wl_data_offer *wl_data_offer, uint32_t source_actions) {
		reinterpret_cast<WaylandDataOffer *>(data)->actions = source_actions;
	},

	.action = [](void *data, struct wl_data_offer *wl_data_offer, uint32_t dnd_action) {
		reinterpret_cast<WaylandDataOffer *>(data)->selectedAction = dnd_action;
	}
};

static struct wl_data_device_listener s_dataDeviceListener{
	.data_offer = [](void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		device->pendingOffer = Rc<WaylandDataOffer>::create(device->wayland, id);
	},

	.enter = [](void *data, struct wl_data_device *wl_data_device, uint32_t serial, struct wl_surface *surface,
		wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		if (!id) {
			// A drag with no data source, e.g. one confined to another client
			device->leave();
			return;
		}

		auto offer = reinterpret_cast<WaylandDataOffer *>(
				wl_data_offer_get_user_data(id));

		offer->serial = serial;
		offer->surface = surface;
		offer->x = x;
		offer->y = y;

		WaylandWindow *window = nullptr;
		if (surface && !device->seat->root->isDecoration(surface)
				&& device->wayland->ownsProxy(surface)) {
			window = reinterpret_cast<WaylandWindow *>(wl_surface_get_user_data(surface));
		}

		device->enter(offer, window);
	},

	.leave = [](void *data, struct wl_data_device *wl_data_device) {
		reinterpret_cast<WaylandDataDevice *>(data)->leave();
	},

	.motion = [](void *data, struct wl_data_device *wl_data_device,
			uint32_t time, wl_fixed_t x, wl_fixed_t y) {
		reinterpret_cast<WaylandDataDevice *>(data)->motion(x, y);
	},

	.drop = [](void *data, struct wl_data_device *wl_data_device) {
		reinterpret_cast<WaylandDataDevice *>(data)->drop();
	},

	.selection = [](void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		if (id) {
			auto offer = reinterpret_cast<WaylandDataOffer *>(
					wl_data_offer_get_user_data(id));
			device->setSelection(offer);
		} else {
			device->clearSelection();
		}
	}
};

// clang-format on

WaylandDataDeviceManager::~WaylandDataDeviceManager() {
	if (manager) {
		wl_data_device_manager_destroy(manager);
		manager = nullptr;
	}
}

bool WaylandDataDeviceManager::init(NotNull<WaylandDisplay> disp, wl_registry *registry,
		uint32_t name, uint32_t version) {
	root = disp;
	wayland = root->wayland;
	manager = static_cast<struct wl_data_device_manager *>(
			wl_registry_bind(registry, name, &wl_data_device_manager_interface, version));
	wl_data_device_manager_set_user_data(manager, this);
	wl_proxy_set_tag((struct wl_proxy *)manager, &s_XenolithWaylandTag);
	return true;
}

WaylandDataOffer::~WaylandDataOffer() {
	if (offer) {
		wl_data_offer_destroy(offer);
		offer = nullptr;
	}
}

bool WaylandDataOffer::init(NotNull<WaylandLibrary> w, wl_data_offer *o) {
	wayland = w;
	offer = o;
	wl_data_offer_add_listener(offer, &s_dataOfferListener, this);
	wl_data_offer_set_user_data(offer, this);
	return true;
}

WaylandDataInputTransfer::~WaylandDataInputTransfer() {
	if (pipefd[0] != -1) {
		::close(pipefd[0]);
		pipefd[0] = -1;
	}
	if (pipefd[1] != -1) {
		::close(pipefd[1]);
		pipefd[1] = -1;
	}
}

bool WaylandDataInputTransfer::init(StringView t, NotNull<WaylandDataOffer> o,
		Rc<ClipboardRequest> &&req) {
	type = t.str<String>();
	offer = o;
	request = sprt::move(req);

	if (::pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
		wl_data_offer_receive(offer->offer, type.data(), pipefd[1]);
		return true;
	}
	return false;
}

void WaylandDataInputTransfer::schedule(NotNull<dispatch::Looper> looper) {
	if (pipefd[1] != -1) {
		::close(pipefd[1]);
		pipefd[1] = -1;
	}
	handle = looper->listenPollableHandle(pipefd[0],
			filesystem::PollFlags::In | filesystem::PollFlags::HungUp,
			[this](native_handle fd, filesystem::PollFlags flags) {
		if (hasFlag(flags, filesystem::PollFlags::In)) {
			ssize_t bytesRead = 0;
			do {
				buffer.soft_clear();
				size_t emptySpace = buffer.capacity();
				auto ptr = buffer.prepare(emptySpace);

				bytesRead = ::read(pipefd[0], ptr, emptySpace);
				if (bytesRead > 0) {
					receivedSize += size_t(bytesRead);
					if (receivedSize > MaxClipboardTransferSize) {
						// oversized/hostile selection source — abort the transfer
						cancel();
						return Status::Done;
					}
					buffer.save(ptr, bytesRead);
					chunks.emplace_back(Bytes(buffer.data(), buffer.data() + buffer.size()));
				}
			} while (bytesRead > 0);

			if (bytesRead < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return Status::Ok;
				} else {
					cancel();
					return Status::Done;
				}
			}
		}
		if (hasFlag(flags, filesystem::PollFlags::Err)) {
			cancel();
			return Status::Done;
		}
		if (hasFlag(flags, filesystem::PollFlags::HungUp)) {
			commit();
			return Status::Done;
		}
		return Status::Ok;
	}, this);
}

void WaylandDataInputTransfer::commit() {
	size_t outBufferSize = 0;
	for (auto &it : chunks) { outBufferSize += it.size(); }

	Bytes data;
	data.resize(outBufferSize);
	outBufferSize = 0;
	for (auto &it : chunks) {
		::__sprt_memcpy(data.data() + outBufferSize, it.data(), it.size());
		outBufferSize += it.size();
	}

	if (request && request->dataCallback) {
		request->dataCallback(Status::Ok, data, type);
	}

	request = nullptr;
	handle = nullptr;
}

void WaylandDataInputTransfer::cancel() {
	chunks.clear();
	if (request && request->dataCallback) {
		request->dataCallback(Status::ErrorCancelled, BytesView(), StringView());
	}
	handle = nullptr;
}

void WaylandDataOutputTransfer::create(Bytes &&d, int fd) {
	auto obj = __new<WaylandDataOutputTransfer>();
	obj->init(sprt::move(d), fd);
	sprt::release(obj, 0);
}

WaylandDataOutputTransfer::~WaylandDataOutputTransfer() {
	if (targetFd != -1) {
		::close(targetFd);
		targetFd = -1;
	}
}

bool WaylandDataOutputTransfer::init(Bytes &&d, int fd) {
	data = sprt::move(d);
	targetFd = fd;

	::fcntl(targetFd, F_SETFL, (fcntl(targetFd, F_GETFL) | O_NONBLOCK));

	blockSize = ::fcntl(targetFd, F_GETPIPE_SZ);

	auto success = write();

	if (offset == data.size()) {
		::close(targetFd);
		return true;
	} else if (!success && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		// make handle to wait for other end
		auto looper = dispatch::Looper::getIfExists();
		if (looper) {
			handle = looper->listenPollableHandle(targetFd, filesystem::PollFlags::Out,
					[this](native_handle fd, filesystem::PollFlags flags) {
				if (hasFlag(flags, filesystem::PollFlags::Out)) {
					write();
					if (offset == data.size()) {
						if (targetFd != -1) {
							::close(targetFd);
							targetFd = -1;
						}
						return Status::Done;
					}
				}
				if (hasFlag(flags, filesystem::PollFlags::Err)) {
					if (targetFd != -1) {
						::close(targetFd);
						targetFd = -1;
					}
					return Status::Done;
				}
				return Status::Ok;
			}, this);
		}
	}
	return true;
}

bool WaylandDataOutputTransfer::write() {
	ssize_t bytesWritten = 0;
	do {
		auto targetSize = sprt::min(data.size() - offset, blockSize);
		bytesWritten = ::write(targetFd, data.data() + offset, targetSize);
		if (bytesWritten > 0) {
			offset += bytesWritten;
		}
	} while (bytesWritten > 0 && offset < data.size());

	if (bytesWritten < 0) {
		return false;
	}
	return true;
}

WaylandDataSource::~WaylandDataSource() {
	if (source) {
		wl_data_source_destroy(source);
		source = nullptr;
	}
}

bool WaylandDataSource::init(NotNull<WaylandDataDevice> dev, Rc<ClipboardData> &&d) {
	wayland = dev->wayland;
	device = dev;
	data = sprt::move(d);

	source = wl_data_device_manager_create_data_source(device->manager->manager);
	wl_data_source_add_listener(source, &s_dataSourceListener, this);

	for (auto &it : data->types) { wl_data_source_offer(source, it.data()); }

	return true;
}

void WaylandDataSource::send(StringView type, int32_t fd) {
	auto it = sprt::find(data->types.begin(), data->types.end(), type);
	if (it == data->types.end()) {
		::close(fd);
		return;
	}

	auto bytes = data->encodeCallback(type);
	if (bytes.empty()) {
		::close(fd);
		return;
	}

	WaylandDataOutputTransfer::create(sprt::move(bytes), fd);
}

void WaylandDataSource::cancel() {
	if (device && device->selectionSource == this) {
		device->selectionSource = nullptr;
	}
}

static DragActions WaylandDropOffer_readActions(uint32_t actions) {
	auto ret = DragActions::None;
	if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
		ret |= DragActions::Copy;
	}
	if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
		ret |= DragActions::Move;
	}
	return ret;
}

static uint32_t WaylandDropOffer_writeAction(DragActions action) {
	switch (action) {
	case DragActions::Copy: return WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
	case DragActions::Move: return WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
	default: break;
	}
	return WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
}

bool WaylandDropOffer::init(NotNull<dispatch::Looper> looper, NotNull<WaylandDataDevice> device,
		NotNull<WaylandDataOffer> offer) {
	// Before version 3 a drag carries no actions at all, and means a copy
	auto allowed = WaylandDropOffer_readActions(offer->actions);
	if (wl_data_offer_get_version(offer->offer) < 3) {
		allowed = DragActions::Copy;
	}

	if (!DropOffer::init(looper, Vector<String>(offer->types), allowed)) {
		return false;
	}

	_device = device;
	_offer = offer;
	return true;
}

void WaylandDropOffer::handleRead(StringView type, ReadCallback &&cb) {
	if (!_offer) {
		cb(Status::ErrorCancelled, BytesView());
		return;
	}

	auto req = Rc<ClipboardRequest>::create();
	req->dataCallback = [cb](Status st, BytesView data, StringView) { cb(st, data); };

	auto transfer = Rc<WaylandDataInputTransfer>::create(type, _offer, sprt::move(req));
	if (!transfer) {
		cb(Status::ErrorUnknown, BytesView());
		return;
	}

	flush();
	transfer->schedule(_looper);
}

void WaylandDropOffer::handleStatus(DragActions action) {
	if (!_offer || (_statusSent && _sentStatus == action)) {
		return;
	}

	_statusSent = true;
	_sentStatus = action;

	// The compositor only delivers a drop the client accepted with a type
	const char *type = nullptr;
	if (action != DragActions::None && !_types.empty()) {
		type = _types.front().data();
	}
	wl_data_offer_accept(_offer->offer, _offer->serial, type);

	// Only the resolved action is offered: a compositor choosing another one on a modifier would
	// let the source delete what the target only copied
	if (wl_data_offer_get_version(_offer->offer) >= WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
		auto wlAction = WaylandDropOffer_writeAction(action);
		wl_data_offer_set_actions(_offer->offer, wlAction, wlAction);
	}

	flush();
}

void WaylandDropOffer::handleFinish(DragActions performed) {
	// finish() is a protocol error unless the compositor has settled on an action; without it the
	// source learns the outcome from the offer being destroyed
	if (_offer && performed != DragActions::None && _offer->selectedAction != 0
			&& wl_data_offer_get_version(_offer->offer) >= WL_DATA_OFFER_FINISH_SINCE_VERSION) {
		wl_data_offer_finish(_offer->offer);
		flush();
	}

	_offer = nullptr;
	_device = nullptr;
}

void WaylandDropOffer::flush() {
	if (_device && _device->seat) {
		wl_display_flush(_device->seat->root->display);
	}
}

WaylandDataDevice::~WaylandDataDevice() {
	dropOffer = nullptr;
	dropWindow = nullptr;
	pendingOffer = nullptr;
	selectionOffer = nullptr;
	dnd = nullptr;

	if (selectionSource) {
		selectionSource->device = nullptr;
		selectionSource = nullptr;
	}

	if (device) {
		wl_data_device_release(device);
		device = nullptr;
	}

	manager = nullptr;
}

bool WaylandDataDevice::init(NotNull<WaylandDataDeviceManager> m, NotNull<WaylandSeat> s) {
	wayland = m->wayland;
	seat = s;
	manager = m;

	device = wl_data_device_manager_get_data_device(manager->manager, seat->seat);

	wl_data_device_add_listener(device, &s_dataDeviceListener, this);

	return true;
}

void WaylandDataDevice::setSelection(NotNull<WaylandDataOffer> offer) {
	// The pending reference may be the only one: hold the offer before letting it go
	Rc<WaylandDataOffer> held(offer.get());
	if (held == pendingOffer) {
		pendingOffer = nullptr;
	}
	if (offer != selectionOffer) {
		selectionOffer = offer;
		seat->root->handleClipboardChanged();
	}
}

void WaylandDataDevice::clearSelection() {
	if (selectionOffer) {
		selectionOffer = nullptr;
		seat->root->handleClipboardChanged();
	}
}

void WaylandDataDevice::enter(NotNull<WaylandDataOffer> offer, WaylandWindow *window) {
	// As in setSelection: the offer outlives the pending reference it is taken from
	Rc<WaylandDataOffer> held(offer.get());
	if (held == pendingOffer) {
		pendingOffer = nullptr;
	}

	if (dropOffer && dropOffer->isDropped()) {
		// The drag that dropped lives on with the application until it is finished
		dropOffer = nullptr;
		dropWindow = nullptr;
	} else if (dropOffer && offer != dnd) {
		leave();
	}

	dnd = offer;

	if (!window || dropOffer) {
		return;
	}

	auto looper = dispatch::Looper::getIfExists();
	if (!looper) {
		return;
	}

	dropOffer = Rc<WaylandDropOffer>::create(looper, this, offer);
	dropWindow = window;
	if (dropOffer) {
		dropWindow->emitDropEvent(DropPhase::Enter, dropOffer, offer->x, offer->y);
	}
}

void WaylandDataDevice::motion(wl_fixed_t x, wl_fixed_t y) {
	if (dnd) {
		dnd->x = x;
		dnd->y = y;
	}
	if (dropOffer && dropWindow && !dropOffer->isDropped()) {
		dropWindow->emitDropEvent(DropPhase::Motion, dropOffer, x, y);
	}
}

void WaylandDataDevice::leave() {
	// A compositor sends leave after a drop as well; that drag is already over for the window
	if (dropOffer && dropWindow && !dropOffer->isDropped()) {
		dropWindow->emitDropEvent(DropPhase::Leave, dropOffer, dnd ? dnd->x : 0, dnd ? dnd->y : 0);
	}
	dropOffer = nullptr;
	dropWindow = nullptr;
	dnd = nullptr;
}

void WaylandDataDevice::drop() {
	if (dropOffer && dropWindow && !dropOffer->isDropped()) {
		dropWindow->emitDropEvent(DropPhase::Drop, dropOffer, dnd ? dnd->x : 0, dnd ? dnd->y : 0);
	}
}

void WaylandDataDevice::clearWindow(WaylandWindow *window) {
	if (dropWindow == window) {
		if (dropOffer && !dropOffer->isDropped()) {
			dropOffer->refuse(DropPhase::Motion);
		}
		dropWindow = nullptr;
	}
}

Status WaylandDataDevice::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (!selectionOffer) {
		return Status::Declined;
	}

	Vector<StringView> dataList;
	for (auto &it : selectionOffer->types) { dataList.emplace_back(it); }
	auto selectedType = req->typeCallback(dataList);
	if (sprt::find(dataList.begin(), dataList.end(), selectedType) == dataList.end()) {
		return Status::ErrorInvalidArguemnt;
	}

	auto transfer =
			Rc<WaylandDataInputTransfer>::create(selectedType, selectionOffer, sprt::move(req));
	if (transfer) {
		wl_display_flush(seat->root->display);
		transfer->schedule(dispatch::Looper::getIfExists());
		return Status::Ok;
	}

	return Status::ErrorNotImplemented;
}

Status WaylandDataDevice::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (!selectionOffer) {
		return Status::Declined;
	}

	Vector<StringView> dataList;
	for (auto &it : selectionOffer->types) { dataList.emplace_back(it); }

	probe->typeCallback(Status::Ok, dataList);

	return Status::Ok;
}

Status WaylandDataDevice::writeToClipboard(Rc<ClipboardData> &&data) {
	auto source = Rc<WaylandDataSource>::create(this, sprt::move(data));

	wl_data_device_set_selection(device, source->source, seat->serial);
	selectionSource = source;

	return Status::Ok;
}

} // namespace sprt::window
