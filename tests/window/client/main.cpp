/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLCommon.h"
#include "XLClientContext.h"
#include "XLRemoteProtocol.h" // remote::getDevBearerKey
#include "SPCoreCrypto.h" // crypto::Sha512

using namespace sp;
using namespace sp::xenolith;

int main(int argc, const char *argv[]) {
	perform_main(argc, argv, [&] {
		auto ctx = Rc<ClientContext>::create();

		// Server endpoint: first CLI arg "host:port", default 127.0.0.1:4480.
		//ctx->setServerAddress(argc > 1 ? StringView(argv[1]) : StringView("127.0.0.1:4480"));
		ctx->setServerAddress(StringView("127.0.0.1:4480"));

		// Bearer key: shared dev key by default; an optional 2nd arg overrides it (Sha512(arg)) to
		// exercise the auth-failure path.
		//if (argc > 2) {
		//	auto h = crypto::Sha512::perform(StringView(argv[2]));
		//	ctx->setBearerKey(BytesView(h.data(), h.size()));
		//} else {
		ctx->setBearerKey(remote::getDevBearerKey());
		//}

		ctx->setWindowConnectedCallback([](NotNull<RemoteWindow>) { return true; });

		ctx->run();
		return 0;
	});
}
