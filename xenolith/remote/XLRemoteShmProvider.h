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

#ifndef XENOLITH_REMOTE_XLREMOTESHMPROVIDER_H_
#define XENOLITH_REMOTE_XLREMOTESHMPROVIDER_H_

#include "XLRemoteShmBlock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// How a `shm:` connection gets its block: the transport itself only attaches rings to memory it is
// handed. `shm:@name` goes to the in-process provider; any other path to the platform's own
// (tmpfs files on Linux, the kernel on Embox).

// Memory of one connection block, and what the platform knows about the peer holding the other
// side. Owned by the connection; released when the connection is destroyed, never on close, since
// a wait on the doorbell may still be armed.
class SP_PUBLIC ShmBlockMemory : public Ref {
public:
	virtual ~ShmBlockMemory() = default;

	uint8_t *getData() const { return _data; }
	size_t getSize() const { return _size; }

	// Polled from TransportConnection::handleEvents; false once the peer is known to be gone.
	virtual bool isPeerAlive() { return true; }

	// This side closed: withdraw whatever the platform still holds for the peer to find.
	virtual void handleLocalClose() { }

protected:
	uint8_t *_data = nullptr;
	size_t _size = 0;
};

// The platform half of a listener.
class SP_PUBLIC ShmListenerBackend : public Ref {
public:
	virtual ~ShmListenerBackend() = default;

	virtual TransportWaitAddress getWaitAddress() = 0;

	// Hand over the blocks clients have posted, a bounded number per call, each with the peer as
	// the platform knows it. The blocks are formatted by their clients and still untrusted.
	virtual void accept(const Callback<void(Rc<ShmBlockMemory> &&, PeerIdentity &&)> &) = 0;

	virtual void close() = 0;
};

class SP_PUBLIC ShmProvider {
public:
	virtual ~ShmProvider() = default;

	virtual Rc<ShmListenerBackend> listen(const Address &, const TransportServerConfig &) = 0;

	// Create a block, format it with `config` and post it to the listener. `peer` receives the
	// server as the platform knows it.
	virtual Rc<ShmBlockMemory> connect(const Address &, const ShmBlockConfig &config,
			PeerIdentity &peer) = 0;
};

// In-process rendezvous by name, available wherever the transport is built.
ShmProvider *getLocalShmProvider();

// The platform's provider for `shm:<path>`; nullptr where there is none.
ShmProvider *getPlatformShmProvider();

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTESHMPROVIDER_H_ */
