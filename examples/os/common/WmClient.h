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


#ifndef EXAMPLES_OS_COMMON_WMCLIENT_H_
#define EXAMPLES_OS_COMMON_WMCLIENT_H_

#include "WmProtocol.h"
#include "XLClientAppThread.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

/* A client scene's line to the window manager: the AppMessage channel of its ClientAppThread.
In --connect mode nothing can install a handler before the scene exists, so a client asks for the
state it needs when it enters, and the server's later notifications arrive through the handler. */
class WmClient : public Ref {
public:
	using Handler = Function<void(Value &&)>;

	virtual ~WmClient() = default;

	bool init(Director *dir) {
		_thread = dir ? dynamic_cast<ClientAppThread *>(dir->getApplication()) : nullptr;
		return _thread != nullptr;
	}

	// Notifications from the server; a request from it is refused, the protocol has none.
	void setHandler(Handler &&handler) {
		_thread->setAppMessageHandler(
				[handler = sp::move(handler)](Value &&value, Rc<AppReply> &&reply) {
			if (reply) {
				reply->refuse();
			} else if (handler) {
				handler(sp::move(value));
			}
		});
	}

	bool notify(const Value &value) { return _thread->sendAppNotification(value); }

	bool request(const Value &value, Function<void(Status, Value &&)> &&cb,
			uint64_t timeoutUs = 5'000'000) {
		return _thread->sendAppRequest(value, sp::move(cb), timeoutUs);
	}

protected:
	Rc<ClientAppThread> _thread;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_COMMON_WMCLIENT_H_ */
