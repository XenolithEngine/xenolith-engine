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

#include "SPCommon.h"

#include "SPNetworkContext.h"
#include "SPNetworkData.h"
#include "SPNetworkHandle.h"

#ifdef LINUX
#include <sys/types.h>
#include <fcntl.h>
#endif

#define SP_NETWORK_LOG(...)
//#define SP_NETWORK_LOG(...) log::format("Network", __VA_ARGS__)

namespace STAPPLER_VERSIONIZED stappler::network {

static constexpr auto NetworkUserdataKey = "org.stappler.Network.Handle";

struct CurlHandle {
public:
	static CURL *alloc() {
		++s_activeHandles;
		return curl_easy_init();
	}

	static void release(CURL *curl) {
		--s_activeHandles;
		curl_easy_cleanup(curl);
	}

	static CURL *getHandle(bool reuse, memory::pool_t *pool) {
		if (reuse) {
			if (pool) {
				void *data = nullptr;
				memory::pool::userdata_get(&data, NetworkUserdataKey, pool);
				if (!data) {
					data = new CurlHandle();
					memory::pool::userdata_set(data, NetworkUserdataKey, [](void *obj) {
						((CurlHandle *)obj)->~CurlHandle();
						return Status::Ok;
					}, pool);
				}

				return ((CurlHandle *)data)->get();
			} else if (!tl_handle) {
				tl_handle = new CurlHandle();
			}
			return tl_handle->get();
		} else {
			return CurlHandle::alloc();
		}
	}

	static void releaseHandle(CURL *curl, bool reuse, bool success, memory::pool_t *pool) {
		if (!reuse) {
			CurlHandle::release(curl);
		} else if (pool) {
			void *data = nullptr;
			memory::pool::userdata_get(&data, NetworkUserdataKey, pool);
			if (data) {
				if (!success) {
					((CurlHandle *)data)->invalidate(curl);
				} else {
					((CurlHandle *)data)->reset();
				}
			} else {
				CurlHandle::release(curl);
			}
		} else if (tl_handle) {
			if (!success) {
				tl_handle->invalidate(curl);
			} else {
				tl_handle->reset();
			}
		} else {
			CurlHandle::release(curl);
		}
	}

	static uint32_t getActiveHandles() { return s_activeHandles; }

	CurlHandle() { _curl = alloc(); }

	CurlHandle(CurlHandle &&other) : _curl(other._curl) { other._curl = nullptr; }

	CurlHandle &operator=(CurlHandle &&other) {
		_curl = other._curl;
		other._curl = nullptr;
		return *this;
	}

	CurlHandle(const CurlHandle &) = delete;
	CurlHandle &operator=(const CurlHandle &) = delete;

	~CurlHandle() {
		if (_curl) {
			release(_curl);
			_curl = nullptr;
		}
	}

	CURL *get() { return _curl; }
	explicit operator bool() { return _curl != nullptr; }

	void invalidate(CURL *curl) {
		if (_curl == curl) {
			curl_easy_cleanup(_curl);
			_curl = curl_easy_init();
		}
	}

	void reset() {
		if (_curl) {
			curl_easy_reset(_curl);
		}
	}

protected:
	CURL *_curl = nullptr;
	static std::atomic<uint32_t> s_activeHandles;
	static thread_local CurlHandle *tl_handle;
};

std::atomic<uint32_t> CurlHandle::s_activeHandles = 0;
thread_local CurlHandle *CurlHandle::tl_handle = nullptr;

SPUNUSED static CURL *CurlHandle_alloc() { return CurlHandle::alloc(); }

SPUNUSED static void CurlHandle_release(CURL *curl) { CurlHandle::release(curl); }

SPUNUSED static CURL *CurlHandle_getHandle(bool reuse, memory::pool_t *pool) {
	return CurlHandle::getHandle(reuse, pool);
}

SPUNUSED static void CurlHandle_releaseHandle(CURL *curl, bool reuse, bool success,
		memory::pool_t *pool) {
	CurlHandle::releaseHandle(curl, reuse, success, pool);
}

uint32_t getActiveHandles() { return CurlHandle::getActiveHandles(); }

} // namespace stappler::network

#include "SPNetworkCABundle.cc"
#include "SPNetworkSetup.cc"
#include "SPNetworkData.cc"
#include "SPNetworkHandle.cc"

//#include "SPNetworkMultiHandle.cc"
