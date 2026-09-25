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

#include "XLRemotePeer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

AppReply::~AppReply() {
	if (!_answered) {
		refuse();
	}
}

bool AppReply::init(Ref *owner, RemotePeer *peer, uint32_t serial) {
	_owner = owner;
	_peer = peer;
	_serial = serial;
	return _peer != nullptr;
}

bool AppReply::send(const Value &val) {
	if (_answered) {
		return false;
	}
	_answered = true;
	return _peer->remoteSendCborReply(_serial, remote::Domain::Global,
			toInt(remote::GlobalCode::AppRequest), val);
}

bool AppReply::refuse(remote::GlobalError err) {
	if (_answered) {
		return false;
	}
	_answered = true;
	return _peer->remoteSendError(remote::Domain::Global, toInt(err), _serial);
}

Status getAppReplyStatus(const remote::MessageHeader &h) {
	if (!remote::isError(h)) {
		return Status::Ok;
	}
	if (remote::Domain(h.domain) == remote::Domain::Error) {
		return Status::ErrorTimeout; // completed locally by ReplyTable::failExpired
	}
	if (remote::GlobalError(h.code) == remote::GlobalError::NotImplemented) {
		return Status::ErrorNotImplemented;
	}
	return Status::ErrorCancelled;
}

} // namespace stappler::xenolith
