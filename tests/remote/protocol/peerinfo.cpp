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

/* GlobalCode::ServerInfo -- who each side is, and the ABI tag that gates the session.
 *
 * The load-bearing assertions here are about the TAG: what it must notice (a struct that changed
 * shape) and what it must not (a build whose structs are identical). Everything the tag protects --
 * the raw InputEventData / WindowLayer dumps -- is memory corruption when it is wrong, so its
 * behaviour is asserted rather than assumed.
 */

#include "SPCommon.h"

#include "XLRemotePeerInfo.h"

#include "SPData.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

void performPeerInfoTests() {
	sprt::cout << "--- remote peer info ---\n";

	{
		auto local = PeerInfo::makeLocal();
		check(local.abi == getLocalAbiTag() && local.abi != 0,
				"peerinfo: a local build reports its own abi tag");
		check(local.platform != OsPlatform::Unknown && local.arch != OsArch::Unknown,
				"peerinfo: platform and arch come from the build, not from a probe");
		check(!local.engineVersion.empty(), "peerinfo: the engine version travels for diagnostics");
		// A client leaves these unset; the fields exist for the server's answer.
		check(local.api == InstanceApi::None && local.wm == WindowSubsystem::Unknown,
				"peerinfo: makeLocal claims no gAPI and no window system");
	}

	{
		// The whole point: two peers of the same build agree, and disagreeing on anything the raw
		// dumps depend on is what ends the session.
		auto a = PeerInfo::makeLocal();
		auto b = PeerInfo::makeLocal();
		check(a.isWireCompatible(b), "peerinfo: two peers of the same build share a wire contract");

		b.abi = a.abi ^ 1;
		check(!a.isWireCompatible(b), "peerinfo: a differing tag is reported as a mismatch");

		// A peer that never filled the field is not "compatible by default". Zero is the value a
		// truncated/absent message decodes to, so treating it as a match would make the check
		// silently vanish exactly when the message was malformed.
		b.abi = 0;
		check(!a.isWireCompatible(b), "peerinfo: an absent abi tag is not a match");
		auto zero = PeerInfo();
		check(!zero.isWireCompatible(zero), "peerinfo: two absent tags do not match each other");
	}

	{
		// The tag must be a COMPILE-TIME value: it is compared before the peer has been allowed to
		// send anything, and a runtime probe could not do that. This line is the assertion -- it
		// fails to compile, not at runtime, if getLocalAbiTag() ever stops being constexpr.
		constexpr uint64_t tag = getLocalAbiTag();
		check(tag != 0, "peerinfo: the tag is a constant expression");

		// The properties of the mixer the tag's usefulness rests on. Two builds whose structs
		// differ must land on different numbers, so the fold has to be sensitive to each value AND
		// to the order they arrive in -- a mixer that was not would let a field swap (same sizes,
		// different offsets) collide with the layout it replaced.
		check(abi::mix(abi::kFnvOffset, 1) != abi::mix(abi::kFnvOffset, 2),
				"peerinfo: the abi mixer distinguishes values");
		check(abi::mix(abi::mix(abi::kFnvOffset, 1), 2)
						!= abi::mix(abi::mix(abi::kFnvOffset, 2), 1),
				"peerinfo: the abi mixer distinguishes order, so a field swap cannot collide");
		check(abi::mix(abi::kFnvOffset, 0) != abi::kFnvOffset,
				"peerinfo: mixing a zero still changes the state");
	}

	{
		PeerInfo info;
		info.engineVersion = String("1.2.3.4");
		info.abi = 0xDEAD'BEEF'0BAD'F00Dull;
		info.debug = true;
		info.platform = OsPlatform::MacOs;
		info.arch = OsArch::Aarch64;
		info.wm = WindowSubsystem::Cocoa;
		info.api = InstanceApi::Metal;
		info.apiVersion = 0x0040'2000;
		info.features = PeerFeatures::FrameCapture | PeerFeatures::FontServer;
		info.transportScheme = String("unix");
		info.transportCaps = TransportCaps::PeerAuthenticated | TransportCaps::Pollable;

		auto val = serializePeerInfo(info);
		// Through actual CBOR bytes, not just the Value: the abi tag is a full 64-bit pattern with
		// the high bit set, and an encoder that clamped or widened it would break the one field
		// that must survive exactly.
		auto bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
		auto back = deserializePeerInfo(data::read<Interface>(bytes));

		checkEq(back.abi, info.abi, "peerinfo: the abi tag survives a CBOR round trip intact");
		checkEq(StringView(back.engineVersion), StringView(info.engineVersion),
				"peerinfo: engine version round-trips");
		check(back.debug == info.debug, "peerinfo: the debug flag round-trips");
		check(back.platform == info.platform && back.arch == info.arch,
				"peerinfo: platform and arch round-trip");
		check(back.wm == info.wm, "peerinfo: the window subsystem round-trips");
		check(back.api == info.api && back.apiVersion == info.apiVersion,
				"peerinfo: the gAPI round-trips");
		check(back.features == info.features, "peerinfo: the feature mask round-trips");
		checkEq(StringView(back.transportScheme), StringView(info.transportScheme),
				"peerinfo: the transport scheme round-trips");
		check(back.transportCaps == info.transportCaps, "peerinfo: transport caps round-trip");
	}

	{
		// A dict, so a peer built before a field existed reads what it knows and ignores the rest.
		// This is what lets ServerInfo grow (device properties, limits, the M6 capability list)
		// without another format version.
		auto info = PeerInfo::makeLocal();
		auto val = serializePeerInfo(info);
		val.setString("something a newer peer added", "futureField");
		auto &gapi = val.emplace("gapi");
		gapi.setInteger(9'999, "someFutureLimit");

		auto back = deserializePeerInfo(val);
		checkEq(back.abi, info.abi, "peerinfo: unknown keys do not disturb the known ones");
		check(back.platform == info.platform, "peerinfo: an extended message still decodes");
	}

	{
		// An empty/garbage message must decode to "nothing was said" rather than to a plausible
		// peer -- the abi check then refuses it, which is the safe direction.
		auto empty = deserializePeerInfo(Value());
		checkEq(empty.abi, uint64_t(0), "peerinfo: an empty message carries no abi tag");
		check(empty.platform == OsPlatform::Unknown && empty.api == InstanceApi::None,
				"peerinfo: an empty message claims nothing");
	}

	{
		check(isApplePlatform(OsPlatform::MacOs) && isApplePlatform(OsPlatform::Ios)
						&& isApplePlatform(OsPlatform::Darwin),
				"peerinfo: isApplePlatform covers the Darwin family, as SPRT_APPLE does");
		check(!isApplePlatform(OsPlatform::Linux) && !isApplePlatform(OsPlatform::Unknown),
				"peerinfo: isApplePlatform is false for everything else");
	}

	{
		using sprt::window::SurfaceBackend;
		check(toWindowSubsystem(SurfaceBackend::Wayland) == WindowSubsystem::Wayland
						&& toWindowSubsystem(SurfaceBackend::Xcb) == WindowSubsystem::Xcb
						&& toWindowSubsystem(SurfaceBackend::Headless) == WindowSubsystem::Headless,
				"peerinfo: the window subsystem maps off SurfaceBackend");
		// A backend the engine has no window path for must map to Unknown rather than to whatever
		// enumerator happens to share its number.
		check(toWindowSubsystem(SurfaceBackend::QNX) == WindowSubsystem::Unknown,
				"peerinfo: an unmapped backend is Unknown, not a wrong answer");
	}
	{
		/* The code masks (M6.3).
		
		 They are maintained by hand beside the enums, which makes them a second source of truth --
		 so what they contain is pinned here. A code added without a handler, or a handler added
		 without its bit, fails an assertion in review rather than becoming a message that quietly
		 does nothing. */
		auto local = PeerInfo::makeLocal();
		check(local.supports(Domain::Window, toInt(WindowCode::InputEvents))
						&& local.supports(Domain::Window, toInt(WindowCode::TextInputState)),
				"peerinfo: this build advertises the window codes it handles");
		check(local.supports(Domain::Data, toInt(DataCode::Cancel)),
				"peerinfo: Cancel is advertised, so a peer knows a transfer can be called off");

		// THE entry that makes the mask more than a restatement of the enum: FontCode::CompileImage
		// is declared and dispatched by nobody.
		check(!local.supports(Domain::Font, toInt(FontCode::CompileImage)),
				"peerinfo: a declared-but-unhandled code is NOT advertised");
		check(local.supports(Domain::Font, toInt(FontCode::GlyphRequest)),
				"peerinfo: the font codes that do have handlers are advertised");

		// Silence is not a claim of emptiness. A peer built before the field says nothing, and
		// refusing to send it anything would turn a missing diagnostic into a dead session.
		PeerInfo quiet;
		check(quiet.supports(Domain::Window, toInt(WindowCode::AcquireFrame)),
				"peerinfo: a peer that advertised nothing is assumed to support everything");

		auto val = serializePeerInfo(local);
		auto bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
		auto back = deserializePeerInfo(data::read<Interface>(bytes));
		check(back.windowCodes == local.windowCodes && back.fontCodes == local.fontCodes,
				"peerinfo: the masks survive a CBOR round trip");

		// And the report names what is missing rather than only that something is.
		auto older = local;
		older.fontCodes &= ~codeBit(FontCode::GlyphRequest);
		StringStream missing;
		local.describeMissingCodes(older, [&](StringView str) { missing << str; });
		check(missing.str().find("font") != maxOf<size_t>()
						&& missing.str().find("3") != maxOf<size_t>(),
				toString("peerinfo: the missing code is named: '", missing.str(), "'"));
	}
}

} // namespace stappler::xenolith::remote