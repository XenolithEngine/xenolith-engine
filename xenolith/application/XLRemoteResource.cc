#include "XLClientAppThread.h"
#include "XLRemoteSession.h"
#include "XLServerAppThread.h"
#include "XLRemoteSerialize.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static constexpr uint64_t RemoteResourceMaxBytes = remote::QueueCodec::ResourceUploadMaxBytes;
static constexpr uint64_t RemoteResourceSessionBytes = 32 * 1'024 * 1'024;

const core::ImageData *RemoteSession::resolveResourceImage(uint64_t id) const {
	for (auto &resource : _resources) {
		for (auto image : resource->getImages()) {
			if (image->image && _host->getSharedObjects()->get(image->image.get()) == id) {
				return image;
			}
		}
	}
	return nullptr;
}

void ClientAppThread::compileRemoteResource(Rc<core::Resource> &&res,
		Function<void(bool)> &&callback) {
	log::source().debug("RemoteResource", "compile ", res ? res->getName() : StringView());
	if (!callback) {
		callback = [](bool) { };
	}
	if (_windows.empty() || !res || !res->getBuffers().empty() || res->getImages().empty()
			|| res->getImages().size() > 16) {
		callback(false);
		return;
	}
	Value request;
	request.setInteger(int64_t(_windows.begin()->first), "window");
	request.setString(res->getName(), "name");
	Value images(Value::Type::ARRAY);
	uint64_t total = 0;
	for (auto image : res->getImages()) {
		auto &e = image->extent;
		uint64_t size = uint64_t(e.width) * e.height * 4;
		if (image->format != core::ImageFormat::R8G8B8A8_UNORM || !e.width || !e.height
				|| e.width > 4'096 || e.height > 4'096 || e.depth != 1
				|| image->arrayLayers.get() != 1 || image->mipLevels.get() != 1
				|| size > RemoteResourceMaxBytes - total) {
			callback(false);
			return;
		}
		total += size;
		Bytes pixels(size);
		if (image->writeData(pixels.data(), pixels.size()) != size) {
			callback(false);
			return;
		}
		Value value;
		value.setString(image->key, "key");
		value.setInteger(e.width, "width");
		value.setInteger(e.height, "height");
		value.setBool(sp::hasFlag(image->hints, core::ImageHints::Opaque), "opaque");
		value.setBytes(sp::move(pixels), "pixels");
		images.addValue(sp::move(value));
	}
	request.setValue(sp::move(images), "images");
	auto cb = sp::move(callback);
	if (!sendMessageWithReply(remote::Domain::Window, toInt(remote::WindowCode::CompileResource),
				request,
				[this, res = sp::move(res), cb](const remote::MessageHeader &h, BytesView bytes) {
		if (remote::isError(h)) {
			cb(false);
			return;
		}
		auto mirror = remote::QueueCodec::decodeResource(bytes, *_sharedObjects);
		if (!mirror || mirror->getImages().size() != res->getImages().size()) {
			cb(false);
			return;
		}
		for (auto image : res->getImages()) {
			auto compiled = mirror->getImage(image->key);
			if (!compiled || !compiled->image) {
				cb(false);
				return;
			}
			image->image = compiled->image;
			_sharedObjects->registerImageData(remote::getRemoteObjectId(*image->image), image);
		}
		res->setCompiled(true);
		_remoteResources.emplace_back(res);
		log::source().debug("RemoteResource", "compiled ", res->getName());
		cb(true);
	}, 120'000'000)) {
		cb(false);
	}
}

void RemoteSession::compileResource(const remote::MessageHeader &header, BytesView payload) {
	auto refuse = [&] {
		remoteSendError(remote::Domain::Window, toInt(remote::WindowError::SerializationFailed),
				header.serial);
	};
	if (payload.size() > RemoteResourceMaxBytes + 4'096 || _resources.size() >= 32) {
		refuse();
		return;
	}
	auto value = data::read<Interface>(payload);
	auto id = uint64_t(value.getInteger("window"));
	auto registry = _host->getSharedObjects();
	if (!registry) {
		refuse();
		return;
	}
	auto window = registry->getWindows().find(id);
	if (window == registry->getWindows().end()
			|| (window->second.ownerSession != _id && window->second.creatorSession != _id)) {
		remoteSendError(remote::Domain::Window, toInt(remote::WindowError::InvalidObjecthandle),
				header.serial);
		return;
	}
	uint64_t total = 0;
	auto resource = remote::QueueCodec::decodeResourceUpload(value,
			RemoteResourceSessionBytes - _resourceBytes, total);
	if (!resource || !_host->getGlLoop()) {
		refuse();
		return;
	}
	_resourceBytes += total;
	_resources.emplace_back(resource);
	_host->getGlLoop()->compileResource(Rc<core::Resource>(resource),
			[host = Rc<ServerAppThread>(_host), session = Rc<RemoteSession>(this), resource,
					serial = header.serial](bool success) {
		host->performOnAppThread([host, session, resource, serial, success] {
			if (session->isClosed()) {
				return;
			}
			if (!success) {
				session->remoteSendError(remote::Domain::Window,
						toInt(remote::WindowError::SerializationFailed), serial);
				return;
			}
			auto bytes = remote::QueueCodec::encodeResource(*resource, *host->getSharedObjects());
			session->getConnection()->sendReply(serial, remote::Domain::Window,
					toInt(remote::WindowCode::CompileResource), bytes);
		}, session);
	});
}

} // namespace stappler::xenolith
