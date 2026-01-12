/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPNetworkContext.h"
#include "SPNetworkData.h"
#include "curl/curl.h"

#ifndef SP_NETWORK_LOG
#define SP_NETWORK_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::network {

#define SP_TERMINATED_DATA(view) (view.terminated()?view.data():view.str<Interface>().data())

static constexpr auto SP_NETWORK_PROGRESS_TIMEOUT = TimeInterval::microseconds(250'000ull);
static constexpr auto s_UserAgent = "Stappler/1 CURL";

struct CurlHandle;

SPUNUSED static StringView getCABundle();

SPUNUSED static CURL *CurlHandle_getHandle(bool reuse, memory::pool_t *pool);
SPUNUSED static void CurlHandle_releaseHandle(CURL *curl, bool reuse, bool success,
		memory::pool_t *pool);

static size_t _writeDummy(const void *data, size_t size, size_t nmemb, void *userptr) {
	return size * nmemb;
}

template <typename Interface>
static size_t _writeDebug(CURL *handle, curl_infotype type, char *data, size_t size,
		void *userptr) {
	auto task = static_cast<HandleData<Interface> *>(userptr);
	task->process.debugData.write(data, size);
	return size;
}

template <typename Interface>
static size_t _writeData(char *data, size_t size, size_t nmemb, void *userptr) {
	auto task = static_cast<HandleData<Interface> *>(userptr);

	return std::visit([&](auto &&arg) {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, typename HandleData<Interface>::IOCallback>) {
			return arg(data, size * nmemb);
		}
		return size_t(size * nmemb);
	}, task->receive.data);
}

template <typename Interface>
static size_t _writeHeaders(char *data, size_t size, size_t nmemb, void *userptr) {
	auto task = static_cast<HandleData<Interface> *>(userptr);

	StringView reader(data, size * nmemb);
	if (!reader.is("\r\n")) {
		if (task->send.method != Method::Smtp) {
			if (!reader.is("HTTP/")) {
				auto name = reader.readUntil<StringView::Chars<':'>>();
				reader++;

				name.trimChars<StringView::WhiteSpace>();
				reader.trimChars<StringView::WhiteSpace>();

				auto nameStr = string::tolower<Interface>(name);
				auto valueStr = reader.str<Interface>();

				if (task->receive.headerCallback) {
					task->receive.headerCallback(nameStr, valueStr);
				}
				task->receive.parsed.emplace(sp::move(nameStr), sp::move(valueStr));
			} else {
				reader.skipUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
				reader.skipUntil<StringView::CharGroup<CharGroupId::Numbers>>();
				reader.readInteger().unwrap(
						[&](int64_t code) { task->process.responseCode = code; });
			}
		}

		task->receive.headers.emplace_back(StringView(data, size).str<Interface>());
	}

	return size * nmemb;
}

template <typename Interface>
static size_t _readData(char *data, size_t size, size_t nmemb, void *userptr) {
	if (userptr != NULL) {
		auto task = static_cast<HandleData<Interface> *>(userptr);

		return std::visit([&](auto &&arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, typename HandleData<Interface>::IOCallback>) {
				return arg(data, size * nmemb);
			} else if constexpr (std::is_same_v<T, typename HandleData<Interface>::Bytes>) {
				size_t remains = task->send.size;
				if (size * nmemb <= remains) {
					memcpy(data, arg.data() + (arg.size() - task->send.size), size * nmemb);
					task->send.size -= size * nmemb;
					return size * nmemb;
				} else {
					memcpy(data, arg.data() + (arg.size() - task->send.size), remains);
					task->send.size = 0;
					return remains;
				}
			}
			return size_t(0);
		}, task->send.data);
	} else {
		return 0;
	}
}

template <typename Interface>
static int _progress(void *userptr, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
		curl_off_t ulnow) {
	auto task = static_cast<HandleData<Interface> *>(userptr);
	auto timing = Time::now();

	int uProgress = 0;
	if (task->process.uploadProgress && ulnow != task->process.uploadProgressValue
			&& (!task->process.uploadProgressTiming
					|| (timing - task->process.uploadProgressTiming
							> SP_NETWORK_PROGRESS_TIMEOUT))) {
		task->process.uploadProgressValue = ulnow;
		task->process.uploadProgressTiming = timing;
		uProgress = task->process.uploadProgress(ultotal, ulnow);
	}

	int dProgress = 0;
	if (task->process.downloadProgress && dlnow != task->process.downloadProgressValue
			&& (!task->process.downloadProgressTiming
					|| (timing - task->process.downloadProgressTiming
							> SP_NETWORK_PROGRESS_TIMEOUT))) {
		task->process.downloadProgressValue = dlnow;
		task->process.downloadProgressTiming = timing;
		return task->process.downloadProgress(dltotal + task->receive.offset,
				dlnow + task->receive.offset);
	}

	if (ultotal == ulnow || ultotal == 0) {
		return dProgress;
	} else {
		return uProgress;
	}
}

template <typename Interface>
Pair<FILE *, uint64_t> _openFile(const FileInfo &filename, bool readOnly, bool resume = false) {
	uint64_t pos = 0;
	FILE *file = nullptr;
	if (filesystem::exists(filename)) {
		filesystem::Stat stat;
		if (filesystem::stat(filename, stat) && stat.size) {
			pos = stat.size;
			if (!readOnly) {
				filesystem::enumerateWritablePaths(filename, filesystem::Access::None,
						[&](const LocationInfo &loc, StringView path) {
					void *fileDes = nullptr;
					const char *mode = nullptr;
					if (!resume) {
						filesystem::remove(filename);
						fileDes = loc.interface->_open(loc, path,
								filesystem::OpenFlags::Read | filesystem::OpenFlags::Override,
								nullptr);
						mode = "w+b";
					} else {
						fileDes = loc.interface->_open(loc, path,
								filesystem::OpenFlags::Read | filesystem::OpenFlags::Append
										| filesystem::OpenFlags::Write,
								nullptr);
						mode = "a+b";
					}
					if (fileDes) {
						file = loc.interface->_fdopen(fileDes, mode, nullptr);
						if (!file) {
							loc.interface->_close(fileDes, nullptr);
						}
					}
					return false;
				});
			} else {
				filesystem::enumeratePaths(filename, filesystem::Access::None,
						[&](const LocationInfo &loc, StringView path) {
					auto fileDes =
							loc.interface->_open(loc, path, filesystem::OpenFlags::Read, nullptr);
					if (fileDes) {
						file = loc.interface->_fdopen(fileDes, "rb", nullptr);
						if (!file) {
							loc.interface->_close(fileDes, nullptr);
						}
					}
					return false;
				});
			}
		}
	} else {
		if (!readOnly) {
			filesystem::enumerateWritablePaths(filename, filesystem::Access::None,
					[&](const LocationInfo &loc, StringView path) {
				auto fileDes = loc.interface->_open(loc, path,
						filesystem::OpenFlags::Read | filesystem::OpenFlags::Write, nullptr);
				if (fileDes) {
					file = loc.interface->_fdopen(fileDes, "w+b", nullptr);
					if (!file) {
						loc.interface->_close(fileDes, nullptr);
					}
				}
				return false;
			});
		}
	}
	return pair(file, pos);
}

template <typename K, typename T>
inline void SetOpt(bool &check, CURL *curl, K opt, const T &value, bool optional = false) {
#ifdef DEBUG
	int err = CURLE_OK;
	if (check) {
		err = curl_easy_setopt(curl, opt, value);
		if (err != CURLE_OK) {
			if (!optional || err != CURLE_NOT_BUILT_IN) {
				slog().debug("CURL", "curl_easy_setopt (", opt, ") failed: ", err);
				check = false;
			}
		}
	}
#else
	check = (check) ? curl_easy_setopt(curl, opt, value) == CURLE_OK : false;
#endif
}

template <typename Interface>
static bool _setupCurl(const HandleData<Interface> &iface, CURL *curl, char *errorBuffer) {
	bool check = true;

	auto CABundle = getCABundle();
	static struct curl_blob blob{(void *)CABundle.data(), CABundle.size(), CURL_BLOB_NOCOPY};

#ifdef STAPPLER_SHARED
	SetOpt(check, curl, CURLOPT_CAINFO_BLOB, &blob, true);
#else
	SetOpt(check, curl, CURLOPT_CAINFO_BLOB, &blob);
#endif

	SetOpt(check, curl, CURLOPT_USE_SSL, CURLUSESSL_TRY, true);

	SetOpt(check, curl, CURLOPT_NOSIGNAL, 1);
	SetOpt(check, curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);

	SetOpt(check, curl, CURLOPT_ERRORBUFFER, errorBuffer);
	SetOpt(check, curl, CURLOPT_LOW_SPEED_TIME, iface.process.lowSpeedTime);
	SetOpt(check, curl, CURLOPT_LOW_SPEED_LIMIT, iface.process.lowSpeedLimit);
	//SetOpt(check, curl, CURLOPT_TIMEOUT, SP_NW_READ_TIMEOUT);
	SetOpt(check, curl, CURLOPT_CONNECTTIMEOUT, iface.process.connectTimeout);

	if (iface.process.verifyTsl) {
		SetOpt(check, curl, CURLOPT_SSL_VERIFYPEER, 1L);
		SetOpt(check, curl, CURLOPT_SSL_VERIFYHOST, 2L);
	} else {
		SetOpt(check, curl, CURLOPT_SSL_VERIFYPEER, 0L);
		SetOpt(check, curl, CURLOPT_SSL_VERIFYHOST, 0L);
	}

	SetOpt(check, curl, CURLOPT_URL, iface.send.url.data());
	SetOpt(check, curl, CURLOPT_RESUME_FROM, 0); // drop byte-ranged GET

	SetOpt(check, curl, CURLOPT_WRITEFUNCTION, _writeDummy);
	SetOpt(check, curl, CURLOPT_WRITEDATA, nullptr);

	/* enable all supported built-in compressions */
	SetOpt(check, curl, CURLOPT_ACCEPT_ENCODING, "");

	if (!check) {
		slog().debug("CURL", "Fail to perform general setup");
	}

	return check;
}

template <typename Interface>
static bool _setupDebug(const HandleData<Interface> &iface, CURL *curl, bool debug) {
	bool check = true;
	if (debug) {
		SetOpt(check, curl, CURLOPT_VERBOSE, 1);
		SetOpt(check, curl, CURLOPT_DEBUGFUNCTION, _writeDebug<Interface>);
		SetOpt(check, curl, CURLOPT_DEBUGDATA, &iface);
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform debug setup");
	}

	return check;
}

template <typename Interface>
static bool _setupHeaders(const HandleData<Interface> &iface, Context<Interface> *ctx,
		const typename HandleData<Interface>::HeaderMap &vec, curl_slist **headers) {
	bool check = true;
	StringView keySign;
	if (iface.auth.authMethod == AuthMethod::PKey) {
		if (auto sign = std::get_if<typename HandleData<Interface>::String>(&iface.auth.data)) {
			keySign = StringView(*sign);
		}
	}

	ctx->headersData.reserve(vec.size());
	for (auto &it : vec) {
		if (iface.send.method == Method::Get || iface.send.method == Method::Head
				|| iface.send.method == Method::Delete) {
			if (it.first == "Content-Type") {
				continue;
			}
		}

		if (it.first != "Authorization" || keySign.empty()) {
			ctx->headersData.emplace_back(string::toString<Interface>(it.first, ": ", it.second));
		}
	}

	if (!keySign.empty()) {
		ctx->headersData.emplace_back(string::toString<Interface>("Authorization: pkey ", keySign));
		*headers = curl_slist_append(*headers,
				string::toString<Interface>("Authorization: pkey ", keySign).data());
	}

	for (auto &it : ctx->headersData) { *headers = curl_slist_append(*headers, it.data()); }

	if (!ctx->headersData.empty() || *headers) {
		SetOpt(check, ctx->curl, CURLOPT_HTTPHEADER, *headers);
	}

	SetOpt(check, ctx->curl, CURLOPT_HEADERFUNCTION, _writeHeaders<Interface>);
	SetOpt(check, ctx->curl, CURLOPT_HEADERDATA, &iface);

	if (!check) {
		slog().debug("CURL", "Fail to perform headers setup");
	}

	return check;
}

template <typename Interface>
static bool _setupUserAgent(const HandleData<Interface> &iface, CURL *curl,
		const StringView &agent) {
	bool check = true;
	if (!agent.empty()) {
		SetOpt(check, curl, CURLOPT_USERAGENT, SP_TERMINATED_DATA(agent));
	} else {
		SetOpt(check, curl, CURLOPT_USERAGENT, s_UserAgent);
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform user-agent setup");
	}

	return check;
}

template <typename Interface>
static bool _setupUser(const HandleData<Interface> &iface, CURL *curl, const StringView &user,
		const StringView &password, AuthMethod m) {
	bool check = true;
	if (!user.empty()) {
		switch (m) {
		case AuthMethod::Basic: SetOpt(check, curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC); break;
		case AuthMethod::Digest: SetOpt(check, curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST); break;
		default: return false; break;
		}
		SetOpt(check, curl, CURLOPT_USERNAME, SP_TERMINATED_DATA(user));
		if (!password.empty()) {
			SetOpt(check, curl, CURLOPT_PASSWORD, SP_TERMINATED_DATA(password));
		}
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform user setup");
	}

	return check;
}

template <typename Interface>
static bool _setupFrom(const HandleData<Interface> &iface, CURL *curl, const StringView &from) {
	bool check = true;
	SetOpt(check, curl, CURLOPT_MAIL_FROM, SP_TERMINATED_DATA(from));

	if (!check) {
		slog().debug("CURL", "Fail to perform user-from setup");
	}

	return check;
}

template <typename Interface>
static bool _setupRecv(const HandleData<Interface> &iface, CURL *curl,
		const typename Interface::template VectorType<typename Interface::StringType> &vec,
		curl_slist **mailTo) {
	bool check = true;
	if (vec.size() > 0) {
		for (const auto &str : vec) { *mailTo = curl_slist_append(*mailTo, str.c_str()); }
		SetOpt(check, curl, CURLOPT_MAIL_RCPT, *mailTo);
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform recv setup");
	}

	return check;
}

template <typename Interface>
static bool _setupProgress(const HandleData<Interface> &iface, CURL *curl) {
	bool check = true;
	if (iface.send.method != Method::Head
			&& (iface.process.uploadProgress || iface.process.downloadProgress)) {
		SetOpt(check, curl, CURLOPT_NOPROGRESS, 0);
	} else {
		SetOpt(check, curl, CURLOPT_NOPROGRESS, 1);
	}
	SetOpt(check, curl, CURLOPT_XFERINFOFUNCTION, _progress<Interface>);
	SetOpt(check, curl, CURLOPT_XFERINFODATA, &iface);

	if (!check) {
		slog().debug("CURL", "Fail to perform progress setup");
	}

	return check;
}

template <typename Interface>
static bool _setupCookies(const HandleData<Interface> &iface, CURL *curl,
		const StringView &cookiePath) {
	bool check = true;
	if (!cookiePath.empty()) {
		SetOpt(check, curl, CURLOPT_COOKIEFILE, SP_TERMINATED_DATA(cookiePath));
		SetOpt(check, curl, CURLOPT_COOKIEJAR, SP_TERMINATED_DATA(cookiePath));
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform cookies setup");
	}

	return check;
}

template <typename Interface>
static bool _setupProxy(const HandleData<Interface> &iface, CURL *curl, const StringView &proxy,
		const StringView &auth) {
	bool check = true;
	if (!proxy.empty()) {
		SetOpt(check, curl, CURLOPT_PROXY, SP_TERMINATED_DATA(proxy));
	} else {
		SetOpt(check, curl, CURLOPT_PROXY, nullptr);
	}

	if (!auth.empty()) {
		SetOpt(check, curl, CURLOPT_PROXYUSERPWD, SP_TERMINATED_DATA(auth));
	} else {
		SetOpt(check, curl, CURLOPT_PROXYUSERPWD, nullptr);
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform proxy setup");
	}

	return true;
}

template <typename Interface>
static bool _setupReceive(HandleData<Interface> &iface, CURL *curl, FILE *&inputFile,
		uint64_t &inputPos) {
	bool check = true;
	if (iface.send.method != Method::Head) {
		std::visit([&](auto &&arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, typename HandleData<Interface>::String>) {
				iface.receive.offset = 0;
				std::tie(inputFile, inputPos) =
						_openFile<Interface>(FileInfo{arg}, false, iface.receive.resumeDownload);
				if (inputFile) {
					SetOpt(check, curl, CURLOPT_WRITEFUNCTION, (void *)NULL);
					SetOpt(check, curl, CURLOPT_WRITEDATA, inputFile);
					if (inputPos != 0 && iface.receive.resumeDownload) {
						iface.receive.offset = inputPos;
						SetOpt(check, curl, CURLOPT_RESUME_FROM_LARGE, inputPos);
					}
				}
			} else if constexpr (std::is_same_v<T, typename HandleData<Interface>::IOCallback>) {
				SetOpt(check, curl, CURLOPT_WRITEFUNCTION, _writeData<Interface>);
				SetOpt(check, curl, CURLOPT_WRITEDATA, &iface);
				if (iface.receive.offset > 0) {
					SetOpt(check, curl, CURLOPT_RESUME_FROM_LARGE, iface.receive.offset);
				}
			}
		}, iface.receive.data);
	}

	if (!check) {
		slog().debug("CURL", "Fail to perform receive setup");
	}

	return check;
}

template <typename Interface>
static bool _setupMethodGet(const HandleData<Interface> &iface, CURL *curl) {
	bool check = true;
	SetOpt(check, curl, CURLOPT_HTTPGET, 1);
	SetOpt(check, curl, CURLOPT_FOLLOWLOCATION, 1);

	if (!check) {
		slog().debug("CURL", "Fail to perform GET method setup");
	}

	return check;
}

template <typename Interface>
static bool _setupMethodHead(const HandleData<Interface> &iface, CURL *curl) {
	bool check = true;
	SetOpt(check, curl, CURLOPT_HTTPGET, 1);
	SetOpt(check, curl, CURLOPT_FOLLOWLOCATION, 1);
	SetOpt(check, curl, CURLOPT_NOBODY, 1);

	if (!check) {
		slog().debug("CURL", "Fail to perform HEAD method setup");
	}

	return check;
}

template <typename Interface>
static void _setupSendData(bool &check, const HandleData<Interface> &iface, CURL *curl,
		FILE *&outputFile) {
	std::visit([&](auto &&arg) {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, typename HandleData<Interface>::String>) {
			size_t size;
			std::tie(outputFile, size) = _openFile<Interface>(FileInfo{arg}, true);
			if (outputFile) {
				SetOpt(check, curl, CURLOPT_READFUNCTION, (void *)NULL);
				SetOpt(check, curl, CURLOPT_READDATA, outputFile);
				SetOpt(check, curl, CURLOPT_POSTFIELDSIZE, size);
				SetOpt(check, curl, CURLOPT_INFILESIZE, size);
			}
		} else if constexpr (std::is_same_v<T, typename HandleData<Interface>::IOCallback>) {
			SetOpt(check, curl, CURLOPT_READFUNCTION, _readData<Interface>);
			SetOpt(check, curl, CURLOPT_READDATA, &iface);
			SetOpt(check, curl, CURLOPT_POSTFIELDSIZE, iface.send.size);
			SetOpt(check, curl, CURLOPT_INFILESIZE, iface.send.size);
		} else if constexpr (std::is_same_v<T, typename HandleData<Interface>::Bytes>) {
			SetOpt(check, curl, CURLOPT_POSTFIELDS, arg.data());
			SetOpt(check, curl, CURLOPT_POSTFIELDSIZE, arg.size());
			SetOpt(check, curl, CURLOPT_INFILESIZE, arg.size());
		}
	}, iface.send.data);

	if (!check) {
		slog().debug("CURL", "Fail to perform output data setup");
	}
}


template <typename Interface>
static bool _setupMethodPost(const HandleData<Interface> &iface, CURL *curl, FILE *&outputFile) {
	bool check = true;
	SetOpt(check, curl, CURLOPT_POST, 1);

	SetOpt(check, curl, CURLOPT_READFUNCTION, (void *)NULL);
	SetOpt(check, curl, CURLOPT_READDATA, (void *)NULL);
	SetOpt(check, curl, CURLOPT_POSTFIELDS, (void *)NULL);
	SetOpt(check, curl, CURLOPT_POSTFIELDSIZE, 0);

	if (!check) {
		slog().debug("CURL", "Fail to perform POST method setup");
	}

	_setupSendData(check, iface, curl, outputFile);

	return check;
}

template <typename Interface>
static bool _setupMethodPut(const HandleData<Interface> &iface, CURL *curl, FILE *&outputFile) {
	bool check = true;

	SetOpt(check, curl, CURLOPT_UPLOAD, 1);
	SetOpt(check, curl, CURLOPT_READFUNCTION, (void *)NULL);
	SetOpt(check, curl, CURLOPT_READDATA, (void *)NULL);
	SetOpt(check, curl, CURLOPT_CUSTOMREQUEST, "PUT");

	if (!check) {
		slog().debug("CURL", "Fail to perform PUT method setup");
	}

	_setupSendData(check, iface, curl, outputFile);

	return check;
}

template <typename Interface>
static bool _setupMethodDelete(const HandleData<Interface> &iface, CURL *curl) {
	bool check = true;
	SetOpt(check, curl, CURLOPT_FOLLOWLOCATION, 1);
	SetOpt(check, curl, CURLOPT_CUSTOMREQUEST, "DELETE");

	if (!check) {
		slog().debug("CURL", "Fail to perform DELETE method setup");
	}

	return check;
}

template <typename Interface>
static bool _setupMethodSmpt(const HandleData<Interface> &iface, CURL *curl, FILE *&outputFile) {
	bool check = true;

	SetOpt(check, curl, CURLOPT_UPLOAD, 1);
	SetOpt(check, curl, CURLOPT_READFUNCTION, (void *)NULL);
	SetOpt(check, curl, CURLOPT_READDATA, (void *)NULL);
	SetOpt(check, curl, CURLOPT_INFILESIZE, 0);

	if (!check) {
		slog().debug("CURL", "Fail to perform SMTP setup");
	}

	_setupSendData(check, iface, curl, outputFile);

	SetOpt(check, curl, CURLOPT_USE_SSL, CURLUSESSL_ALL, true);
	return check;
}

template <typename Interface>
bool prepare(HandleData<Interface> &iface, Context<Interface> *ctx,
		const Callback<bool(CURL *)> &onBeforePerform) {
	if (iface.process.debug) {
		iface.process.debugData = typename Interface::StringStreamType();
	}

	iface.receive.parsed.clear();
	iface.receive.headers.clear();

	ctx->handle = &iface;

	bool check = true;

	if (ctx->share) {
		SetOpt(check, ctx->curl, CURLOPT_SHARE, iface.process.sharedHandle);
	} else if (iface.process.shared) {
		if (!iface.process.sharedHandle) {
			iface.process.sharedHandle = curl_share_init();
			curl_share_setopt((CURLSH *)iface.process.sharedHandle, CURLSHOPT_SHARE,
					CURL_LOCK_DATA_COOKIE);
			curl_share_setopt((CURLSH *)iface.process.sharedHandle, CURLSHOPT_SHARE,
					CURL_LOCK_DATA_DNS);
			curl_share_setopt((CURLSH *)iface.process.sharedHandle, CURLSHOPT_SHARE,
					CURL_LOCK_DATA_SSL_SESSION);
			curl_share_setopt((CURLSH *)iface.process.sharedHandle, CURLSHOPT_SHARE,
					CURL_LOCK_DATA_CONNECT);
		}
		SetOpt(check, ctx->curl, CURLOPT_COOKIEFILE, "/undefined");
		SetOpt(check, ctx->curl, CURLOPT_SHARE, iface.process.sharedHandle);
	} else {
		SetOpt(check, ctx->curl, CURLOPT_SHARE, nullptr);
	}

	check = (check) ? _setupCurl(iface, ctx->curl, ctx->error.data()) : false;
	check = (check) ? _setupDebug(iface, ctx->curl, iface.process.debug) : false;
	check = (check) ? _setupHeaders(iface, ctx, iface.send.headers, &ctx->headers) : false;
	check = (check) ? _setupUserAgent(iface, ctx->curl, iface.send.userAgent) : false;
	if (auto u = std::get_if<Pair<typename HandleData<Interface>::String,
					typename HandleData<Interface>::String>>(&iface.auth.data)) {
		check = (check) ? _setupUser(iface, ctx->curl, u->first, u->second, iface.auth.authMethod)
						: false;
	}
	check = (check) ? _setupProgress(iface, ctx->curl) : false;
	check = (check) ? _setupCookies(iface, ctx->curl, iface.process.cookieFile) : false;
	check = (check) ? _setupProxy(iface, ctx->curl, iface.auth.proxyAddress, iface.auth.proxyAuth)
					: false;
	check = (check) ? _setupReceive(iface, ctx->curl, ctx->inputFile, ctx->inputPos) : false;

	switch (iface.send.method) {
	case Method::Get: check = (check) ? _setupMethodGet(iface, ctx->curl) : false; break;
	case Method::Head: check = (check) ? _setupMethodHead(iface, ctx->curl) : false; break;
	case Method::Post:
		check = (check) ? _setupMethodPost(iface, ctx->curl, ctx->outputFile) : false;
		break;
	case Method::Put:
		check = (check) ? _setupMethodPut(iface, ctx->curl, ctx->outputFile) : false;
		break;
	case Method::Delete: check = (check) ? _setupMethodDelete(iface, ctx->curl) : false; break;
	case Method::Smtp:
		check = (check) ? _setupRecv(iface, ctx->curl, iface.send.recipients, &ctx->mailTo) : false;
		check = (check) ? _setupFrom(iface, ctx->curl, iface.send.from) : false;
		check = (check) ? _setupMethodSmpt(iface, ctx->curl, ctx->outputFile) : false;
		break;
	default: break;
	}

	if (!check) {
		if (!iface.process.silent) {
			log::source().error("CURL", "Fail to setup: ", iface.send.url.data());
		}
		return false;
	}

	if (onBeforePerform) {
		if (!onBeforePerform(ctx->curl)) {
			if (!iface.process.silent) {
				log::source().error("CURL", "onBeforePerform failed");
			}
			return false;
		}
	}

	iface.process.debugData.clear();
	iface.receive.parsed.clear();
	iface.receive.headers.clear();
	return true;
}

template <typename Interface>
bool finalize(HandleData<Interface> &iface, Context<Interface> *ctx,
		const Callback<bool(CURL *)> &onAfterPerform) {
	iface.process.errorCode = ctx->code;
	if (ctx->headers) {
		curl_slist_free_all(ctx->headers);
		ctx->headers = nullptr;
	}

	if (ctx->mailTo) {
		curl_slist_free_all(ctx->mailTo);
		ctx->mailTo = nullptr;
	}

	if (CURLE_RANGE_ERROR == iface.process.errorCode && iface.send.method == Method::Get) {
		size_t allowedRange = size_t(iface.getReceivedHeaderInt("X-Range"));
		if (allowedRange == ctx->inputPos) {
			if (!iface.process.silent) {
				log::source().warn("CURL",
						"Get 0-range is not an error, fixed error code to CURLE_OK");
			}
			ctx->success = true;
			iface.process.errorCode = CURLE_OK;
		}
	}

	if (CURLE_OK == iface.process.errorCode) {
		iface.process.performed = true;
		if (iface.send.method != Method::Smtp) {
			const char *ct = NULL;
			long code = 200;

			curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(ctx->curl, CURLINFO_CONTENT_TYPE, &ct);
			if (ct) {
				iface.receive.contentType = ct;
			}

			iface.process.responseCode = (long)code;

			SP_NETWORK_LOG("performed: %d %s %ld", (int)_method, _url.c_str(), _responseCode);

			if (iface.process.responseCode == 416) {
				size_t allowedRange = size_t(iface.getReceivedHeaderInt("X-Range"));
				if (allowedRange == ctx->inputPos) {
					iface.process.responseCode = 200;
					if (!iface.process.silent) {
						log::source().warn("CURL", iface.send.url,
								": Get 0-range is not an error, fixed response code to 200");
					}
				}
			}

			if (iface.process.responseCode >= 200 && iface.process.responseCode < 400) {
				ctx->success = true;
			} else {
				ctx->success = false;
			}
		} else {
			ctx->success = true;
		}
	} else {
		if (!iface.process.silent) {
			log::format(log::Error, "CURL", SP_LOCATION, "fail to perform %s: (%ld) %s",
					iface.send.url.data(), iface.process.errorCode, ctx->error.data());
		}
		iface.process.error = ctx->error.data();
		if (iface.process.debug) {
			std::visit([&](auto &&arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, typename HandleData<Interface>::String>) {
					log::source().debug("CURL", "Input file: ", arg);
				}
			}, iface.receive.data);
		}
		ctx->success = false;
	}

	if (!iface.process.cookieFile.empty()) {
		auto it = iface.receive.parsed.find("set-cookie");
		if (it != iface.receive.parsed.end()) {
			iface.process.invalidate = true;
		}
	}

	if (onAfterPerform) {
		if (!onAfterPerform(ctx->curl)) {
			ctx->success = false;
		}
	}

	if (ctx->inputFile) {
		fflush(ctx->inputFile);
		fclose(ctx->inputFile);
		ctx->inputFile = nullptr;
	}
	if (ctx->outputFile) {
		fclose(ctx->outputFile);
		ctx->outputFile = nullptr;
	}
	return ctx->success;
}

template <typename Interface>
static bool _perform(Context<Interface> &ctx, HandleData<Interface> &iface,
		const Callback<bool(CURL *)> &onBeforePerform,
		const Callback<bool(CURL *)> &onAfterPerform) {

	iface.process.performed = false;
	iface.process.errorCode = CURLE_OK;
	iface.process.responseCode = -1;

	if (!ctx.curl) {
		return false;
	}

	if (!prepare(iface, &ctx, onBeforePerform)) {
		return false;
	}

	ctx.code = curl_easy_perform(ctx.curl);
	return finalize(iface, &ctx, onAfterPerform);
}

template <>
bool perform(HandleData<memory::PoolInterface> &iface,
		const Callback<bool(CURL *)> &onBeforePerform,
		const Callback<bool(CURL *)> &onAfterPerform) {
	auto p = memory::pool::acquire();
	Context<memory::PoolInterface> ctx;
	ctx.curl = CurlHandle_getHandle(iface.process.reuse, p);
	auto ret = _perform<memory::PoolInterface>(ctx, iface, onBeforePerform, onAfterPerform);
	CurlHandle_releaseHandle(ctx.curl, iface.process.reuse,
			!iface.process.invalidate && ctx.code == CURLE_OK, p);
	return ret;
}

template <>
bool perform(HandleData<memory::StandartInterface> &iface,
		const Callback<bool(CURL *)> &onBeforePerform,
		const Callback<bool(CURL *)> &onAfterPerform) {
	Context<memory::StandartInterface> ctx;
	ctx.curl = CurlHandle_getHandle(iface.process.reuse, nullptr);
	auto ret = _perform<memory::StandartInterface>(ctx, iface, onBeforePerform, onAfterPerform);
	CurlHandle_releaseHandle(ctx.curl, iface.process.reuse,
			!iface.process.invalidate && ctx.code == CURLE_OK, nullptr);
	return ret;
}

} // namespace stappler::network
