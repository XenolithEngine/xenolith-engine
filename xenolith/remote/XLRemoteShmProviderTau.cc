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

// The Tau provider shares one client-owned Frame through tau.wm@1 and rings Notifications.

#include "XLRemoteShmProvider.h"

#if SPRT_TAU

#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/cxx/__atomic/ops.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

static constexpr size_t kShmTauMaxBlock = 2u << 20;

class TauShmBlock : public ShmBlockMemory {
public:
	virtual ~TauShmBlock() {
		if (_block) {
			__sprt_tau_wm_release(_block);
		}
	}

	bool init(__sprt_tau_wm_block *block) {
		struct __sprt_tau_wm_view view;
		if (__sprt_tau_wm_view(block, &view) < 0) {
			return false;
		}
		_block = block;
		_data = static_cast<uint8_t *>(view.data);
		_size = view.size;
		_peer = view.peer;
		_wake = view.wake;
		return true;
	}

	virtual bool isPeerAlive() override { return __sprt_tau_wm_alive(_block) != 0; }
	virtual bool isDataAlive() override { return __sprt_tau_wm_data_alive(_block) != 0; }
	virtual void ringPeer() override { __sprt_tau_wm_ring(_block); }

	virtual TransportWaitAddress getWaitAddress() override {
		return {_wake, sprt::_atomic::loadSeq(_wake)};
	}

	uint64_t getPeer() const { return _peer; }

protected:
	__sprt_tau_wm_block *_block = nullptr;
	uint64_t _peer = 0;
	uint32_t *_wake = nullptr;
};

class TauShmListener : public ShmListenerBackend {
public:
	virtual ~TauShmListener() { close(); }

	bool init(size_t maxBlock) {
		_open = __sprt_tau_wm_listen(sprt::min(maxBlock, kShmTauMaxBlock)) == 0;
		return _open;
	}

	virtual TransportWaitAddress getWaitAddress() override {
		auto epoch = __sprt_tau_wm_pending();
		return TransportWaitAddress{epoch, sprt::_atomic::loadSeq(epoch)};
	}

	virtual void accept(const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &cb) override {
		for (unsigned i = 0; _open && i < 8; ++i) {
			__sprt_tau_wm_block *block = nullptr;
			if (__sprt_tau_wm_accept(&block) != 1) {
				break;
			}
			auto memory = Rc<TauShmBlock>::create(block);
			if (!memory) {
				__sprt_tau_wm_release(block);
			}
			if (memory) {
				PeerIdentity peer;
				peer.authenticated = true;
				peer.pid = int64_t(memory->getPeer());
				peer.description = toString("shm:tau-module=", peer.pid);
				cb(Rc<ShmBlockMemory>(memory.get()), sp::move(peer));
			}
		}
	}

	virtual void close() override {
		if (_open) {
			__sprt_tau_wm_stop();
			_open = false;
		}
	}

protected:
	bool _open = false;
};

class TauShmProvider : public ShmProvider {
public:
	virtual Rc<ShmListenerBackend> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		if (addr.path != "tau.wm") {
			return nullptr;
		}
		return Rc<TauShmListener>::create(cfg.shmMaxBlockSize);
	}

	virtual Rc<ShmBlockMemory> connect(const Address &addr, const ShmBlockConfig &config,
			PeerIdentity &peer) override {
		if (addr.path != "tau.wm") {
			return nullptr;
		}
		auto size = ShmBlock::computeSize(config);
		__sprt_tau_wm_block *block = nullptr;
		auto created = size ? __sprt_tau_wm_create(size, &block) : -1;
		if (created != 0) {
			log::source().error("remote::shm", "Tau block allocation: ", size, " bytes, error ",
					created);
			return nullptr;
		}
		auto memory = Rc<TauShmBlock>::create(block);
		if (!memory) {
			__sprt_tau_wm_release(block);
			return nullptr;
		}
		if (ShmBlock::format(memory->getData(), memory->getSize(), config) != Status::Ok) {
			log::source().error("remote::shm", "Tau block format failed");
			return nullptr;
		}
		auto posted = __sprt_tau_wm_post(block);
		if (posted != 0) {
			if (posted != -__SPRT_TAU_ENOENT && posted != -__SPRT_TAU_ECONNRESET
					&& posted != -__SPRT_TAU_EBUSY) {
				log::source().error("remote::shm", "Tau block post failed: ", posted);
			}
			return nullptr;
		}
		peer.authenticated = true;
		peer.description = "shm:tau.wm (root registry)";
		return memory;
	}
};

} // namespace

ShmProvider *getPlatformShmProvider() {
	static TauShmProvider s_provider;
	return &s_provider;
}

} // namespace stappler::xenolith::remote

#endif
