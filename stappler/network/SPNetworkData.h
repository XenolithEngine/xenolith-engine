/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_NETWORK_SPNETWORKHANDLEDATA_H_
#define STAPPLER_NETWORK_SPNETWORKHANDLEDATA_H_

#include "SPCommon.h"
#include "SPTime.h"
#include "SPCrypto.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::network {

#if LINUX
static constexpr auto DefaultCertPath = "/etc/ssl/certs/";
#else
static constexpr auto DefaultCertPath = "";
#endif

enum class Method {
	Unknown,
	Get,
	Post,
	Put,
	Delete,
	Head,
	Smtp
};

enum class AuthMethod {
	Basic,
	Digest,
	PKey, // custom serenity method
};

uint32_t getActiveHandles();

template <typename Interface>
struct SP_PUBLIC AuthData {
	using String = typename Interface::StringType;

	std::variant< Pair<String, String>, // user, password
			String // keySign
			>
			data;

	String proxyAddress;
	String proxyAuth;
	AuthMethod authMethod = AuthMethod::Basic;
};

template <typename Interface>
struct SP_PUBLIC SendData {
	template <typename T>
	using Function = typename Interface::template FunctionType<T>;

	template <typename T>
	using Vector = typename Interface::template VectorType<T>;

	template <typename K, typename V>
	using Map = typename Interface::template MapType<K, V>;

	using String = typename Interface::StringType;
	using Bytes = typename Interface::BytesType;
	using IOCallback = Function<size_t(char *data, size_t size)>;

	using DataSource = std::variant< std::monostate,
			String, // filename
			Bytes, // data
			IOCallback // data callback
			>;

	Map<String, String> headers;
	String url;
	Vector<String> recipients;
	String from;
	String userAgent;

	DataSource data;
	size_t size = 0;
	size_t offset = 0;
	Method method = Method::Unknown;
};

template <typename Interface>
struct SP_PUBLIC ProcessData {
	template <typename T>
	using Function = typename Interface::template FunctionType<T>;

	using String = typename Interface::StringType;
	using StringStream = typename Interface::StringStreamType;
	using ProgressCallback = Function<int(int64_t, int64_t)>;

	StringStream debugData;
	String cookieFile;
	String error;

	ProgressCallback uploadProgress = nullptr;
	ProgressCallback downloadProgress = nullptr;

	void *sharedHandle = nullptr;

	int64_t uploadProgressValue = 0;
	Time uploadProgressTiming;

	int64_t downloadProgressValue = 0;
	Time downloadProgressTiming;

	long errorCode = 0;
	long responseCode = -1;

	int connectTimeout = 20;
	int lowSpeedTime = 120;
	int lowSpeedLimit = 10_KiB;

	bool shared = false;
	bool verifyTsl = true;
	bool debug = false;
	bool reuse = true;
	bool silent = false;
	bool performed = false;
	bool invalidate = false;
};

template <typename Interface>
struct SP_PUBLIC ReceiveData {
	template <typename T>
	using Function = typename Interface::template FunctionType<T>;

	template <typename T>
	using Vector = typename Interface::template VectorType<T>;

	template <typename K, typename V>
	using Map = typename Interface::template MapType<K, V>;

	using String = typename Interface::StringType;
	using Bytes = typename Interface::BytesType;
	using IOCallback = Function<size_t(char *data, size_t size)>;
	using HeaderCallback = Function<void(StringView, StringView)>;

	using DataSource = std::variant< std::monostate,
			String, // filename
			IOCallback // data callback
			>;

	Vector<String> headers;
	Map<String, String> parsed;
	String contentType;

	DataSource data;
	HeaderCallback headerCallback;

	uint64_t offset = 0;
	bool resumeDownload = false;
};

template <typename Interface>
struct SP_PUBLIC HandleData : Interface::AllocBaseType {
	template <typename T>
	using Vector = typename Interface::template VectorType<T>;

	template <typename T>
	using Function = typename Interface::template FunctionType<T>;

	template <typename K, typename V>
	using Map = typename Interface::template MapType<K, V>;

	using String = typename Interface::StringType;
	using StringStream = typename Interface::StringStreamType;
	using Bytes = typename Interface::BytesType;
	using Value = data::ValueTemplate<Interface>;
	using HeaderMap = Map<String, String>;

	using ProgressCallback = Function<int(int64_t, int64_t)>;
	using IOCallback = Function<size_t(char *data, size_t size)>;
	using HeaderCallback = Function<void(StringView, StringView)>;

	SendData<Interface> send;
	ProcessData<Interface> process;
	ReceiveData<Interface> receive;
	AuthData<Interface> auth;

	~HandleData();

	bool reset(Method method, StringView url);

	long getResponseCode() const;
	long getErrorCode() const;
	StringView getError() const;

	void setCookieFile(const FileInfo &str);
	void setUserAgent(StringView str);
	void setUrl(StringView str);

	void clearHeaders();
	void addHeader(StringView header, StringView value);
	const HeaderMap &getRequestHeaders() const;

	void setMailFrom(StringView from);
	void clearMailTo();
	void addMailTo(StringView to);

	void setAuthority(StringView user, StringView passwd = StringView(),
			AuthMethod method = AuthMethod::Basic);
	bool setPrivateKeyAuth(BytesView priv);
	bool setPrivateKeyAuth(const crypto::PrivateKey &priv);

	void setProxy(StringView proxy, StringView auth);

	void setReceiveFile(const FileInfo &str, bool resumeDownload);
	void setReceiveCallback(IOCallback &&cb);
	void setResumeDownload(bool value);
	void setResumeOffset(uint64_t);
	const typename ReceiveData<Interface>::DataSource &getReceiveDataSource() const;

	void setSendSize(size_t size);
	void setSendFile(const FileInfo &str, StringView type = StringView());

	void setSendCallback(IOCallback &&cb, size_t outSize, StringView type = StringView());
	void setSendData(StringView data, StringView type = StringView());
	void setSendData(BytesView data, StringView type = StringView());
	void setSendData(Bytes &&data, StringView type = StringView());
	void setSendData(const uint8_t *data, size_t size, StringView type = StringView());
	void setSendData(const Value &data, data::EncodeFormat fmt = data::EncodeFormat());
	const typename SendData<Interface>::DataSource &getSendDataSource() const;

	void setHeaderCallback(HeaderCallback &&);
	const HeaderCallback &getHeaderCallback() const;

	StringView getReceivedHeaderString(StringView ch) const;
	int64_t getReceivedHeaderInt(StringView ch) const;

	Method getMethod() const;
	StringView getUrl() const;
	StringView getCookieFile() const;
	StringView getUserAgent() const;
	StringView getResponseContentType() const;
	const Vector<String> &getRecievedHeaders() const;

	void setDebug(bool value);
	void setReuse(bool value);
	void setShared(bool value);
	void setSilent(bool value);
	const StringStream &getDebugData() const;

	void setDownloadProgress(ProgressCallback &&callback);
	void setUploadProgress(ProgressCallback &&callback);

	void setConnectTimeout(int time);
	void setLowSpeedLimit(int time, size_t limit);

	void setVerifyTls(bool);
};

} // namespace stappler::network

#endif /* STAPPLER_NETWORK_SPNETWORKHANDLEDATA_H_ */
