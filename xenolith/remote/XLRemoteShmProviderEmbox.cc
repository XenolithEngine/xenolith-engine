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

/* The Embox providers for `shm:<name>`, the engine half of XLRemoteShmEmbox.h.
 *
 * SPRT_EMBOX (the window server in the kernel): listens through the ops table a kernel module
 * registers. SPRT_EMBOX_USER (a client task): connects through /dev/wm.
 *
 * The SPRT_EMBOX_USER half has never been compiled: there is no embox-user toolchain yet, and the
 * kernel has neither /dev/wm nor a generic mmap of a device descriptor.
 */

#include "XLRemoteShmProvider.h"
#include "XLRemoteShmEmbox.h"

#if SPRT_EMBOX

#include <sprt/cxx/__atomic/ops.h>
#include <errno.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

static constexpr uint32_t kShmKernelMaxAcceptsPerPump = 8;

static const struct xl_shm_kernel_ops *volatile s_kernelOps = nullptr;

static const struct xl_shm_kernel_ops *shmKernelOps() {
	return __atomic_load_n(&s_kernelOps, __ATOMIC_SEQ_CST);
}

class KernelShmBlock : public ShmBlockMemory {
public:
	virtual ~KernelShmBlock() {
		if (_ops && _conn) {
			_ops->release(_conn);
		}
	}

	bool init(const struct xl_shm_kernel_ops *ops, const struct xl_shm_accept &accepted) {
		_ops = ops;
		_conn = accepted.conn;
		_data = static_cast<uint8_t *>(accepted.block);
		_size = accepted.size;
		return _data != nullptr;
	}

	// The kernel sets the dead side's closed bit and rings us; nothing to poll.

protected:
	const struct xl_shm_kernel_ops *_ops = nullptr;
	void *_conn = nullptr;
};

class KernelShmListener : public ShmListenerBackend {
public:
	virtual ~KernelShmListener() { close(); }

	bool init(const struct xl_shm_kernel_ops *ops, StringView name, size_t maxBlockSize) {
		_ops = ops;
		_maxBlockSize = maxBlockSize;
		_listener = _ops->listen(name.str<Interface>().data(), maxBlockSize);
		if (!_listener) {
			log::source().error("remote::shm", "the kernel refused to listen on ", name);
			return false;
		}
		return true;
	}

	virtual TransportWaitAddress getWaitAddress() override {
		if (!_listener) {
			return TransportWaitAddress();
		}
		auto doorbell = _ops->listener_doorbell(_listener);
		return doorbell ? TransportWaitAddress{doorbell, sprt::_atomic::loadSeq(doorbell)}
						: TransportWaitAddress();
	}

	virtual void accept(const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &cb) override {
		for (uint32_t i = 0; _listener && i < kShmKernelMaxAcceptsPerPump; ++i) {
			struct xl_shm_accept accepted = {};
			if (_ops->accept(_listener, &accepted) != 0) {
				break;
			}
			auto block = Rc<KernelShmBlock>::create(_ops, accepted);
			if (!block) {
				if (accepted.conn) {
					_ops->release(accepted.conn);
				}
				continue;
			}
			if (accepted.size > _maxBlockSize) {
				log::source().warn("remote::shm", "the kernel handed over a block above the limit");
				continue;
			}

			PeerIdentity peer;
			peer.authenticated = true; // the kernel knows the task that posted it
			peer.pid = accepted.peer_task;
			peer.description = toString("shm:task=", accepted.peer_task);
			cb(Rc<ShmBlockMemory>(block.get()), sp::move(peer));
		}
	}

	virtual void close() override {
		if (_listener) {
			_ops->close_listener(_listener);
			_listener = nullptr;
		}
	}

protected:
	const struct xl_shm_kernel_ops *_ops = nullptr;
	void *_listener = nullptr;
	size_t _maxBlockSize = 0;
};

class KernelShmProvider : public ShmProvider {
public:
	virtual ~KernelShmProvider() = default;

	virtual Rc<ShmListenerBackend> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		auto ops = shmKernelOps();
		if (!ops) {
			log::source().error("remote::shm", "the kernel block provider is not registered; ",
					addr.description(), " is unavailable");
			return nullptr;
		}
		auto l = Rc<KernelShmListener>::create(ops, StringView(addr.path), cfg.shmMaxBlockSize);
		return l ? Rc<ShmListenerBackend>(l.get()) : nullptr;
	}

	virtual Rc<ShmBlockMemory> connect(const Address &addr, const ShmBlockConfig &,
			PeerIdentity &) override {
		log::source().error("remote::shm", "clients of ", addr.description(),
				" run in user tasks; the kernel side only listens");
		return nullptr;
	}
};

static void shmSetKernelOps(const struct xl_shm_kernel_ops *ops) {
	if (ops && ops->abi_version != XL_SHM_KERNEL_ABI_VERSION) {
		log::source().error("remote::shm", "kernel block provider ABI ", ops->abi_version,
				" is not ", XL_SHM_KERNEL_ABI_VERSION, "; ignored");
		return;
	}
	__atomic_store_n(&s_kernelOps, ops, __ATOMIC_SEQ_CST);
}

} // namespace

ShmProvider *getPlatformShmProvider() {
	static KernelShmProvider s_provider;
	return &s_provider;
}

} // namespace stappler::xenolith::remote

void xl_remote_shm_set_kernel_ops(const struct xl_shm_kernel_ops *ops) {
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::remote::shmSetKernelOps(ops);
}

#elif SPRT_EMBOX_USER

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

static constexpr const char *s_wmDevice = "/dev/wm";

class WmClientBlock : public ShmBlockMemory {
public:
	virtual ~WmClientBlock() {
		if (_data) {
			::munmap(_data, _size);
			_data = nullptr;
		}
		if (_fd >= 0) {
			::close(_fd);
			_fd = -1;
		}
	}

	// Takes the descriptor over, including on failure.
	bool init(int fd, size_t size) {
		_fd = fd;
		auto mem = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mem == MAP_FAILED) {
			log::source().error("remote::shm", "mmap(", s_wmDevice, ") failed: ", errno);
			return false;
		}
		_data = static_cast<uint8_t *>(mem);
		_size = size;
		return true;
	}

	int getFd() const { return _fd; }

protected:
	int _fd = -1;
};

class WmClientProvider : public ShmProvider {
public:
	virtual ~WmClientProvider() = default;

	virtual Rc<ShmListenerBackend> listen(const Address &addr,
			const TransportServerConfig &) override {
		log::source().error("remote::shm", "a user task can not listen on ", addr.description(),
				"; the window server runs in the kernel");
		return nullptr;
	}

	virtual Rc<ShmBlockMemory> connect(const Address &addr, const ShmBlockConfig &config,
			PeerIdentity &peer) override {
		struct xl_wm_connect req = {};
		if (addr.path.size() >= sizeof(req.name)) {
			log::source().error("remote::shm", "listener name is too long: ", addr.path);
			return nullptr;
		}
		__sprt_memcpy(req.name, addr.path.data(), addr.path.size());
		req.size = ShmBlock::computeSize(config);

		int fd = ::open(s_wmDevice, O_RDWR | O_CLOEXEC);
		if (fd < 0) {
			log::source().error("remote::shm", "open(", s_wmDevice, ") failed: ", errno);
			return nullptr;
		}
		if (::ioctl(fd, XL_WM_IOC_CONNECT, &req) != 0) {
			log::source().error("remote::shm", "no listener ", addr.path, ": ", errno);
			::close(fd);
			return nullptr;
		}

		auto block = Rc<WmClientBlock>::create(fd, size_t(req.size));
		if (!block || ShmBlock::format(block->getData(), block->getSize(), config) != Status::Ok) {
			return nullptr;
		}
		if (::ioctl(block->getFd(), XL_WM_IOC_POST, 0) != 0) {
			log::source().error("remote::shm", "posting the block to ", addr.path,
					" failed: ", errno);
			return nullptr;
		}

		peer.authenticated = true;
		peer.pid = req.server_task;
		peer.description = toString("shm:task=", req.server_task, " (kernel)");
		return Rc<ShmBlockMemory>(block.get());
	}
};

} // namespace

ShmProvider *getPlatformShmProvider() {
	static WmClientProvider s_provider;
	return &s_provider;
}

} // namespace stappler::xenolith::remote

#endif
