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

#ifndef XENOLITH_REMOTE_XLREMOTEREPLYTABLE_H_
#define XENOLITH_REMOTE_XLREMOTEREPLYTABLE_H_

#include "XLRemoteProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// Requests of one connection waiting for their replies, keyed by message serial. Serials are
// counted per connection, so a host with several connections keeps a table per connection.
class SP_PUBLIC ReplyTable {
public:
	using ReplyCallback = Function<void(const MessageHeader &, BytesView payload)>;

	// `deadlineUs` is absolute (monotonic clock); 0 means none.
	void wait(uint32_t serial, ReplyCallback &&, uint64_t deadlineUs);

	// Complete the waiter a reply or error belongs to. False when the message is not a reply this
	// table waits for.
	bool dispatch(const MessageHeader &, BytesView payload);

	// Complete every waiter past its deadline with a local error header of `errorType` (Domain::Error,
	// GlobalError::NetworkBackend). True when any expired; the caller then drops the connection.
	bool failExpired(uint64_t nowUs, MessageType errorType);

	// Drop every waiter without calling it.
	void clear() { _requests.clear(); }

	bool empty() const { return _requests.empty(); }
	size_t size() const { return _requests.size(); }

protected:
	struct PendingReply {
		ReplyCallback cb;
		uint64_t deadline = 0;
	};

	HashMap<uint32_t, PendingReply> _requests;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEREPLYTABLE_H_ */
