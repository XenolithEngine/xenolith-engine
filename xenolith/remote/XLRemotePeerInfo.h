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

// A number two peers must agree on before they may exchange raw struct dumps.
//
// It hashes LAYOUT, not identity: struct sizes, alignments and field offsets, the ranges of the
// enums those fields carry, pointer size and byte order. Deliberately NOT the engine version --
// two revisions whose structs are byte-identical really can talk, and folding the build number in
// would refuse them for no reason while catching nothing extra. The version travels beside the tag
// (PeerInfo::engineVersion) so a human reading a rejection can see the skew.
//
// Enum ranges are in here because a value inserted in the MIDDLE of InputEventName changes what the
// same bytes mean without changing a single offset -- the one silent break that pure layout hashing
// would miss.
constexpr uint64_t getLocalAbiTag() {
	using sprt::window::InputEventData;
	using sprt::window::InputEventName;
	using sprt::window::InputKeyCode;
	using sprt::window::InputMouseButton;
	using sprt::window::WindowCursor;
	using sprt::window::WindowLayer;

	uint64_t h = abi::kFnvOffset;

	h = abi::mix(h, kProtocolVersion);
	h = abi::mix(h, sizeof(void *));
	h = abi::mix(h, __BYTE_ORDER__);

	// WindowCode::InputEvents: [u64 windowId][InputEventData[] native layout]
	h = abi::mix(h, sizeof(InputEventData));
	h = abi::mix(h, alignof(InputEventData));
	h = abi::mix(h, offsetof(InputEventData, id));
	h = abi::mix(h, offsetof(InputEventData, event));
	h = abi::mix(h, offsetof(InputEventData, input));
	h = abi::mix(h, offsetof(InputEventData, input.button));
	h = abi::mix(h, offsetof(InputEventData, input.modifiers));
	h = abi::mix(h, offsetof(InputEventData, input.x));
	h = abi::mix(h, offsetof(InputEventData, input.y));
	h = abi::mix(h, offsetof(InputEventData, point));
	h = abi::mix(h, offsetof(InputEventData, point.density));
	h = abi::mix(h, offsetof(InputEventData, key.keysym));
	h = abi::mix(h, offsetof(InputEventData, key.keychar));
	h = abi::mix(h, offsetof(InputEventData, window.changes));

	h = abi::mix(h, toInt(InputEventName::Max));
	h = abi::mix(h, toInt(InputMouseButton::Max));
	h = abi::mix(h, toInt(InputKeyCode::Max));

	// WindowCode::UpdateLayers: [u64 windowId][WindowLayer[] native layout]
	h = abi::mix(h, sizeof(WindowLayer));
	h = abi::mix(h, alignof(WindowLayer));
	h = abi::mix(h, offsetof(WindowLayer, rect));
	h = abi::mix(h, offsetof(WindowLayer, cursor));
	h = abi::mix(h, offsetof(WindowLayer, flags));
	h = abi::mix(h, toInt(WindowCursor::Max));

	return h;
}

struct SP_PUBLIC PeerInfo {
	// --- engine ---
	String engineVersion; // human-readable; diagnostics only, never gates the session
	uint64_t abi = 0; // getLocalAbiTag() of the peer's build -- the field that DOES gate it
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

	// Everything a build can answer about itself with no window, no loop and no connection.
	// The caller fills in whatever else it knows (wm, api, features, transport).
	static PeerInfo makeLocal();

	// True when this peer may exchange raw struct dumps with `other`.
	bool isAbiCompatible(const PeerInfo &other) const { return abi != 0 && abi == other.abi; }

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
