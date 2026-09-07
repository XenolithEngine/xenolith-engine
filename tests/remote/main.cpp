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

// Tests for the remote transport's wire layer.
//
// Everything here runs WITHOUT a socket: the message codec, the stream reassembler and the small
// value serializers are pure functions over bytes, so they can be asserted directly. That covers the
// regressions this suite exists for -- a frame that decodes to the wrong length, a reassembler that
// mis-splits a stream, a decompression bomb that gets past the caps.
//
// What is deliberately NOT here yet is the setup handshake (clientHandshake / serverHandshake): it
// only speaks through an `SSL *`, and driving both ends in one process means either two threads or a
// fake transport. The fake transport is the `mem:` scheme from the transport-abstraction milestone,
// and these tests grow the handshake cases when it lands.

#include "SPCommon.h"

#include "tests.h"

using namespace stappler;

// One entry per topic. An array rather than a map so the run order is the order written here (see
// the same note in tests/stappler/main.cpp).
struct TestEntry {
	sprt::StringView name;
	void (*fn)();
};

static const TestEntry s_testList[] = {
	{"address", &stappler::xenolith::remote::performAddressTests},
	{"framing", &stappler::xenolith::remote::performFramingTests},
	{"serialize", &stappler::xenolith::remote::performSerializeTests},
	{"transport", &stappler::xenolith::remote::performTransportTests},
	{"streams", &stappler::xenolith::remote::performStreamTests},
	{"wire", &stappler::xenolith::remote::performWireTests},
	{"peerinfo", &stappler::xenolith::remote::performPeerInfoTests},
};

int main(int argc, const char *argv[]) {
	return perform_main(argc, argv, [&]() -> int {
		if (argc <= 1) {
			for (auto &it : s_testList) { it.fn(); }
		} else {
			for (int i = 1; i < argc; ++i) {
				const TestEntry *found = nullptr;
				for (auto &it : s_testList) {
					if (it.name == sprt::StringView(argv[i])) {
						found = &it;
						break;
					}
				}
				if (found) {
					found->fn();
				} else {
					sprt::cout << "unknown test: " << argv[i] << "\n";
					return 1;
				}
			}
		}
		auto failures = test::failures();
		sprt::cout << (failures ? "FAILED: " : "PASSED, failures: ") << failures << "\n";
		return failures == 0 ? 0 : 1;
	});
}
