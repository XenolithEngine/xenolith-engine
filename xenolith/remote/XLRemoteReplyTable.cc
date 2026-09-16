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

#include "XLRemoteReplyTable.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

void ReplyTable::wait(uint32_t serial, ReplyCallback &&cb, uint64_t deadlineUs, bool fatal) {
	_requests.insert_or_assign(serial, PendingReply{sp::move(cb), deadlineUs, fatal});
}

bool ReplyTable::dispatch(const MessageHeader &h, BytesView payload) {
	if (!isReplyOrError(h)) {
		return false;
	}
	auto it = _requests.find(h.serial);
	if (it == _requests.end()) {
		return false;
	}
	auto cb = sp::move(it->second.cb);
	_requests.erase(it);
	if (cb) {
		cb(h, payload);
	}
	return true;
}

bool ReplyTable::failExpired(uint64_t nowUs, MessageType errorType) {
	if (_requests.empty()) {
		return false;
	}

	// Collect expired serials first: a waiter's callback may register or erase requests.
	Vector<uint32_t> expired;
	for (auto &it : _requests) {
		if (it.second.deadline != 0 && nowUs >= it.second.deadline) {
			expired.emplace_back(it.first);
		}
	}
	if (expired.empty()) {
		return false;
	}

	bool fatalExpired = false;
	for (auto serial : expired) {
		auto it = _requests.find(serial);
		if (it == _requests.end()) {
			continue;
		}
		auto cb = sp::move(it->second.cb);
		auto fatal = it->second.fatal;
		_requests.erase(it);

		fatalExpired = fatalExpired || fatal;
		if (fatal) {
			log::source().warn("remote::ReplyTable", "request ", serial,
					" timed out without a reply; failing with local protocol error");
		}

		if (cb) {
			MessageHeader h{};
			h.msgtype = toInt(errorType);
			h.domain = toInt(Domain::Error);
			h.code = toInt(GlobalError::NetworkBackend);
			h.serial = serial;
			cb(h, BytesView());
		}
	}
	return fatalExpired;
}

} // namespace stappler::xenolith::remote
