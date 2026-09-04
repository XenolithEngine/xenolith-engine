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
		// M4 codecs. The interesting cases are not "does a number survive" -- they are the two
		// places where this could be wrong in a way nothing would notice at runtime.
		sprt::window::WindowGeometry g;
		g.rect = sprt::geom::IRect{-100, 40, 1'280, 720};
		g.hasPosition = true;

		auto rg = deserializeWindowGeometry(serializeWindowGeometry(g));
		check(rg.rect.x == -100 && rg.rect.y == 40 && rg.rect.width == 1'280
						&& rg.rect.height == 720,
				"serialize: WindowGeometry rect survives, negative origin included");
		check(rg.hasPosition, "serialize: WindowGeometry carries hasPosition");

		// A zero origin with hasPosition=false must NOT come back as "at the top-left corner":
		// on Wayland and the windowless backends the platform never reports a position, and the
		// flag is the only thing that separates "unknown" from "0,0".
		sprt::window::WindowGeometry unknown;
		unknown.rect = sprt::geom::IRect{0, 0, 800, 600};
		auto ru = deserializeWindowGeometry(serializeWindowGeometry(unknown));
		check(!ru.hasPosition, "serialize: an unknown position stays unknown");
	}

	{
		core::FrameTimingInfo t;
		t.lastFrameInterval = 16'666;
		t.avgFrameInterval = 16'700;
		t.lastFrameTime = 4'200;
		t.lastFenceFrameTime = 3'100;
		t.lastTimestampFrameTime = 2'900;

		auto rt = deserializeFrameTiming(serializeFrameTiming(t));
		check(rt.lastFrameInterval == t.lastFrameInterval
						&& rt.avgFrameInterval == t.avgFrameInterval
						&& rt.lastFrameTime == t.lastFrameTime
						&& rt.lastFenceFrameTime == t.lastFenceFrameTime
						&& rt.lastTimestampFrameTime == t.lastTimestampFrameTime,
				"serialize: FrameTimingInfo round-trips");

		/* THE case this codec exists for. Both these structs grow members under XL_FRAME_ACCOUNT,
		so their size is a build-flag fact -- and the ABI tag hashes only InputEventData and
		WindowLayer, so a server built with the flag and a client built without it connect
		successfully today. A raw dump would corrupt that pair. Field-by-field with the flagged
		members APPENDED means the shorter side reads the prefix it understands. */
		auto truncated = serializeFrameTiming(t);
		while (truncated.size() > 3) { truncated.getArray().pop_back(); }
		auto rp = deserializeFrameTiming(truncated);
		check(rp.lastFrameInterval == t.lastFrameInterval && rp.lastFrameTime == t.lastFrameTime,
				"serialize: a shorter timing array decodes its prefix");
		checkEq(rp.lastFenceFrameTime, uint64_t(0),
				"serialize: missing timing fields read as zero, not as garbage");
	}

	{
		core::DrawStat d{};
		d.vertexes = 12'000;
		d.triangles = 4'000;
		d.drawCalls = 17;
		d.vertexInputTime = 1'234;
		d.pixelsTotal = 800 * 600;
		d.pixelsFilled = 1'000'000; // overdraw: legitimately larger than the target

		auto rd = deserializeDrawStat(serializeDrawStat(d));
		check(rd.vertexes == d.vertexes && rd.triangles == d.triangles
						&& rd.drawCalls == d.drawCalls && rd.vertexInputTime == d.vertexInputTime,
				"serialize: DrawStat counters round-trip");
		check(rd.pixelsTotal == d.pixelsTotal && rd.pixelsFilled == d.pixelsFilled,
				"serialize: DrawStat 64-bit pixel counters round-trip");

		// Most of DrawStat has no default initializer, so a short array must decode to zeros
		// rather than to whatever was on the stack.
		auto empty = deserializeDrawStat(Value());
		check(empty.vertexes == 0 && empty.pixelsTotal == 0 && empty.drawCalls == 0,
				"serialize: a malformed DrawStat decodes to zeros, not to stack contents");
	}

	{
		/* Text input. The cursors are UTF-16 INDICES and the text travels as UTF-8, so the pair
		only stays consistent if the round trip reproduces the same UTF-16 sequence. ASCII would
		never show a mistake here: the two encodings agree on length. This string does not. */
		core::TextInputState st;
		st.string = Rc<sprt::window::TextInputString>::alloc();
		st.string->string = sprt::window::WideString(u"Hiにほ");
		st.cursor = core::TextCursor(4, 2);
		st.marked = core::TextCursor(2, 2);
		st.serial = 0xDEAD'BEEFull;
		st.enabled = true;
		st.type = sprt::window::TextInputType::Text;
		st.compose = sprt::window::InputKeyComposeState::Composing;

		auto rs = deserializeTextInputState(serializeTextInputState(st));
		check(rs.getStringView() == st.getStringView(),
				"serialize: TextInputState text survives UTF-16 -> UTF-8 -> UTF-16");
		checkEq(rs.size(), st.size(), "serialize: and keeps its UTF-16 length");
		check(rs.cursor == st.cursor && rs.marked == st.marked,
				"serialize: TextInputState cursors are unchanged UTF-16 offsets");
		checkEq(rs.serial, st.serial, "serialize: the correlation serial round-trips");
		check(rs.enabled && rs.compose == st.compose,
				"serialize: enabled/compose are carried (getState() drops them)");

		core::TextInputCommand cmd;
		cmd.op = core::TextInputCommandOp::SetMarked;
		cmd.text = sprt::window::WideString(u"にほ");
		cmd.marked = core::TextCursor(0, 2);
		auto rc = deserializeTextInputCommand(serializeTextInputCommand(cmd));
		check(rc.op == cmd.op && rc.text == cmd.text && rc.marked == cmd.marked,
				"serialize: TextInputCommand round-trips");
		// InvalidCursor is a VALUE (the default for an unset range), not an absence. Decoding it as
		// {0,0} would turn "no range given" into "an empty range at the start".
		check(rc.replacement == core::TextCursor::InvalidCursor,
				"serialize: an unset range stays InvalidCursor, not {0,0}");
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
		announce.addValue(
				serializeFrameConstraints(core::FrameConstraints())); // AnnounceConstraints
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
