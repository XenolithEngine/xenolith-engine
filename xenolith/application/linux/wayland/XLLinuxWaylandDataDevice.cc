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

#include "XLLinuxWaylandDataDevice.h"
#include "XLLinuxWaylandDisplay.h"
#include "SPEvent.h"
#include "SPPlatformUnistd.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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
		auto offer = Rc<WaylandDataOffer>::create(device->wayland, id);
		offer->retain();
	},

	.enter = [](void *data, struct wl_data_device *wl_data_device, uint32_t serial, struct wl_surface *surface,
		wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		auto offer = reinterpret_cast<WaylandDataOffer *>(
				device->wayland->wl_data_offer_get_user_data(id));

		offer->serial = serial;
		offer->surface = surface;
		offer->x = x;
		offer->y = y;

		device->enter(offer);
		//log::source().debug("WaylandDataDevice", "enter");
	},

	.leave = [](void *data, struct wl_data_device *wl_data_device) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		device->leave();
		//log::source().debug("WaylandDataDevice", "leave");
	},

	.motion = [](void *data, struct wl_data_device *wl_data_device,
			uint32_t serial, wl_fixed_t x, wl_fixed_t y) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		if (device->dnd) {
			device->dnd->serial = serial;
			device->dnd->x = x;
			device->dnd->y = y;
		}
		//log::source().debug("WaylandDataDevice", "motion");
	},

	.drop = [](void *data, struct wl_data_device *wl_data_device) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		device->drop();
		//log::source().debug("WaylandDataDevice", "drop");
	},

	.selection = [](void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id) {
		auto device = reinterpret_cast<WaylandDataDevice *>(data);
		auto offer = reinterpret_cast<WaylandDataOffer *>(
				device->wayland->wl_data_offer_get_user_data(id));
		device->setSelection(offer);
	}
};

// clang-format on

WaylandDataDeviceManager::~WaylandDataDeviceManager() {
	if (manager) {
		wayland->wl_data_device_manager_destroy(manager);
		manager = nullptr;
	}
}

bool WaylandDataDeviceManager::init(NotNull<WaylandDisplay> disp, wl_registry *registry,
		uint32_t name, uint32_t version) {
	root = disp;
	wayland = root->wayland;
	manager = static_cast<struct wl_data_device_manager *>(
			wayland->wl_registry_bind(registry, name, wayland->wl_data_device_manager_interface,
					std::min(int(version), wayland->wl_data_device_manager_interface->version)));
	wayland->wl_data_device_manager_set_user_data(manager, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)manager, &s_XenolithWaylandTag);
	return true;
}

WaylandDataOffer::~WaylandDataOffer() { }

bool WaylandDataOffer::init(NotNull<WaylandLibrary> w, wl_data_offer *o) {
	wayland = w;
	offer = o;
	wayland->wl_data_offer_add_listener(offer, &s_dataOfferListener, this);
	wayland->wl_data_offer_set_user_data(offer, this);
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
	type = t.str<Interface>();
	offer = o;
	request = sp::move(req);

	if (::pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
		offer->wayland->wl_data_offer_receive(offer->offer, type.data(), pipefd[1]);
		return true;
	}
	return false;
}

void WaylandDataInputTransfer::schedule(NotNull<event::Looper> looper) {
	if (pipefd[1] != -1) {
		::close(pipefd[1]);
		pipefd[1] = -1;
	}
	handle =
			looper->listenPollableHandle(pipefd[0], event::PollFlags::In | event::PollFlags::HungUp,
					[this](event::NativeHandle fd, event::PollFlags flags) {
		if (hasFlag(flags, event::PollFlags::In)) {
			ssize_t bytesRead = 0;
			do {
				buffer.soft_clear();
				size_t emptySpace = buffer.capacity();
				auto ptr = buffer.prepare(emptySpace);

				auto bytesRead = ::read(fd, ptr, emptySpace);
				if (bytesRead > 0) {
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
		if (hasFlag(flags, event::PollFlags::Err)) {
			cancel();
			return Status::Done;
		}
		if (hasFlag(flags, event::PollFlags::HungUp)) {
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
	auto obj = new (std::nothrow) WaylandDataOutputTransfer;
	obj->init(sp::move(d), fd);
	obj->release(0);
}

WaylandDataOutputTransfer::~WaylandDataOutputTransfer() {
	if (targetFd != -1) {
		::close(targetFd);
		targetFd = -1;
	}
}

bool WaylandDataOutputTransfer::init(Bytes &&d, int fd) {
	data = sp::move(d);
	targetFd = fd;

	::fcntl(targetFd, F_SETFL, (fcntl(targetFd, F_GETFL) | O_NONBLOCK));

	blockSize = ::fcntl(targetFd, F_GETPIPE_SZ);

	auto success = write();

	if (offset == data.size()) {
		::close(targetFd);
		return true;
	} else if (!success && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		// make handle to wait for other end
		handle = event::Looper::getIfExists()->listenPollableHandle(targetFd, event::PollFlags::Out,
				[this](event::NativeHandle fd, event::PollFlags flags) {
			if (hasFlag(flags, event::PollFlags::Out)) {
				write();
				if (offset == data.size()) {
					if (targetFd != -1) {
						::close(targetFd);
						targetFd = -1;
					}
					return Status::Done;
				}
			}
			if (hasFlag(flags, event::PollFlags::Err)) {
				if (targetFd != -1) {
					::close(targetFd);
					targetFd = -1;
				}
				return Status::Done;
			}
			return Status::Ok;
		}, this);
	}

	return true;
}

bool WaylandDataOutputTransfer::write() {
	ssize_t bytesWritten = 0;
	do {
		auto targetSize = std::min(data.size() - offset, blockSize);
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
		wayland->wl_data_source_destroy(source);
		source = nullptr;
	}
}

bool WaylandDataSource::init(NotNull<WaylandDataDevice> device, Rc<ClipboardData> &&d) {
	wayland = device->wayland;
	data = sp::move(d);

	source = wayland->wl_data_device_manager_create_data_source(device->manager->manager);
	wayland->wl_data_source_add_listener(source, &s_dataSourceListener, this);

	for (auto &it : data->types) { wayland->wl_data_source_offer(source, it.data()); }

	return true;
}

void WaylandDataSource::send(StringView type, int32_t fd) {
	auto it = std::find(data->types.begin(), data->types.end(), type);
	if (it == data->types.end()) {
		::close(fd);
		return;
	}

	auto bytes = data->encodeCallback(type);
	if (bytes.empty()) {
		::close(fd);
		return;
	}

	WaylandDataOutputTransfer::create(sp::move(bytes), fd);
}

void WaylandDataSource::cancel() {
	if (device->selectionSource == this) {
		device->selectionSource = nullptr;
	}
}

WaylandDataDevice::~WaylandDataDevice() {
	if (device) {
		manager->wayland->wl_data_device_release(device);
		manager->wayland->wl_data_device_destroy(device);
		device = nullptr;
	}

	manager = nullptr;
}

bool WaylandDataDevice::init(NotNull<WaylandDataDeviceManager> m, NotNull<WaylandSeat> s) {
	wayland = m->wayland;
	seat = s;
	manager = m;

	device = wayland->wl_data_device_manager_get_data_device(manager->manager, seat->seat);

	wayland->wl_data_device_add_listener(device, &s_dataDeviceListener, this);

	return true;
}

void WaylandDataDevice::setSelection(NotNull<WaylandDataOffer> offer) {
	if (offer != selectionOffer) {
		selectionOffer = offer;
		if (!offer->attached) {
			offer->attached = true;
			offer->release(0);
		}
		seat->root->handleClipboardChanged();
	}
}

void WaylandDataDevice::enter(NotNull<WaylandDataOffer> offer) {
	if (offer != dnd) {
		dnd = offer;
		if (!offer->attached) {
			offer->attached = true;
			offer->release(0);
		}
	}
}

void WaylandDataDevice::leave() { dnd = nullptr; }

void WaylandDataDevice::drop() { dnd = nullptr; }

Status WaylandDataDevice::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (!selectionOffer) {
		return Status::Declined;
	}

	Vector<StringView> dataList;
	for (auto &it : selectionOffer->types) { dataList.emplace_back(it); }
	auto selectedType = req->typeCallback(dataList);
	if (std::find(dataList.begin(), dataList.end(), selectedType) == dataList.end()) {
		return Status::ErrorInvalidArguemnt;
	}

	auto transfer =
			Rc<WaylandDataInputTransfer>::create(selectedType, selectionOffer, sp::move(req));
	if (transfer) {
		wayland->wl_display_flush(seat->root->display);
		transfer->schedule(event::Looper::getIfExists());
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
	auto source = Rc<WaylandDataSource>::create(this, sp::move(data));

	wayland->wl_data_device_set_selection(device, source->source, seat->serial);
	selectionSource = source;

	return Status::Ok;
}

} // namespace stappler::xenolith::platform
