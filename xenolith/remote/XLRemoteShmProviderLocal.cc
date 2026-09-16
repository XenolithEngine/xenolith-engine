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

/* The in-process provider for `shm:@name`: a block on the heap handed to a listener in the same
 * process, typically on another thread. Both sides hold the same memory object, so it lives until
 * the last of them lets go. It exercises the whole transport where no platform provider exists
 * yet, the Embox kernel among them.
 */

#include "XLRemoteShmProvider.h"

#if SPRT_LINUX || SPRT_EMBOX_ANY

#include <sprt/cxx/__atomic/ops.h>
#include <sprt/cxx/mutex>
#include <sprt/c/sys/__sprt_sprt.h>

#include <unistd.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

static constexpr size_t kShmLocalMaxPending = 16;
static constexpr uint32_t kShmLocalMaxAcceptsPerPump = 8;

class LocalShmBlock : public ShmBlockMemory {
public:
	virtual ~LocalShmBlock() = default;

	bool init(size_t size) {
		_storage.resize(size);
		_data = _storage.data();
		_size = size;
		return true;
	}

protected:
	Bytes _storage;
};

class LocalShmListener;

struct LocalShmRegistry {
	sprt::mutex mutex;
	Map<String, LocalShmListener *> listeners;

	static LocalShmRegistry &get() {
		static LocalShmRegistry s_registry;
		return s_registry;
	}
};

static PeerIdentity shmLocalIdentity(StringView name) {
	PeerIdentity peer;
	peer.authenticated = true; // the same process on both sides
	peer.pid = int64_t(::getpid());
	peer.description = toString("shm:@", name, " (in-process)");
	return peer;
}

class LocalShmListener : public ShmListenerBackend {
public:
	virtual ~LocalShmListener();

	bool init(StringView name) {
		auto &registry = LocalShmRegistry::get();
		sprt::unique_lock lock(registry.mutex);
		if (registry.listeners.find(name.str<Interface>()) != registry.listeners.end()) {
			log::source().error("remote::shm", "shm:@", name, " is already bound");
			return false;
		}
		_name = name.str<Interface>();
		registry.listeners.emplace(_name, this);
		return true;
	}

	virtual TransportWaitAddress getWaitAddress() override {
		return TransportWaitAddress{&_doorbell, sprt::_atomic::loadSeq(&_doorbell)};
	}

	virtual void accept(const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &cb) override {
		Vector<Rc<LocalShmBlock>> taken;
		{
			sprt::unique_lock lock(_mutex);
			auto n = sprt::min(size_t(kShmLocalMaxAcceptsPerPump), _pending.size());
			for (size_t i = 0; i < n; ++i) { taken.emplace_back(sp::move(_pending[i])); }
			_pending.erase(_pending.begin(), _pending.begin() + n);
		}
		for (auto &it : taken) { cb(Rc<ShmBlockMemory>(it.get()), shmLocalIdentity(_name)); }
	}

	virtual void close() override {
		auto &registry = LocalShmRegistry::get();
		sprt::unique_lock lock(registry.mutex);
		auto it = registry.listeners.find(_name);
		if (it != registry.listeners.end() && it->second == this) {
			registry.listeners.erase(it);
		}
		sprt::unique_lock listenerLock(_mutex);
		_closed = true;
		_pending.clear();
	}

	// Called with the registry locked, which is what keeps this listener alive meanwhile.
	bool post(Rc<LocalShmBlock> &&block) {
		{
			sprt::unique_lock lock(_mutex);
			if (_closed || _pending.size() >= kShmLocalMaxPending) {
				return false;
			}
			_pending.emplace_back(sp::move(block));
		}
		sprt::_atomic::fetchAdd(&_doorbell, uint32_t(1));
		__sprt_sprt_qlock_wake_all(&_doorbell, __SPRT_SPRT_LOCK_FLAG_SHARED);
		return true;
	}

protected:
	String _name;
	sprt::mutex _mutex;
	Vector<Rc<LocalShmBlock>> _pending;
	uint32_t _doorbell = 0;
	bool _closed = false;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

LocalShmListener::~LocalShmListener() { close(); }

__SPRT_POP_ALLOW_CXXABI_ALLOC

class LocalShmProvider : public ShmProvider {
public:
	virtual ~LocalShmProvider() = default;

	virtual Rc<ShmListenerBackend> listen(const Address &addr,
			const TransportServerConfig &) override {
		auto name = StringView(addr.path).sub(1);
		if (name.empty()) {
			log::source().error("remote::shm", "an endpoint name is required: shm:@<name>");
			return nullptr;
		}
		auto l = Rc<LocalShmListener>::create(name);
		return l ? Rc<ShmListenerBackend>(l.get()) : nullptr;
	}

	virtual Rc<ShmBlockMemory> connect(const Address &addr, const ShmBlockConfig &config,
			PeerIdentity &peer) override {
		auto name = StringView(addr.path).sub(1);
		auto size = ShmBlock::computeSize(config);
		auto block = Rc<LocalShmBlock>::create(size);
		if (!block || ShmBlock::format(block->getData(), size, config) != Status::Ok) {
			return nullptr;
		}

		auto &registry = LocalShmRegistry::get();
		sprt::unique_lock lock(registry.mutex);
		auto it = registry.listeners.find(name.str<Interface>());
		if (it == registry.listeners.end()) {
			log::source().error("remote::shm", "no endpoint bound at ", addr.description());
			return nullptr;
		}
		if (!it->second->post(Rc<LocalShmBlock>(block))) {
			log::source().error("remote::shm", addr.description(), " has no room for a request");
			return nullptr;
		}
		peer = shmLocalIdentity(name);
		return Rc<ShmBlockMemory>(block.get());
	}
};

} // namespace

ShmProvider *getLocalShmProvider() {
	static LocalShmProvider s_provider;
	return &s_provider;
}

} // namespace stappler::xenolith::remote

#endif
