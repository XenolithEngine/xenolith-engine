/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLNetworkController.h"
#include "SPNetworkContext.h"
#include "XLEventListener.h"
#include "XLNetworkRequest.h"
#include "XLContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::network {

struct ControllerHandle {
	using Context = stappler::network::Context<Interface>;

	Rc<Request> request;
	Handle *handle;
	Context context;
};

struct Controller::Data final : thread::Thread {
	using Context = stappler::network::Context<Interface>;

	AppThread *_application = nullptr;
	Controller *_controller = nullptr;
	String _name;
	Bytes _signKey;

	Mutex _mutexQueue;
	Mutex _mutexFree;

	CURLM *_handle = nullptr;

	memory::PriorityQueue<Rc<Request>> _pending;

	Map<String, void *> _sharegroups;

	Map<CURL *, ControllerHandle> _handles;
	NetworkFlags _capabilities = NetworkFlags::None;

	Data(AppThread *app, Controller *c, StringView name, Bytes &&signKey);
	virtual ~Data();

	bool init();
	void invalidate();

	virtual void threadInit() override;
	virtual bool worker() override;
	virtual void threadDispose() override;

	void *getSharegroup(StringView);

	void handleNetworkStateChanged(NetworkFlags);

	void onUploadProgress(Handle *, int64_t total, int64_t now);
	void onDownloadProgress(Handle *, int64_t total, int64_t now);
	bool onComplete(Handle *, bool success);

	void sign(NetworkHandle &, Context &) const;

	void pushTask(Rc<Request> &&handle);
	void wakeup();

	bool prepare(Handle &handle, Context *ctx, const Callback<bool(CURL *)> &onBeforePerform);
	bool finalize(Handle &handle, Context *ctx, const Callback<bool(CURL *)> &onAfterPerform);
};

Controller::Data::Data(AppThread *app, Controller *c, StringView name, Bytes &&signKey)
: _application(app), _controller(c), _name(name.str<Interface>()), _signKey(sp::move(signKey)) { }

Controller::Data::~Data() { }

bool Controller::Data::init() {
	_capabilities = _application->getNetworkFlags();
	return true;
}

void Controller::Data::invalidate() { }

void Controller::Data::threadInit() {
	_pending.setQueueLocking(_mutexQueue);
	_pending.setFreeLocking(_mutexFree);

	thread::ThreadInfo::setThreadInfo(_name);

	_handle = curl_multi_init();

	Thread::threadInit();
}

bool Controller::Data::worker() {
	if (!_continueExecution.test_and_set()) {
		return false;
	}

	do {
		if (!_pending.pop_direct([&, this](memory::PriorityQueue<Rc<Handle>>::PriorityType type,
										 Rc<Request> &&it) {
			auto h = curl_easy_init();
			auto networkHandle = const_cast<Handle *>(&it->getHandle());
			auto i = _handles.emplace(h, ControllerHandle{move(it), networkHandle}).first;

			auto sg = i->second.handle->getSharegroup();
			if (!sg.empty()) {
				i->second.context.share = getSharegroup(sg);
			}

			i->second.context.userdata = this;
			i->second.context.curl = h;
			i->second.context.origHandle = networkHandle;

			i->second.context.origHandle->setDownloadProgress(
					[this, h = networkHandle](int64_t total, int64_t now) -> int {
				onDownloadProgress(h, total, now);
				return 0;
			});

			i->second.context.origHandle->setUploadProgress(
					[this, h = networkHandle](int64_t total, int64_t now) -> int {
				onUploadProgress(h, total, now);
				return 0;
			});

			if (i->second.handle->shouldSignRequest()) {
				sign(*networkHandle, i->second.context);
			}

			prepare(*networkHandle, &i->second.context, nullptr);

			curl_multi_add_handle(reinterpret_cast<CURLM *>(_handle), h);
		})) {
			break;
		}
	} while (0);

	int running = 0;
	auto err = curl_multi_perform(reinterpret_cast<CURLM *>(_handle), &running);
	if (err != CURLM_OK) {
		log::source().error("CURL", toString("Fail to perform multi: ", err));
		return false;
	}

	int timeout = 16;
	if (running == 0) {
		timeout = 1'000;
	}

	err = curl_multi_poll(reinterpret_cast<CURLM *>(_handle), NULL, 0, timeout, nullptr);
	if (err != CURLM_OK) {
		log::source().error("CURL", toString("Fail to poll multi: ", err));
		return false;
	}

	struct CURLMsg *msg = nullptr;
	do {
		int msgq = 0;
		msg = curl_multi_info_read(reinterpret_cast<CURLM *>(_handle), &msgq);
		if (msg && (msg->msg == CURLMSG_DONE)) {
			CURL *e = msg->easy_handle;
			curl_multi_remove_handle(reinterpret_cast<CURLM *>(_handle), e);

			auto it = _handles.find(e);
			if (it != _handles.end()) {
				it->second.context.code = msg->data.result;
				auto ret = finalize(*it->second.handle, &it->second.context, nullptr);
				if (!onComplete(it->second.handle, ret)) {
					_handles.erase(it);
					return false;
				}
				_handles.erase(it);
			}

			curl_easy_cleanup(e);
		}
	} while (msg);

	return true;
}

void Controller::Data::threadDispose() {
	if (_handle) {
		for (auto &it : _handles) {
			curl_multi_remove_handle(reinterpret_cast<CURLM *>(_handle), it.first);
			it.second.context.code = CURLE_FAILED_INIT;
			finalize(*it.second.handle, &it.second.context, nullptr);
			curl_easy_cleanup(it.first);
		}

		curl_multi_cleanup(reinterpret_cast<CURLM *>(_handle));

		for (auto &it : _sharegroups) { curl_share_cleanup((CURLSH *)it.second); }

		_handles.clear();
		_sharegroups.clear();

		_handle = nullptr;
	}

	Thread::threadDispose();
}

void *Controller::Data::getSharegroup(StringView name) {
	auto it = _sharegroups.find(name);
	if (it != _sharegroups.end()) {
		return it->second;
	}

	auto sharegroup = curl_share_init();
	curl_share_setopt(reinterpret_cast<CURLSH *>(sharegroup), CURLSHOPT_SHARE,
			CURL_LOCK_DATA_COOKIE);
	curl_share_setopt(reinterpret_cast<CURLSH *>(sharegroup), CURLSHOPT_SHARE,
			CURL_LOCK_DATA_SSL_SESSION);
	curl_share_setopt(reinterpret_cast<CURLSH *>(sharegroup), CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);

	_sharegroups.emplace(name.str<Interface>(), sharegroup);
	return sharegroup;
}

void Controller::Data::handleNetworkStateChanged(NetworkFlags caps) { _capabilities = caps; }

void Controller::Data::onUploadProgress(Handle *handle, int64_t total, int64_t now) {
	_application->performOnAppThread([handle, total, now] {
		auto req = handle->getReqeust();
		req->notifyOnUploadProgress(total, now);
	});
	_application->wakeup();
}

void Controller::Data::onDownloadProgress(Handle *handle, int64_t total, int64_t now) {
	_application->performOnAppThread([handle, total, now] {
		auto req = handle->getReqeust();
		req->notifyOnDownloadProgress(total, now);
	});
	_application->wakeup();
}

bool Controller::Data::onComplete(Handle *handle, bool success) {
	_application->performOnAppThread([handle, success] {
		auto req = handle->getReqeust();
		req->notifyOnComplete(success);
	});

	_application->wakeup();
	return true;
}

void Controller::Data::sign(NetworkHandle &handle, Context &ctx) const {
	String date = Time::now().toHttp<Interface>();

	auto appInfo = _application->getContext()->getInfo();

	auto msg = toString(handle.getUrl(), "\r\n", "X-ApplicationName: ", appInfo->bundleName, "\r\n",
			"X-ApplicationVersion: ", appInfo->appVersion, "\r\n", "X-ClientDate: ", date, "\r\n",
			"User-Agent: ", appInfo->userAgent, "\r\n");

	auto sig = string::Sha512::hmac(msg, _signKey);

	ctx.headers = curl_slist_append(ctx.headers, toString("X-ClientDate: ", date).data());
	ctx.headers = curl_slist_append(ctx.headers,
			toString("X-Stappler-Sign: ", base64url::encode<Interface>(sig)).data());

	if (!appInfo->userAgent.empty()) {
		handle.setUserAgent(appInfo->userAgent);
	}
}

void Controller::Data::pushTask(Rc<Request> &&handle) {
	_pending.push(0, false, move(handle));
	curl_multi_wakeup(_handle);
}

void Controller::Data::wakeup() { curl_multi_wakeup(_handle); }

bool Controller::Data::prepare(Handle &handle, Context *ctx,
		const Callback<bool(CURL *)> &onBeforePerform) {
	if (!handle.prepare(ctx)) {
		return false;
	}

	return stappler::network::prepare(*handle.getData(), ctx, onBeforePerform);
}

bool Controller::Data::finalize(Handle &handle, Context *ctx,
		const Callback<bool(CURL *)> &onAfterPerform) {
	auto ret = stappler::network::finalize(*handle.getData(), ctx, onAfterPerform);
	return handle.finalize(ctx, ret);
}

Rc<ApplicationExtension> Controller::createController(AppThread *app, StringView name,
		Bytes &&signKey) {
	return Rc<network::Controller>::alloc(app, name, sp::move(signKey));
}

Controller::Controller(AppThread *app, StringView name, Bytes &&signKey) {
	_data = new (std::nothrow) Data(app, this, name, sp::move(signKey));
	_data->init();
	_data->run();
}

Controller::~Controller() { }

void Controller::initialize(AppThread *) { }

void Controller::invalidate(AppThread *) {
	_data->stop();
	curl_multi_wakeup(_data->_handle);
	_data->waitStopped();
	delete _data;
	_data = nullptr;
}

void Controller::update(AppThread *, const UpdateTime &t, bool) { }

void Controller::handleNetworkStateChanged(NetworkFlags flags) { _data->_capabilities = flags; }

AppThread *Controller::getApplication() const { return _data->_application; }

StringView Controller::getName() const { return _data->_name; }

void Controller::run(Rc<Request> &&handle) { _data->pushTask(move(handle)); }

void Controller::setSignKey(Bytes &&value) { _data->_signKey = sp::move(value); }

bool Controller::isNetworkOnline() const {
	return (_data->_capabilities & NetworkFlags::Internet) != NetworkFlags::None;
}

} // namespace stappler::xenolith::network
