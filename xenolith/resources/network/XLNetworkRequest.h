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

#ifndef XENOLITH_RESOURCES_NETWORK_XLNETWORKREQUEST_H_
#define XENOLITH_RESOURCES_NETWORK_XLNETWORKREQUEST_H_

#include "XLAppThread.h" // IWYU pragma: keep
#include "SPNetworkHandle.h"

namespace STAPPLER_VERSIONIZED stappler::network {

template <typename Interface>
struct Context;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::network {

class Controller;
class Request;

using Method = stappler::network::Method;

class SP_PUBLIC Handle final : public NetworkHandle {
public:
	using Context = stappler::network::Context<Interface>;

	virtual ~Handle() { }

	// just GET url, actions with data defined with setSend*/setReceive*
	bool init(StringView url);

	// download to file with GET
	bool init(StringView url, const FileInfo &fileName);

	// perform query with specific method, actions with data defined with setSend*/setReceive*
	bool init(Method method, StringView url);

	bool isSuccess() const { return _success; }
	int64_t getMTime() const { return _mtime; }
	StringView getETag() const { return _etag; }
	StringView getSharegroup() const { return _sharegroup; }

	void setMTime(int64_t val) { _mtime = val; }
	void setETag(StringView val) { _etag = val.str<Interface>(); }
	void setSharegroup(StringView val) { _sharegroup = val.str<Interface>(); }

	void setSignRequest(bool value) { _signRequest = value; }
	bool shouldSignRequest() const { return _signRequest; }

	const Rc<Request> &getReqeust() const { return _request; }

protected:
	friend class Controller;
	friend class Request;

	bool prepare(Context *ctx);
	bool finalize(Context *ctx, bool success);

	bool _success = false;
	bool _signRequest = false;
	std::array<char, 256> _errorBuffer = {0};

	uint64_t _mtime = 0;
	String _etag;
	String _sharegroup;

	const Controller *_controller = nullptr;
	Rc<Request> _request;
};

class SP_PUBLIC Request : public Ref {
public:
	using CompleteCallback = Function<void(const Request &, bool)>;
	using ProgressCallback = Function<void(const Request &, int64_t total, int64_t now)>;

	virtual ~Request();

	virtual bool init(const Callback<bool(Handle &)> &setupCallback, Rc<Ref> && = nullptr);

	virtual void perform(AppThread *, CompleteCallback &&cb);
	virtual void perform(Controller *, CompleteCallback &&cb);

	void setIgnoreResponseData(bool);
	bool isIgnoreResponseData() const { return _ignoreResponseData; }

	bool isRunning() const { return _running; }

	const Handle &getHandle() const { return _handle; }

	float getUploadProgress() const {
		return float(_uploadProgress.second) / float(_uploadProgress.first);
	}
	float getDownloadProgress() const {
		return float(_downloadProgress.second) / float(_downloadProgress.first);
	}

	Pair<int64_t, int64_t> getUploadProgressCounters() const { return _uploadProgress; }
	Pair<int64_t, int64_t> getDownloadProgressCounters() const { return _downloadProgress; }

	void setUploadProgress(ProgressCallback &&);
	void setDownloadProgress(ProgressCallback &&);

	BytesView getData() const { return _data; }

protected:
	friend class Controller;

	void handleHeader(StringView, StringView);
	size_t handleReceive(char *, size_t);

	void notifyOnComplete(bool);
	void notifyOnUploadProgress(int64_t total, int64_t now);
	void notifyOnDownloadProgress(int64_t total, int64_t now);

	bool _running = false;
	bool _ignoreResponseData = false;
	bool _setupInput = false;
	Function<void(StringView, StringView)> _targetHeaderCallback;
	Pair<int64_t, int64_t> _uploadProgress = pair(0, 0); // total, now
	Pair<int64_t, int64_t> _downloadProgress = pair(0, 0); // total, now
	ProgressCallback _onDownloadProgress;
	ProgressCallback _onUploadProgress;
	CompleteCallback _onComplete;
	Handle _handle;
	Rc<Ref> _owner;

	size_t _nbytes = 0;
	Bytes _data;
};

} // namespace stappler::xenolith::network

#endif /* XENOLITH_RESOURCES_NETWORK_XLNETWORKREQUEST_H_ */
