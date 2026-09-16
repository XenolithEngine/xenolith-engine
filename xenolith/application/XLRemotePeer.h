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

#ifndef XENOLITH_APPLICATION_XLREMOTEPEER_H_
#define XENOLITH_APPLICATION_XLREMOTEPEER_H_

#include "XLCommon.h"
#include "XLRemoteProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;

// The far end of one connection, as the per-connection helpers (block transfer, font endpoint)
// see it. A client has one, its AppThread; a server has one per connected client, a RemoteSession.
// Everything here runs on the peer thread.
class SP_PUBLIC RemotePeer {
public:
	using ReplyCallback = Function<void(const remote::MessageHeader &, BytesView payload)>;

	virtual ~RemotePeer() = default;

	// The thread the connection lives on.
	virtual AppThread *getPeerThread() const = 0;

	// False when there is no open connection to send over.
	virtual bool remoteSendCbor(remote::Domain, uint8_t code, const Value &,
			uint32_t *outSerial = nullptr) = 0;
	virtual bool remoteSendRaw(remote::Domain, uint8_t code, BytesView,
			uint32_t *outSerial = nullptr) = 0;
	virtual bool remoteSendCborReply(uint32_t serial, remote::Domain, uint8_t code,
			const Value &) = 0;
	virtual bool remoteSendError(remote::Domain, uint8_t code, uint32_t serial) = 0;

	// `timeoutUs` is the relative reply deadline, 0 for none; an expired waiter is completed with a
	// local error and the connection is dropped.
	virtual bool remoteSendCborWithReply(remote::Domain, uint8_t code, const Value &,
			ReplyCallback &&, uint64_t timeoutUs) = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEPEER_H_ */
