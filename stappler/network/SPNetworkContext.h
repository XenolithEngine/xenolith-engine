/**
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

#ifndef STAPPLER_NETWORK_SPNETWORKCONTEXT_H_
#define STAPPLER_NETWORK_SPNETWORKCONTEXT_H_

#include "SPNetworkHandle.h"

#if WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define CURL_STATICLIB
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wcomment"
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#include "curl/curl.h"

#if XWIN
#pragma clang diagnostic pop
#endif

namespace STAPPLER_VERSIONIZED stappler::network {

template <typename Interface>
struct SP_PUBLIC Context {
	Rc<Ref> userdata;
	CURL *curl = nullptr;
	CURLSH *share = nullptr;
	Handle<Interface> *origHandle = nullptr;
	HandleData<Interface> *handle = nullptr;
	typename HandleData<Interface>::template Vector<typename HandleData<Interface>::String> headersData;

	curl_slist *headers = nullptr;
	curl_slist *mailTo = nullptr;

	FILE *inputFile = nullptr;
	FILE *outputFile = nullptr;
	uint64_t inputPos = 0;

	int code = 0;
	bool success = false;
	std::array<char, 256> error = { 0 };
};

template <typename Interface>
SP_PUBLIC bool prepare(HandleData<Interface> &iface, Context<Interface> *ctx, const Callback<bool(CURL *)> &onBeforePerform);

template <typename Interface>
SP_PUBLIC bool finalize(HandleData<Interface> &iface, Context<Interface> *ctx, const Callback<bool(CURL *)> &onAfterPerform);

template <typename Interface>
SP_PUBLIC bool perform(HandleData<Interface> &iface,
		const Callback<bool(CURL *)> &onBeforePerform, const Callback<bool(CURL *)> &onAfterPerform);

}

#endif /* STAPPLER_NETWORK_SPNETWORKCONTEXT_H_ */
