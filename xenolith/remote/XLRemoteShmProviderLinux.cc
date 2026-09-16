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

/* The Linux provider for `shm:<path>`: blocks are tmpfs files both processes map.
 *
 * The listener owns a small rendezvous block at the address path. A client creates its connection
 * block next to it as `<path>.<token>`, formats it and posts the token into a free slot; the server
 * opens that file, unlinks it and attaches. Nothing but the token travels through the slot, so the
 * server never opens a path a client chose.
 *
 * Both files must belong to this uid, which is also the peer identity. A peer of the same uid can
 * still truncate a mapped file and fault the other side; such a peer can attach a debugger anyway.
 */

#include "XLRemoteShmProvider.h"

#if SPRT_LINUX

#include <sprt/cxx/__atomic/ops.h>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/cross/__sprt_syscall.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __SPRT_SYSCALL_pidfd_open
__SPRT_C_FUNC long int syscall(long int __sysno, ...);
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

static constexpr uint32_t kShmListenMagic = 0x584C'534C; // 'XLSL'
static constexpr uint16_t kShmListenVersion = 1;
static constexpr uint16_t kShmListenSlotCount = 16;
static constexpr uint32_t kShmMaxAcceptsPerPump = 8;
static constexpr size_t kShmTokenLength = 32;

enum class ShmSlotState : uint32_t {
	Free = 0,
	Claimed = 1, // a client took the slot and is filling it
	Requested = 2, // the token is complete, the server may accept
	Accepting = 3, // the server is opening the connection block
};

struct ShmListenSlot {
	uint32_t state;
	int32_t clientPid;
	char token[kShmTokenLength + 1];
	uint8_t reserved[7];
};

struct ShmListenHeader {
	uint32_t magic;
	uint16_t version;
	uint16_t slotCount;
	int32_t serverPid;
	uint32_t doorbell; // a client increments it after posting a request
	uint32_t waiting;
	uint8_t reserved[44];
};

static_assert(sizeof(ShmListenSlot) == 48);
static_assert(sizeof(ShmListenHeader) == 64);

static constexpr size_t kShmListenBlockSize =
		sizeof(ShmListenHeader) + kShmListenSlotCount * sizeof(ShmListenSlot);

static ShmListenSlot *shmListenSlots(ShmListenHeader *header) {
	return reinterpret_cast<ShmListenSlot *>(header + 1);
}

static bool shmIsToken(const char *token) {
	for (size_t i = 0; i < kShmTokenLength; ++i) {
		auto c = token[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
			return false;
		}
	}
	return token[kShmTokenLength] == 0;
}

static String shmConnectionPath(StringView listenPath, const char *token) {
	return toString(listenPath, ".", StringView(token, kShmTokenLength));
}

static bool shmIsProcessAlive(int32_t pid) {
	return pid > 0 && (::kill(pid, 0) == 0 || errno != ESRCH);
}

static int shmOpenPidfd(int32_t pid) {
#ifdef __SPRT_SYSCALL_pidfd_open
	if (pid > 0) {
		return int(::syscall(__SPRT_SYSCALL_pidfd_open, pid, 0));
	}
#endif
	return -1;
}

// Map a whole file shared and close the descriptor: the mapping keeps the file alive.
static uint8_t *shmMapFile(int fd, size_t size) {
	auto mem = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	::close(fd);
	return mem == MAP_FAILED ? nullptr : static_cast<uint8_t *>(mem);
}

class LinuxShmBlock : public ShmBlockMemory {
public:
	virtual ~LinuxShmBlock();

	// Takes the mapping over, including on failure.
	bool init(uint8_t *mem, size_t size, int32_t peerPid, StringView ownedPath) {
		_data = mem;
		_size = size;
		_peerPid = peerPid;
		_pidfd = shmOpenPidfd(peerPid);
		_ownedPath = ownedPath.str<Interface>();
		return _data != nullptr;
	}

	virtual bool isPeerAlive() override {
		if (_pidfd >= 0) {
			// A pidfd reads as ready once the process has exited.
			struct pollfd pfd{_pidfd, POLLIN, 0};
			return ::poll(&pfd, 1, 0) <= 0;
		}
		return _peerPid <= 0 || shmIsProcessAlive(_peerPid);
	}

	virtual void handleLocalClose() override {
		if (!_ownedPath.empty()) {
			::unlink(_ownedPath.data());
			_ownedPath.clear();
		}
	}

protected:
	int32_t _peerPid = -1;
	int _pidfd = -1;
	String _ownedPath; // a client's connection block, removed if the server never took it
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

LinuxShmBlock::~LinuxShmBlock() {
	handleLocalClose();
	if (_data) {
		::munmap(_data, _size);
		_data = nullptr;
	}
	if (_pidfd >= 0) {
		::close(_pidfd);
		_pidfd = -1;
	}
}

__SPRT_POP_ALLOW_CXXABI_ALLOC

class LinuxShmListener : public ShmListenerBackend {
public:
	virtual ~LinuxShmListener();

	Status open(const Address &, const TransportServerConfig &);

	virtual TransportWaitAddress getWaitAddress() override {
		if (!_header || _closed) {
			return TransportWaitAddress();
		}
		return TransportWaitAddress{&_header->doorbell, sprt::_atomic::loadSeq(&_header->doorbell)};
	}

	virtual void accept(const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &) override;

	virtual void close() override;

protected:
	void acceptSlot(ShmListenSlot &,
			const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &);

	ShmListenHeader *_header = nullptr;
	String _path;
	uint32_t _maxBlockSize = 0;
	bool _closed = false;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

// Like a connection block, the mapping stays until the destructor: a wait on the doorbell may be
// armed.
LinuxShmListener::~LinuxShmListener() {
	close();
	if (_header) {
		::munmap(_header, kShmListenBlockSize);
		_header = nullptr;
	}
}

__SPRT_POP_ALLOW_CXXABI_ALLOC

Status LinuxShmListener::open(const Address &addr, const TransportServerConfig &cfg) {
	if (addr.path.empty()) {
		log::source().error("remote::shm", "a rendezvous path is required: shm:<path>");
		return Status::ErrorInvalidArguemnt;
	}

	// A crashed server leaves its rendezvous file behind; two live servers must not share a path.
	::unlink(addr.path.data());

	int fd = ::open(addr.path.data(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW,
			mode_t(cfg.socketMode));
	if (fd < 0) {
		log::source().error("remote::shm", "open(", addr.path, ") failed: ", errno);
		return Status::ErrorNotPermitted;
	}

	if (::posix_fallocate(fd, 0, off_t(kShmListenBlockSize)) != 0) {
		log::source().error("remote::shm", "posix_fallocate(", addr.path, ") failed");
		::close(fd);
		::unlink(addr.path.data());
		return Status::ErrorNotPermitted;
	}

	auto mem = shmMapFile(fd, kShmListenBlockSize);
	if (!mem) {
		log::source().error("remote::shm", "mmap(", addr.path, ") failed: ", errno);
		::unlink(addr.path.data());
		return Status::ErrorNotPermitted;
	}

	auto header = new (mem) ShmListenHeader();
	header->version = kShmListenVersion;
	header->slotCount = kShmListenSlotCount;
	header->serverPid = int32_t(::getpid());
	// Last, so a client never accepts a half-written header.
	header->magic = kShmListenMagic;

	_header = header;
	_path = addr.path;
	_maxBlockSize = cfg.shmMaxBlockSize;
	return Status::Ok;
}

void LinuxShmListener::close() {
	if (_closed) {
		return;
	}
	_closed = true;
	if (_header) {
		_header->magic = 0;
	}
	if (!_path.empty()) {
		::unlink(_path.data());
		_path.clear();
	}
}

void LinuxShmListener::accept(const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &cb) {
	if (!_header || _closed) {
		return;
	}

	auto slots = shmListenSlots(_header);
	uint32_t taken = 0;
	for (uint32_t i = 0; i < kShmListenSlotCount && taken < kShmMaxAcceptsPerPump; ++i) {
		auto &slot = slots[i];
		auto state = ShmSlotState(sprt::_atomic::loadSeq(&slot.state));
		if (state == ShmSlotState::Requested) {
			auto expected = toInt(ShmSlotState::Requested);
			if (!sprt::_atomic::compareSwap(&slot.state, &expected,
						toInt(ShmSlotState::Accepting))) {
				continue;
			}
			++taken;
			acceptSlot(slot, cb);
			sprt::_atomic::storeSeq(&slot.state, toInt(ShmSlotState::Free));
		} else if (state == ShmSlotState::Claimed) {
			// A client that died between claiming and posting would hold the slot forever.
			auto pid = slot.clientPid;
			if (pid > 0 && !shmIsProcessAlive(pid)) {
				auto expected = toInt(ShmSlotState::Claimed);
				sprt::_atomic::compareSwap(&slot.state, &expected, toInt(ShmSlotState::Free));
			}
		}
	}
}

void LinuxShmListener::acceptSlot(ShmListenSlot &slot,
		const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &cb) {
	char token[kShmTokenLength + 1];
	__sprt_memcpy(token, slot.token, sizeof(token));
	token[kShmTokenLength] = 0;
	auto pid = slot.clientPid;

	if (!shmIsToken(token)) {
		log::source().warn("remote::shm", "a connection request carries a malformed token");
		return;
	}

	auto path = shmConnectionPath(_path, token);
	int fd = ::open(path.data(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0) {
		return; // the client withdrew before we got here
	}
	::unlink(path.data());

	struct stat st;
	if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != ::getuid()
			|| st.st_size < off_t(sizeof(ShmBlockHeader))
			|| uint64_t(st.st_size) > uint64_t(_maxBlockSize)) {
		log::source().warn("remote::shm", "refusing a connection block that is not ours or ",
				"exceeds ", _maxBlockSize, " bytes");
		::close(fd);
		return;
	}

	auto size = size_t(st.st_size);
	auto mem = shmMapFile(fd, size);
	if (!mem) {
		return;
	}

	auto block = Rc<LinuxShmBlock>::create(mem, size, pid, StringView());
	if (!block) {
		return;
	}

	PeerIdentity peer;
	peer.authenticated = true;
	peer.uid = int64_t(st.st_uid);
	peer.gid = int64_t(st.st_gid);
	peer.pid = int64_t(pid);
	peer.description = toString("shm:uid=", peer.uid, " pid=", pid, " (declared)");

	cb(Rc<ShmBlockMemory>(block.get()), sp::move(peer));
}

class LinuxShmProvider : public ShmProvider {
public:
	virtual ~LinuxShmProvider() = default;

	virtual Rc<ShmListenerBackend> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		auto l = Rc<LinuxShmListener>::create();
		if (!l || l->open(addr, cfg) != Status::Ok) {
			return nullptr;
		}
		return Rc<ShmListenerBackend>(l.get());
	}

	virtual Rc<ShmBlockMemory> connect(const Address &, const ShmBlockConfig &,
			PeerIdentity &) override;
};

Rc<ShmBlockMemory> LinuxShmProvider::connect(const Address &addr, const ShmBlockConfig &config,
		PeerIdentity &peer) {
	auto blockSize = ShmBlock::computeSize(config);

	int lfd = ::open(addr.path.data(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	if (lfd < 0) {
		log::source().error("remote::shm", "no listener at ", addr.description(), ": ", errno);
		return nullptr;
	}

	struct stat lst;
	if (::fstat(lfd, &lst) != 0 || !S_ISREG(lst.st_mode) || lst.st_uid != ::getuid()
			|| lst.st_size != off_t(kShmListenBlockSize)) {
		log::source().error("remote::shm", addr.description(), " is not a rendezvous block");
		::close(lfd);
		return nullptr;
	}

	auto listenMem = shmMapFile(lfd, kShmListenBlockSize);
	if (!listenMem) {
		return nullptr;
	}

	auto header = reinterpret_cast<ShmListenHeader *>(listenMem);
	ShmListenHeader desc;
	__sprt_memcpy(&desc, header, sizeof(ShmListenHeader));
	if (desc.magic != kShmListenMagic || desc.version != kShmListenVersion
			|| desc.slotCount != kShmListenSlotCount) {
		log::source().error("remote::shm", addr.description(), " is not a rendezvous block");
		::munmap(listenMem, kShmListenBlockSize);
		return nullptr;
	}

	uint8_t raw[kShmTokenLength / 2];
	char token[kShmTokenLength + 1];
	if (::getrandom(raw, sizeof(raw), 0) != ssize_t(sizeof(raw))) {
		::munmap(listenMem, kShmListenBlockSize);
		return nullptr;
	}
	static constexpr const char *s_hex = "0123456789abcdef";
	for (size_t i = 0; i < sizeof(raw); ++i) {
		token[i * 2] = s_hex[raw[i] >> 4];
		token[i * 2 + 1] = s_hex[raw[i] & 0xF];
	}
	token[kShmTokenLength] = 0;

	auto path = shmConnectionPath(addr.path, token);
	int cfd = ::open(path.data(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, mode_t(0600));
	if (cfd < 0) {
		log::source().error("remote::shm", "open(", path, ") failed: ", errno);
		::munmap(listenMem, kShmListenBlockSize);
		return nullptr;
	}

	// Allocate up front: a page tmpfs can not back would fault on first write, not fail here.
	uint8_t *blockMem = nullptr;
	if (::posix_fallocate(cfd, 0, off_t(blockSize)) == 0) {
		blockMem = shmMapFile(cfd, blockSize);
	} else {
		::close(cfd);
	}

	Rc<LinuxShmBlock> block;
	if (blockMem) {
		block = Rc<LinuxShmBlock>::create(blockMem, blockSize, desc.serverPid, path);
	}
	if (!block || ShmBlock::format(blockMem, blockSize, config) != Status::Ok) {
		log::source().error("remote::shm", "failed to create a ", blockSize, "-byte block at ",
				path);
		if (blockMem && !block) {
			::munmap(blockMem, blockSize);
		}
		::unlink(path.data());
		::munmap(listenMem, kShmListenBlockSize);
		return nullptr;
	}

	bool posted = false;
	auto slots = shmListenSlots(header);
	for (uint32_t i = 0; i < kShmListenSlotCount; ++i) {
		auto &slot = slots[i];
		auto expected = toInt(ShmSlotState::Free);
		if (sprt::_atomic::compareSwap(&slot.state, &expected, toInt(ShmSlotState::Claimed))) {
			slot.clientPid = int32_t(::getpid());
			__sprt_memcpy(slot.token, token, sizeof(token));
			sprt::_atomic::storeSeq(&slot.state, toInt(ShmSlotState::Requested));
			sprt::_atomic::fetchAdd(&header->doorbell, uint32_t(1));
			__sprt_sprt_qlock_wake_all(&header->doorbell, __SPRT_SPRT_LOCK_FLAG_SHARED);
			posted = true;
			break;
		}
	}
	::munmap(listenMem, kShmListenBlockSize);

	if (!posted) {
		log::source().error("remote::shm", "listener at ", addr.description(), " has no free slot");
		return nullptr; // the block's destructor removes its file
	}

	peer.authenticated = true;
	peer.uid = int64_t(lst.st_uid);
	peer.gid = int64_t(lst.st_gid);
	peer.pid = int64_t(desc.serverPid);
	peer.description = toString("shm:uid=", peer.uid, " pid=", desc.serverPid);
	return Rc<ShmBlockMemory>(block.get());
}

} // namespace

ShmProvider *getPlatformShmProvider() {
	static LinuxShmProvider s_provider;
	return &s_provider;
}

} // namespace stappler::xenolith::remote

#endif
