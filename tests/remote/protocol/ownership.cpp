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


// Who may see and take a shared window when a server has several clients. The registry never
// dereferences a window, so stand-in pointers are enough.

#include "SPCommon.h"

#include "XLRemoteObject.h"
#include "XLCoreQueue.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;

static void performWindowLifetimeTests();
static void performQueueScopeTests();

static void performWindowLifetimeTests();
static void performQueueScopeTests();

static core::RenderServerChannel *fakeWindow(uintptr_t value) {
	return reinterpret_cast<core::RenderServerChannel *>(value);
}

void performOwnershipTests() {
	sprt::cout << "--- remote window ownership ---\n";

	auto registry = Rc<ObjectRegistry>::create();
	registry->shareWindow(fakeWindow(0x1000), SpanView<core::Queue *>(), {});
	registry->shareWindow(fakeWindow(0x2000), SpanView<core::Queue *>(), {});
	auto primary = registry->get(fakeWindow(0x1000));
	auto second = registry->get(fakeWindow(0x2000));

	check(primary != 0 && second != 0 && primary != second, "ownership: windows get ids");
	check(registry->isWindowVisible(primary, 1) && registry->isWindowVisible(primary, 2),
			"ownership: an unassigned window is visible to every session");
	check(!registry->isWindowVisible(primary, 0) && !registry->isWindowVisible(12'345, 1),
			"ownership: no session and no window see nothing");

	check(registry->claimWindow(primary, 1), "ownership: the first session takes the window");
	check(registry->isWindowVisible(primary, 1) && !registry->isWindowVisible(primary, 2),
			"ownership: a taken window is visible to its owner only");
	check(!registry->claimWindow(primary, 2), "ownership: another session can not take it");
	check(registry->claimWindow(primary, 1), "ownership: the owner may take it again");

	check(registry->assignWindow(second, 2) && !registry->assignWindow(12'345, 2),
			"ownership: a known window can be reserved");
	check(!registry->isWindowVisible(second, 1) && registry->isWindowVisible(second, 2),
			"ownership: a reserved window is visible to its session only");
	check(!registry->claimWindow(second, 1) && registry->claimWindow(second, 2),
			"ownership: only its session takes a reserved window");

	auto released = registry->releaseSession(1);
	check(released.size() == 1 && released.front() == primary,
			"ownership: a leaving session releases the windows it owned");
	check(registry->isWindowVisible(primary, 2) && registry->claimWindow(primary, 2),
			"ownership: a released window is open to the others");

	registry->releaseSession(2);
	auto &info = registry->getWindows().find(second)->second;
	check(info.ownerSession == 0 && info.assignedSession == 0
					&& registry->isWindowVisible(second, 3),
			"ownership: a reservation ends with its session");

	performWindowLifetimeTests();
}

// What a window takes with it when it goes. A server that opens a window per client request runs
// this cycle for every window a client ever asks for, so whatever is left behind accumulates.
static void performWindowLifetimeTests() {
	auto registry = Rc<ObjectRegistry>::create();

	auto own = Rc<core::Queue>::create(core::Queue::Builder("own"));
	auto shared = Rc<core::Queue>::create(core::Queue::Builder("shared"));
	if (!own || !shared) {
		check(false, "ownership: queues built");
		return;
	}

	core::Queue *first[] = {own.get(), shared.get()};
	core::Queue *second[] = {shared.get()};
	registry->shareWindow(fakeWindow(0x3'000), makeSpanView(first, 2), {});
	registry->shareWindow(fakeWindow(0x4'000), makeSpanView(second, 1), {});

	auto ownId = registry->get(own.get());
	auto sharedId = registry->get(shared.get());
	check(ownId != 0 && sharedId != 0, "ownership: window queues are shared");

	registry->drop(fakeWindow(0x3'000));
	check(registry->resolveQueue(ownId) == nullptr,
			"ownership: a dropped window releases the queue only it used");
	check(registry->resolveQueue(sharedId) != nullptr,
			"ownership: a queue another window still lists survives");

	registry->drop(fakeWindow(0x4'000));
	check(registry->resolveQueue(sharedId) == nullptr,
			"ownership: the last window using a queue releases it");

	performQueueScopeTests();
}

// The gAPI objects a queue's encoding mints. They used to live until the listener stopped, so a
// server opening a window per client request grew by a queue's worth of objects per window.
static void performQueueScopeTests() {
	auto registry = Rc<ObjectRegistry>::create();

	auto queue = Rc<core::Queue>::create(core::Queue::Builder("scoped"));
	if (!queue) {
		check(false, "ownership: scope queue built");
		return;
	}
	core::Queue *queues[] = {queue.get()};
	registry->shareWindow(fakeWindow(0x5'000), makeSpanView(queues, 1), {});
	auto queueId = registry->get(queue.get());

	auto scoped = Rc<Image>::create(1, core::ImageInfoData());
	auto loose = Rc<Image>::create(2, core::ImageInfoData());
	uint64_t scopedId = 0;
	{
		ObjectRegistry::QueueScope scope(*registry, queueId);
		scopedId = registry->share(scoped.get());
	}
	auto looseId = registry->share(loose.get()); // outside any queue: the font atlas's shape

	check(scopedId != 0 && looseId != 0 && registry->resolveObject(scopedId) == scoped.get(),
			"ownership: objects are shared inside and outside a queue scope");

	registry->drop(fakeWindow(0x5'000));
	check(registry->resolveObject(scopedId) == nullptr,
			"ownership: a queue takes the objects its encoding minted");
	check(registry->resolveObject(looseId) == loose.get(),
			"ownership: an object shared outside a queue outlives it");
}

} // namespace stappler::xenolith::remote
