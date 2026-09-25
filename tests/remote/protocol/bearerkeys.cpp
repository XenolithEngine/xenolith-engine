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

// Keys a server issues with a label (a launch token per process it starts): the label of the key a
// client presents is who that client is.

#include "SPCommon.h"

#include "XLRemoteBearerKeys.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;

void performBearerKeyTests() {
	sprt::cout << "--- remote labelled bearer keys ---\n";

	Bytes shellKey(kBearerKeySize, uint8_t(0x11));
	Bytes appKey(kBearerKeySize, uint8_t(0x22));
	Bytes strangerKey(kBearerKeySize, uint8_t(0x33));

	{
		BearerKeyTable keys;
		keys.add(shellKey, "shell", false);
		keys.add(appKey, "app:form:1", true);

		String label;
		check(keys.match(shellKey, label) && label == "shell",
				"bearerkeys: a key matches its own label");
		check(keys.match(appKey, label) && label == "app:form:1",
				"bearerkeys: each key keeps its label");
		label.clear();
		check(!keys.match(strangerKey, label) && label.empty(),
				"bearerkeys: an unknown key matches nothing");
		check(!keys.match(BytesView(), label), "bearerkeys: an empty key matches nothing");
		check(!keys.match(BytesView(shellKey).sub(0, 32), label),
				"bearerkeys: a prefix of a key does not match");
	}

	{
		BearerKeyTable keys;
		keys.add(appKey, "app", true);
		keys.add(shellKey, "shell", false);
		keys.consume("app");
		keys.consume("shell");
		String label;
		check(!keys.match(appKey, label) && keys.match(shellKey, label) && keys.size() == 1,
				"bearerkeys: a single-use key stops matching once consumed, a reusable one stays");
	}

	{
		BearerKeyTable keys;
		keys.add(shellKey, "shell", false);
		keys.add(appKey, "shell", false);
		String label;
		check(keys.size() == 1 && !keys.match(shellKey, label) && keys.match(appKey, label),
				"bearerkeys: adding under an existing label replaces its key");
		check(keys.remove("shell") && keys.empty() && !keys.remove("shell"),
				"bearerkeys: remove drops the label once");
	}
}

} // namespace stappler::xenolith::remote
