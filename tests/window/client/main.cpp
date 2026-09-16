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

		// Server endpoint: first CLI arg "host:port" (the live-reload server passes it here when it
		// launches us), default 127.0.0.1:4480.
		ctx->setServerAddress(argc > 1 ? StringView(argv[1]) : StringView("127.0.0.1:4480"));

		// Bearer key: derived from a per-session token the live-reload server passes as the 2nd CLI arg
		// (key = Sha512(token)); the shared dev key is the fallback when launched manually with no token.
		if (argc > 2) {
			auto h = crypto::Sha512::perform(StringView(argv[2]));
			ctx->setBearerKey(BytesView(h.data(), h.size()));
		} else {
#if DEBUG
			ctx->setBearerKey(remote::getDevBearerKey());
#else
			// The shared dev key exists only in a debug build (its value is a known constant), so a
			// release client has nothing to fall back on and must be given a token.
			log::source().error("client",
					"no token: a release build carries no development bearer key; pass the session "
					"token as the 2nd argument");
			return 1;
#endif
		}

		// Server identity: the SPKI fingerprint the live-reload server passes as the 3rd CLI arg.
		// Without it the self-signed server certificate is accepted blindly and the bearer key above
		// goes to whoever answered on that port.
		if (argc > 3) {
			ctx->setServerFingerprint(base16::decode<Interface>(StringView(argv[3])));
		}

		ctx->setWindowConnectedCallback([](NotNull<RemoteWindow>) { return true; });

		ctx->run();
		return 0;
	});
}
