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

/* The small value serializers, and the ANNOUNCE LAYOUT they are read out of.
 *
 * The layout assertion is the point of this file. The window announce is a positional array built
 * in RemoteRenderClient::announce and taken apart in RemoteWindow::init, with nothing but the two
 * pieces of code agreeing on what each index means -- and they did not: the WindowInfo was written
 * at index 7 and read from index 6, so every client-side WindowInfo was silently decoded out of the
 * queue array. Nothing failed loudly; getInfo() just answered nonsense.
 *
 * So the indices are pinned here, by name, against a value shaped exactly like the real announce.
 */

#include "SPCommon.h"

#include "SPData.h" // complete data::Value for the announce shape below
#include "XLRemoteSerialize.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

// The announce array's positions, as RemoteRenderClient::announce writes them. Typed size_t rather
// than an enum: Value::getValue has both an index and a string-key overload, and an enum picks the
// wrong one.
static constexpr size_t AnnounceId = 0;
static constexpr size_t AnnounceWindowId = 1;
static constexpr size_t AnnounceState = 2;
static constexpr size_t AnnounceCapabilities = 3;
static constexpr size_t AnnounceConstraints = 4;
static constexpr size_t AnnounceSwapchain = 5;
static constexpr size_t AnnounceQueues = 6;
static constexpr size_t AnnounceWindowInfo = 7;

void performSerializeTests() {
	sprt::cout << "--- remote serialize ---\n";

	{
		core::FrameConstraints c;
		c.extent = Extent3(1'280, 720, 1);
		c.density = 2.0f;
		c.surfaceDensity = 1.5f;
		c.frameInterval = 16'666;

		auto restored = deserializeFrameConstraints(serializeFrameConstraints(c));
		check(restored.extent.width == 1'280 && restored.extent.height == 720
						&& restored.extent.depth == 1,
				"serialize: FrameConstraints extent");
		check(restored.density == c.density && restored.surfaceDensity == c.surfaceDensity,
				"serialize: FrameConstraints density");
		checkEq(restored.frameInterval, c.frameInterval, "serialize: FrameConstraints interval");
	}

	{
		core::SwapchainConfig cfg;
		cfg.extent = Extent2(800, 600);
		cfg.imageCount = 3;

		auto restored = deserializeSwapchainConfig(serializeSwapchainConfig(cfg));
		check(restored.extent.width == 800 && restored.extent.height == 600,
				"serialize: SwapchainConfig extent");
		checkEq(restored.imageCount, cfg.imageCount, "serialize: SwapchainConfig imageCount");
	}

	{
		// An announce shaped like the real one: the WindowInfo goes LAST, after the queue array, and
		// the two must not be confused for one another. A queue array decoded as a WindowInfo is
		// exactly the bug this pins down, so the two are given clearly different content.
		Value announce;
		announce.addInteger(17); // AnnounceId
		announce.addString("main"); // AnnounceWindowId
		announce.addInteger(0); // AnnounceState
		announce.addInteger(0); // AnnounceCapabilities
		announce.addValue(serializeFrameConstraints(core::FrameConstraints())); // AnnounceConstraints
		announce.addValue(serializeSwapchainConfig(core::SwapchainConfig())); // AnnounceSwapchain

		auto &queues = announce.emplace(); // AnnounceQueues
		auto &q = queues.emplace();
		q.addInteger(101);
		q.addString("RemoteClientQueue");

		sprt::window::WindowInfo info;
		info.title = "announced window";
		announce.addValue(serializeWindowInfo(info)); // AnnounceWindowInfo

		check(announce.size() == 8, "announce: eight positional entries");
		check(announce.getValue(AnnounceQueues).isArray()
						&& announce.getValue(AnnounceQueues).size() == 1,
				"announce: index 6 is the queue array");
		// Both entries are ARRAYS -- serializeWindowInfo is a flat positional array too -- so nothing
		// about their shape tells them apart. That is precisely why reading the WindowInfo from index
		// 6 produced no error of any kind; only the field COUNT differs.
		check(announce.getValue(AnnounceWindowInfo).isArray()
						&& announce.getValue(AnnounceWindowInfo).size() == 18,
				"announce: index 7 is the WindowInfo's 18 fields");

		// What RemoteWindow::init does, at the index it must use.
		auto restored = deserializeWindowInfo(announce.getValue(AnnounceWindowInfo));
		check(restored != nullptr, "announce: WindowInfo decodes from index 7");
		if (restored) {
			checkEq(StringView(restored->title), StringView("announced window"),
					"announce: WindowInfo survives the round trip");
		}

		// And the regression itself: the queue array is NOT a WindowInfo. Decoding it must not yield
		// the announced window -- which is what the off-by-one silently did.
		auto wrong = deserializeWindowInfo(announce.getValue(AnnounceQueues));
		check(!wrong || StringView(wrong->title) != StringView("announced window"),
				"announce: the queue array does not decode as the WindowInfo");
	}
}

} // namespace stappler::xenolith::remote
