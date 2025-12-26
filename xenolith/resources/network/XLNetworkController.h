/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RESOURCES_NETWORK_XLNETWORKCONTROLLER_H_
#define XENOLITH_RESOURCES_NETWORK_XLNETWORKCONTROLLER_H_

#include "XLAppThread.h"
#include "SPNetworkHandle.h"
#include "SPThreadTaskQueue.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::network {

class Request;

class SP_PUBLIC Controller final : public ApplicationExtension {
public:
	static Rc<ApplicationExtension> createController(AppThread *, StringView,
			Bytes &&signKey = Bytes());

	Controller(AppThread *, StringView, Bytes &&signKey = Bytes());
	virtual ~Controller();

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	virtual void update(AppThread *, const UpdateTime &t, bool wakeup) override;

	virtual void handleNetworkStateChanged(NetworkFlags) override;

	AppThread *getApplication() const;
	StringView getName() const;

	void run(Rc<Request> &&);

	void setSignKey(Bytes &&value);

	bool isNetworkOnline() const;

protected:
	struct Data;

	Data *_data;
};

} // namespace stappler::xenolith::network

#endif /* XENOLITH_RESOURCES_NETWORK_XLNETWORKCONTROLLER_H_ */
