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

#include "SPFilepath.h"
#include "XLNetworkController.h"
#include "XLNetworkRequest.h"
#include "XLContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::network {

bool Handle::init(StringView url) { return NetworkHandle::init(Method::Get, url); }

bool Handle::init(StringView url, const FileInfo &fileName) {
	if (!init(Method::Get, url)) {
		return false;
	}

	if (!fileName.path.empty()) {
		setReceiveFile(fileName, false);
	}
	return true;
}

bool Handle::init(Method method, StringView url) { return NetworkHandle::init(method, url); }

bool Handle::prepare(Context *ctx) {
	auto appInfo = _controller->getApplication()->getContext()->getInfo();

	if (_mtime > 0) {
		auto httpTime = Time::microseconds(_mtime).toHttp<Interface>();
		ctx->headers =
				curl_slist_append(ctx->headers, toString("If-Modified-Since: ", httpTime).data());
	}

	if (!_etag.empty()) {
		ctx->headers = curl_slist_append(ctx->headers, toString("If-None-Match: ", _etag).data());
	}

	ctx->headers = curl_slist_append(ctx->headers,
			toString("X-ApplicationName: ", appInfo->bundleName).data());
	ctx->headers = curl_slist_append(ctx->headers,
			toString("X-ApplicationVersion: ", appInfo->appVersion).data());

	if (!_sharegroup.empty() && ctx->share) {
		auto cookieFile =
				toString("network.", _controller->getName(), ".", _sharegroup, ".cookies");
		setCookieFile(FileInfo{cookieFile, FileCategory::AppCache});
	}

	if (!appInfo->userAgent.empty()) {
		setUserAgent(appInfo->userAgent);
	}

	return true;
}

bool Handle::finalize(Context *ctx, bool ret) {
	_success = ctx->success;

	if (getResponseCode() < 300) {
		auto httpTime = getReceivedHeaderString("Last-Modified");
		auto eTag = getReceivedHeaderString("ETag");

		_mtime = Time::fromHttp(httpTime).toMicroseconds();
		_etag = eTag.str<Interface>();
	} else {
		auto &f = getReceiveDataSource();
		if (auto str = std::get_if<String>(&f)) {
			filesystem::remove(FileInfo{*str});
		}
	}

	return ret;
}

Request::~Request() { }

bool Request::init(const Callback<bool(Handle &)> &setupCallback, Rc<Ref> &&ref) {
	_owner = move(ref);
	return setupCallback(_handle);
}

void Request::perform(AppThread *app, CompleteCallback &&cb) {
	auto c = app->getExtension<Controller>();
	if (!c) {
		return;
	}

	perform(c, sp::move(cb));
}

void Request::perform(Controller *c, CompleteCallback &&cb) {
	if (cb) {
		_onComplete = sp::move(cb);
	}

	_handle._request = this;
	_handle._controller = c;

	_uploadProgress = pair(0, 0);
	_downloadProgress = pair(0, 0);

	auto &source = _handle.getReceiveDataSource();
	if (std::holds_alternative<std::monostate>(source)) {
		if (!_ignoreResponseData) {
			_targetHeaderCallback = _handle.getHeaderCallback();

			_handle.setHeaderCallback(
					[this](StringView key, StringView value) { handleHeader(key, value); });

			_handle.setReceiveCallback(
					[this](char *buf, size_t size) -> size_t { return handleReceive(buf, size); });
			_handle.setVerifyTls(false);
		}
	}
	c->run(this);
}

void Request::setIgnoreResponseData(bool value) {
	if (!_running) {
		_ignoreResponseData = value;
	}
}

void Request::setUploadProgress(ProgressCallback &&cb) { _onUploadProgress = sp::move(cb); }

void Request::setDownloadProgress(ProgressCallback &&cb) { _onDownloadProgress = sp::move(cb); }

void Request::handleHeader(StringView key, StringView value) {
	if (!_ignoreResponseData) {
		if (key == "content-length") {
			auto length = value.readInteger(10).get(0);
			_data.resize(size_t(length));
		}
	}
	if (_targetHeaderCallback) {
		_targetHeaderCallback(key, value);
	}
}

size_t Request::handleReceive(char *buf, size_t nbytes) {
	if (_data.size() < (nbytes + _nbytes)) {
		_data.resize(nbytes + _nbytes);
	}
	memcpy(_data.data() + _nbytes, buf, nbytes);
	_nbytes += nbytes;
	return nbytes;
}

void Request::notifyOnComplete(bool success) {
	if (_onComplete) {
		_onComplete(*this, success);
	}
	_running = false;
	_handle._request = nullptr;
}

void Request::notifyOnUploadProgress(int64_t total, int64_t now) {
	_uploadProgress =
			pair(total, std::max(now, _uploadProgress.second)); // prevent out-of-order updates
	if (_onUploadProgress && _uploadProgress.second == now) {
		_onUploadProgress(*this, total, now);
	}
}

void Request::notifyOnDownloadProgress(int64_t total, int64_t now) {
	_downloadProgress =
			pair(total, std::max(now, _downloadProgress.second)); // prevent out-of-order updates
	if (_onDownloadProgress && _downloadProgress.second == now) {
		_onDownloadProgress(*this, total, now);
	}
}

} // namespace stappler::xenolith::network
