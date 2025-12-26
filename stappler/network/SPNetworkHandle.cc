/**
Copyright (c) 2016-2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPNetworkHandle.h"

#include "SPString.h"
#include "SPStringView.h"
#include "SPLog.h"
#include "SPValid.h"
#include "SPCrypto.h"
#include "SPFilesystem.h"
#include "SPNetworkContext.h"

#if MODULE_STAPPLER_BITMAP
#include "SPBitmap.h"
#endif

#include "curl/curl.h"

namespace STAPPLER_VERSIONIZED stappler::network {

SPUNUSED static CURL *CurlHandle_alloc();
SPUNUSED static void CurlHandle_release(CURL *curl);

template <>
bool Handle<memory::PoolInterface>::init(Method method, StringView url) {
	if (url.size() == 0 || method == Method::Unknown) {
		return false;
	}

	send.url = url.str<memory::PoolInterface>();
	send.method = method;
	return true;
}

template <>
bool Handle<memory::StandartInterface>::init(Method method, StringView url) {
	if (url.size() == 0 || method == Method::Unknown) {
		return false;
	}

	send.url = url.str<memory::StandartInterface>();
	send.method = method;
	return true;
}

template <>
bool Handle<memory::PoolInterface>::perform() {
	return network::perform(*this, nullptr, nullptr);
}

template <>
bool Handle<memory::StandartInterface>::perform() {
	return network::perform(*this, nullptr, nullptr);
}

template <>
bool MultiHandle<memory::PoolInterface>::perform(
		const Callback<bool(Handle<memory::PoolInterface> *, Ref *)> &cb) {
	auto m = curl_multi_init();
	memory::PoolInterface::MapType<CURL *, Context<memory::PoolInterface>> handles;

	auto initPending = [&, this]() -> int {
		for (auto &it : pending) {
			auto h = CurlHandle_alloc();
			auto i = handles.emplace(h, Context<memory::PoolInterface>()).first;
			i->second.userdata = it.second;
			i->second.curl = h;
			i->second.origHandle = it.first;
			network::prepare(*it.first->getData(), &i->second, nullptr);

			curl_multi_add_handle(m, h);
		}
		auto s = pending.size();
		pending.clear();
		return int(s);
	};

	auto cancel = [&] {
		for (auto &it : handles) {
			curl_multi_remove_handle(m, it.first);
			it.second.code = CURLE_FAILED_INIT;
			network::finalize(*it.second.handle, &it.second, nullptr);

			CurlHandle_release(it.first);
		}
		curl_multi_cleanup(m);
	};

	handles.reserve(pending.size()); // mem_pool has non-std map::reserve

	int running = initPending();
	do {
		auto err = curl_multi_perform(m, &running);
		if (err != CURLM_OK) {
			log::source().error("CURL", "Fail to perform multi: ", err);
			return false;
		}

		if (running > 0) {
			err = curl_multi_poll(m, NULL, 0, 1'000, nullptr);
			if (err != CURLM_OK) {
				log::source().error("CURL", "Fail to poll multi: ", err);
				return false;
			}
		}

		struct CURLMsg *msg = nullptr;
		do {
			int msgq = 0;
			msg = curl_multi_info_read(m, &msgq);
			if (msg && (msg->msg == CURLMSG_DONE)) {
				CURL *e = msg->easy_handle;
				curl_multi_remove_handle(m, e);

				auto it = handles.find(e);
				if (it != handles.end()) {
					it->second.code = msg->data.result;
					network::finalize(*it->second.handle, &it->second, nullptr);
					if (cb) {
						if (!cb(it->second.origHandle, it->second.userdata)) {
							handles.erase(it);
							cancel();
							return false;
						}
					}
					handles.erase(it);
				}

				CurlHandle_release(e);
			}
		} while (msg);

		running += initPending();
	} while (running > 0);

	curl_multi_cleanup(m);
	return true;
}

template <>
bool MultiHandle<memory::StandartInterface>::perform(
		const Callback<bool(Handle<memory::StandartInterface> *, Ref *)> &cb) {
	auto m = curl_multi_init();
	memory::StandartInterface::MapType<CURL *, Context<memory::StandartInterface>> handles;

	auto initPending = [&, this]() -> int {
		for (auto &it : pending) {
			auto h = CurlHandle_alloc();
			auto i = handles.emplace(h, Context<memory::StandartInterface>()).first;
			i->second.userdata = it.second;
			i->second.curl = h;
			i->second.origHandle = it.first;
			network::prepare(*it.first->getData(), &i->second, nullptr);

			curl_multi_add_handle(m, h);
		}
		auto s = pending.size();
		pending.clear();
		return int(s);
	};

	auto cancel = [&] {
		for (auto &it : handles) {
			curl_multi_remove_handle(m, it.first);
			it.second.code = CURLE_FAILED_INIT;
			network::finalize(*it.second.handle, &it.second, nullptr);

			CurlHandle_release(it.first);
		}
		curl_multi_cleanup(m);
	};

	// handles.reserve(pending.size());

	int running = initPending();
	do {
		auto err = curl_multi_perform(m, &running);
		if (err != CURLM_OK) {
			log::source().error("CURL",
					string::toString<memory::StandartInterface>("Fail to perform multi: ", err));
			return false;
		}

		if (running > 0) {
			err = curl_multi_poll(m, NULL, 0, 1'000, nullptr);
			if (err != CURLM_OK) {
				log::source().error("CURL",
						string::toString<memory::StandartInterface>("Fail to poll multi: ", err));
				return false;
			}
		}

		struct CURLMsg *msg = nullptr;
		do {
			int msgq = 0;
			msg = curl_multi_info_read(m, &msgq);
			if (msg && (msg->msg == CURLMSG_DONE)) {
				CURL *e = msg->easy_handle;
				curl_multi_remove_handle(m, e);

				auto it = handles.find(e);
				if (it != handles.end()) {
					it->second.code = msg->data.result;
					network::finalize(*it->second.handle, &it->second, nullptr);
					if (cb) {
						if (!cb(it->second.origHandle, it->second.userdata)) {
							handles.erase(it);
							cancel();
							return false;
						}
					}
					handles.erase(it);
				}

				CurlHandle_release(e);
			}
		} while (msg);

		running += initPending();
	} while (running > 0);

	curl_multi_cleanup(m);
	return true;
}

} // namespace stappler::network
