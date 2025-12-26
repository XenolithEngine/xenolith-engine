/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_NETWORK_SPNETWORKHANDLE_H_
#define STAPPLER_NETWORK_SPNETWORKHANDLE_H_

#include "SPNetworkData.h"

namespace STAPPLER_VERSIONIZED stappler::network {

template <typename Interface>
class SP_PUBLIC Handle : private HandleData<Interface> {
public:
	using Method = network::Method;

	using DataType = HandleData<Interface>;

	using DataType::Vector;
	using DataType::Function;
	using DataType::Map;
	using String = typename DataType::String;
	using StringStream = typename DataType::StringStream;
	using Bytes = typename DataType::Bytes;
	using Value = typename DataType::Value;
	using ProgressCallback = typename DataType::ProgressCallback;
	using IOCallback = typename DataType::IOCallback;

	Handle() { }

	Handle(Handle &&) = default;
	Handle &operator=(Handle &&) = default;

	bool init(Method method, StringView url);

	bool perform();

	using DataType::getResponseCode;
	using DataType::getErrorCode;
	using DataType::getError;
	using DataType::setCookieFile;
	using DataType::setUserAgent;
	using DataType::clearHeaders;
	using DataType::addHeader;
	using DataType::getRequestHeaders;
	using DataType::setMailFrom;
	using DataType::clearMailTo;
	using DataType::addMailTo;
	using DataType::setAuthority;
	using DataType::setPrivateKeyAuth;
	using DataType::setProxy;
	using DataType::setReceiveFile;
	using DataType::setReceiveCallback;
	using DataType::setResumeOffset;
	using DataType::getReceiveDataSource;
	using DataType::setSendFile;
	using DataType::setSendCallback;
	using DataType::setSendData;
	using DataType::setHeaderCallback;
	using DataType::getHeaderCallback;
	using DataType::getSendDataSource;
	using DataType::getReceivedHeaderString;
	using DataType::getReceivedHeaderInt;
	using DataType::getRecievedHeaders;
	using DataType::getMethod;
	using DataType::getUrl;
	using DataType::getCookieFile;
	using DataType::getUserAgent;
	using DataType::getResponseContentType;
	using DataType::setDebug;
	using DataType::setReuse;
	using DataType::setShared;
	using DataType::setSilent;
	using DataType::getDebugData;
	using DataType::setDownloadProgress;
	using DataType::setUploadProgress;
	using DataType::setConnectTimeout;
	using DataType::setLowSpeedLimit;
	using DataType::setVerifyTls;

	using Interface::AllocBaseType::operator new;
	using Interface::AllocBaseType::operator delete;

protected:
	template <typename I>
	friend class MultiHandle;

	HandleData<Interface> *getData() { return this; }
};

template <typename Interface>
class SP_PUBLIC MultiHandle : public Interface::AllocBaseType {
public:
	// handle should be preserved until operation ends
	// multihandle do not stores handles by itself
	void addHandle(Handle<Interface> *handle, Ref *userdata) {
		pending.emplace_back(pair(handle, userdata));
	}

	// sync interface:
	// returns completed handles, so it can be immediately recharged with addHandle
	bool perform(const Callback<bool(Handle<Interface> *, Ref *)> &);

protected:
	typename Interface::template VectorType<Pair<Handle<Interface> *, Rc<Ref>>> pending;
};

}

namespace STAPPLER_VERSIONIZED stappler::mem_std {

using NetworkHandle = network::Handle<memory::StandartInterface>;

}

namespace STAPPLER_VERSIONIZED stappler::mem_pool {

using NetworkHandle = network::Handle<memory::PoolInterface>;

}

#endif /* STAPPLER_NETWORK_SPNETWORKHANDLE_H_ */
