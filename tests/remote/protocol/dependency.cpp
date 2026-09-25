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


// The client's font batch dependency: nothing on the client signals it, the server's AtlasReady
// does. It must start pending, or the frames that need the batch drop it and are never held.

#include "SPCommon.h"

#include "XLCoreAttachment.h"
#include "XLCoreQueue.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;

void performDependencyTests() {
	sprt::cout << "--- dependency events signalled by a peer ---\n";

	auto local = Rc<core::DependencyEvent>::alloc(core::DependencyEvent::QueueSet{}, "test");
	check(local->isSignaled(), "dependency: an event with nothing to wait on starts signalled");

	auto external =
			Rc<core::DependencyEvent>::alloc(core::DependencyEvent::ExternalSignal{}, "test");
	check(!external->isSignaled(), "dependency: an event a peer signals starts pending");

	uint32_t fired = 0;
	external->setSignalCallback([&] { ++fired; });
	check(external->signal(nullptr, false), "dependency: the peer's answer signals it");
	check(external->isSignaled() && !external->isSuccessful(),
			"dependency: a failed answer is kept");
	external->signal(nullptr, true);
	check(fired == 1, "dependency: the callback fires once");
}

} // namespace stappler::xenolith::remote
