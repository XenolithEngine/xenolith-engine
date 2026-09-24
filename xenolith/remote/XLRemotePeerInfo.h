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

/* Who each side is, exchanged over Domain::Global right after the handshake: build and wire
 * contract, host platform, the server's window system and gAPI, features and supported codes.
 * The scene runs on the client, but the window, the GPU and the OS are the server's.
 */

// Numbering mirrors the runtime's __SPRT_PLATFORM_ID_*. A distinct type because it is a wire
// format: values must never be renumbered, even if the runtime's are (checked by static_asserts).
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

// The window system the server's window lives on. Not sprt::window::SurfaceBackend, which may grow
// values in the middle; this one is on the wire. toWindowSubsystem() is the single mapping point.
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

// What the peer can do, as opposed to what it is; clients test these instead of inferring from
// the platform.
enum class PeerFeatures : uint64_t {
	None = 0,
	FrameCapture = 1 << 0, // server can hand back the window's pixels (Domain::Data screenshot)
	FontServer = 1 << 1, // server rasterizes glyphs for the client (Domain::Font)
	Subwindows = 1 << 2, // server's window system has real popups/dialogs
	Clipboard = 1 << 3, // server exposes clipboard services
	ClientWindows = 1 << 4, // server opens windows a client asks for (WindowCode::CreateWindow);
	// set only when the application installed a handler for them
	AppMessages = 1 << 5, // the application handles GlobalCode::AppRequest/AppNotify
	// Damage/partial redraw is per queue (RemoteQueueInfo::damage), not a peer feature.
};

SP_DEFINE_ENUM_AS_MASK(PeerFeatures)

namespace abi {

// FNV-1a, eight bytes at a time; only folds the facts below into one number.
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

/* A fingerprint of the wire contract this build was compiled against: record sizes and enum
 * ceilings sent as integers. It cannot see a value inserted mid-enum; tests/remote pins those.
 *
 * Diagnostic: a mismatch is logged on both sides and the session continues (see
 * PeerInfo::isWireCompatible). The engine version travels separately (PeerInfo::engineVersion).
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
	// getLocalAbiTag() of the peer's build. Diagnostic: a mismatch is logged but does not end the
	// session -- see isWireCompatible.
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
	
	An absent field decodes as all-zero, read as "said nothing", not "supports nothing" (see
	supports()). */
	uint64_t globalCodes = 0;
	uint64_t windowCodes = 0;
	uint64_t dataCodes = 0;
	uint64_t fontCodes = 0;

	// Whether the peer implements `code` in `domain`. A peer that advertised nothing is treated as
	// supporting everything.
	bool supports(Domain domain, uint8_t code) const;

	// The codes `other` is missing relative to this build, as a human-readable list; empty when the
	// two agree.
	void describeMissingCodes(const PeerInfo &other, const Callback<void(StringView)> &) const;

	// Everything a build can answer about itself with no window, no loop and no connection.
	// The caller fills in whatever else it knows (wm, api, features, transport).
	static PeerInfo makeLocal();

	/* Whether the two builds were compiled against the same wire contract (see getLocalAbiTag).
	Diagnostic only. A zero tag never matches: a truncated or absent message decodes to it. */
	bool isWireCompatible(const PeerInfo &other) const { return abi != 0 && abi == other.abi; }

	void description(const Callback<void(StringView)> &) const;
};

// A CBOR dict, not an array: this message grows, and a reader ignores keys it does not know.
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
