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

#ifndef XENOLITH_REMOTE_XLREMOTEPEERINFO_H_
#define XENOLITH_REMOTE_XLREMOTEPEERINFO_H_

#include "XLRemoteProtocol.h"

#include <sprt/runtime/window/gapi.h>
#include <sprt/runtime/window/input.h>
#include <sprt/runtime/window/window_info.h>

#include <stddef.h> // offsetof

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using sprt::window::gapi::InstanceApi;

/* Who each side is, exchanged over Domain::Global right after the handshake.
 *
 * Until the wire format becomes build-independent (M6) the two sides memcpy structs at each other:
 * WindowCode::InputEvents ships a raw `core::InputEventData[]` and WindowCode::UpdateLayers a raw
 * `sprt::window::WindowLayer[]`. A layout disagreement there is not a protocol error that surfaces
 * as a rejected message -- it is silent memory corruption in whichever process reads the blob. So
 * the ABI tag below is checked BEFORE anything is announced, and a mismatch ends the session.
 *
 * The rest of the message answers the question a remote scene cannot answer locally: the scene runs
 * on the client, but the window, the GPU and the OS are the server's. See §8 of the plan.
 */

// Numbering deliberately mirrors the runtime's own __SPRT_PLATFORM_ID_*: the runtime already assigns
// every platform a stable id, and a second independent numbering would be one more table to keep in
// sync. It is still a distinct type, because this one is a WIRE FORMAT -- a value here may never be
// renumbered even if the runtime's ids ever were. The static_asserts below are what would notice.
enum class OsPlatform : uint8_t {
	Unknown = 0,
	MacOs = 1,
	Ios = 2,
	Darwin = 3, // some other Darwin/XNU platform
	Windows = 4,
	Android = 5,
	Linux = 6,
	Wasm = 7,
	Nuttx = 8,
	Embox = 9,
};

enum class OsArch : uint8_t {
	Unknown = 0,
	Aarch64 = 1,
	Arm = 2,
	X86 = 3,
	X86_64 = 4,
	E2k = 5,
	Wasm64 = 6,
	Wasm32 = 7,
	Riscv64 = 8,
	Riscv32 = 9,
	Loongarch64 = 10,
	Loongarch32 = 11,
};

// The window system the server's window actually lives on. Deliberately NOT sprt::window::
// SurfaceBackend: that is an engine enum which may grow a value in the middle, and this one is on
// the wire. toWindowSubsystem() is the single mapping point.
enum class WindowSubsystem : uint8_t {
	Unknown = 0,
	Headless = 1, // no window system at all (pseudo-swapchain)
	Xcb = 2,
	Wayland = 3,
	Win32 = 4,
	Cocoa = 5, // macOS / AppKit
	UiKit = 6, // iOS
	Android = 7,
	Canvas = 8, // browser
	Display = 9, // direct-to-display (KMS), no compositor
};

// What the peer can do, as opposed to what it is. A client asks these instead of inferring
// behaviour from the platform: "macOS" does not mean "can capture a frame", and a headless server
// on Linux answers differently from a windowed one.
enum class PeerFeatures : uint64_t {
	None = 0,
	FrameCapture = 1 << 0, // server can hand back the window's pixels (Domain::Data screenshot)
	FontServer = 1 << 1, // server rasterizes glyphs for the client (Domain::Font)
	Subwindows = 1 << 2, // server's window system has real popups/dialogs
	Clipboard = 1 << 3, // server exposes clipboard services (M7)
	// Damage/partial redraw is deliberately NOT here: it is a property of a QUEUE, not of the peer,
	// and it is already announced per queue (RemoteQueueInfo::damage). A peer-level bit would be a
	// second, coarser answer to a question that already has a precise one.
};

SP_DEFINE_ENUM_AS_MASK(PeerFeatures)

namespace abi {

// FNV-1a, eight bytes at a time. Only used to fold the facts below into one number; nothing
// depends on it being a good hash beyond "a changed input changes the output".
constexpr uint64_t kFnvOffset = 14'695'981'039'346'656'037ull;
constexpr uint64_t kFnvPrime = 1'099'511'628'211ull;

constexpr uint64_t mix(uint64_t h, uint64_t value) {
	for (int i = 0; i < 8; ++i) {
		h ^= (value >> (i * 8)) & 0xFF;
		h *= kFnvPrime;
	}
	return h;
}

} // namespace abi

/* A fingerprint of the WIRE CONTRACT this build was compiled against.
 *
 * It used to hash LAYOUT -- struct sizes, alignments, field offsets -- and it gated the session,
 * because InputEvents and UpdateLayers were raw dumps of that layout and a disagreement was one
 * process reading another's padding as a keycode. Neither is true any more: those messages are
 * field-by-field now (XLRemoteSerialize.h), so what the compiler did with a struct stopped being
 * visible on the wire at all, and hashing it would refuse builds that can talk perfectly well.
 *
 * What remains is the part that a typed format does NOT fix. `event` rides as an integer, so two
 * builds still have to agree on what number 7 means; a value inserted in the MIDDLE of
 * InputEventName changes the meaning of the same bytes without changing any field. The enum
 * CEILINGS below catch a value appended past them, and the record sizes catch a format that grew --
 * both worth reporting. The middle insertion, the one that is genuinely silent, no hash can see:
 * that is pinned by tests/remote instead, where a failure names the value that moved.
 *
 * So this is DIAGNOSTIC. A mismatch is logged on both sides and the session continues -- see
 * PeerInfo::isWireCompatible and its callers. Deliberately not the engine version: two revisions
 * with the same wire contract really can talk, and the version travels beside it
 * (PeerInfo::engineVersion) for a human reading the log.
 */
constexpr uint64_t getLocalAbiTag() {
	using sprt::window::InputEventName;
	using sprt::window::InputKeyCode;
	using sprt::window::InputMouseButton;
	using sprt::window::WindowCursor;

	uint64_t h = abi::kFnvOffset;

	// The shape of what travels, not the shape of what is in memory.
	h = abi::mix(h, kInputEventRecordSize);
	h = abi::mix(h, kWindowLayerRecordSize);

	h = abi::mix(h, toInt(InputEventName::Max));
	h = abi::mix(h, toInt(InputMouseButton::Max));
	h = abi::mix(h, toInt(InputKeyCode::Max));
	h = abi::mix(h, toInt(WindowCursor::Max));

	return h;
}

struct SP_PUBLIC PeerInfo {
	// --- engine ---
	String engineVersion; // human-readable; diagnostics only, never gates the session
	// getLocalAbiTag() of the peer's build. DIAGNOSTIC since M6: a mismatch is worth saying out loud
	// but no longer ends the session -- see isWireCompatible.
	uint64_t abi = 0;
	bool debug = false;

	// --- host ---
	OsPlatform platform = OsPlatform::Unknown;
	OsArch arch = OsArch::Unknown;
	WindowSubsystem wm = WindowSubsystem::Unknown;

	// --- gAPI: the server's, and only the server's. A client draws nothing itself, so it leaves
	// these at None and the server never reads them.
	InstanceApi api = InstanceApi::None;
	uint32_t apiVersion = 0;

	PeerFeatures features = PeerFeatures::None;

	// --- transport: what this side believes it is talking over. The two sides can legitimately
	// disagree in wording (a `mem:` pair, a proxy); it is diagnostics, not negotiation.
	String transportScheme;
	TransportCaps transportCaps = TransportCaps::None;

	/* --- which message codes the peer's build implements, one bit per code per domain ---------
	
	The mechanism the milestone's acceptance is about: two builds that differ can now say WHICH
	messages they differ about, instead of one of them sending something and reading NotImplemented
	back a round trip later. Absent from a peer that predates the field, which decodes as all-zero --
	and zero is read as "said nothing", not as "supports nothing" (see supports()). */
	uint64_t globalCodes = 0;
	uint64_t windowCodes = 0;
	uint64_t dataCodes = 0;
	uint64_t fontCodes = 0;

	// Whether the peer implements `code` in `domain`.
	//
	// A peer that advertised nothing at all is treated as supporting everything: silence has to mean
	// "I did not say", because the alternative -- refusing to send anything to a peer that never
	// filled the field -- would turn a missing diagnostic into a dead session.
	bool supports(Domain domain, uint8_t code) const;

	// The codes `other` is missing relative to this build, as a human-readable list. Empty when the
	// two agree, which is the normal case and the reason this is a log line rather than a check.
	void describeMissingCodes(const PeerInfo &other, const Callback<void(StringView)> &) const;

	// Everything a build can answer about itself with no window, no loop and no connection.
	// The caller fills in whatever else it knows (wm, api, features, transport).
	static PeerInfo makeLocal();

	// True when this peer may exchange raw struct dumps with `other`.
	/* Whether the two builds were compiled against the same wire contract.
	
	Renamed from isAbiCompatible along with what it means: it no longer answers "may these two
	memcpy structs at each other", because nothing does that any more. It answers "do these two
	agree about the ceilings of the enums they send each other as integers", which is worth a line
	in the log and is NOT worth refusing a session over -- the tag cannot see the one divergence
	that would actually hurt (a value inserted mid-enum), so refusing on it would be theatre.
	
	A zero tag is still not a match: zero is what a truncated or absent message decodes to, and
	treating it as agreement would make the check vanish exactly when the message was malformed. */
	bool isWireCompatible(const PeerInfo &other) const { return abi != 0 && abi == other.abi; }

	void description(const Callback<void(StringView)> &) const;
};

// A CBOR dict, not an array: this message is expected to grow (device properties, limits, the
// domain/code capability list in M6), and a reader that ignores keys it does not know keeps working
// against a newer peer. Every other structure on this wire is a positional array because it is
// fixed; this one is not.
SP_PUBLIC Value serializePeerInfo(const PeerInfo &);
SP_PUBLIC PeerInfo deserializePeerInfo(const Value &);

// The Darwin family, the way `SPRT_APPLE` means it -- what a caller wants when the behaviour is
// libSystem/AppKit-shaped rather than specific to one Apple OS.
constexpr bool isApplePlatform(OsPlatform p) {
	return p == OsPlatform::MacOs || p == OsPlatform::Ios || p == OsPlatform::Darwin;
}

SP_PUBLIC StringView getOsPlatformName(OsPlatform);
SP_PUBLIC StringView getOsArchName(OsArch);
SP_PUBLIC StringView getWindowSubsystemName(WindowSubsystem);

// The only place SurfaceBackend crosses into the wire enum.
SP_PUBLIC WindowSubsystem toWindowSubsystem(sprt::window::SurfaceBackend);

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEPEERINFO_H_ */
