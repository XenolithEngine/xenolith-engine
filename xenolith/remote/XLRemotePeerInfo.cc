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

#include "XLRemotePeerInfo.h"
#include "XLCoreInstance.h" // getInstanceApiName

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// The wire numbering was taken from the runtime's platform/arch ids so there is one numbering
// rather than two. These are what would fail if the runtime ever renumbered them -- at which point
// the mapping below has to become an explicit table, and the wire values must NOT follow.
static_assert(toInt(OsPlatform::MacOs) == __SPRT_PLATFORM_ID_MACOS
				&& toInt(OsPlatform::Ios) == __SPRT_PLATFORM_ID_IOS
				&& toInt(OsPlatform::Darwin) == __SPRT_PLATFORM_ID_DARWIN_UNKNOWN
				&& toInt(OsPlatform::Windows) == __SPRT_PLATFORM_ID_WINDOWS
				&& toInt(OsPlatform::Android) == __SPRT_PLATFORM_ID_ANDROID
				&& toInt(OsPlatform::Linux) == __SPRT_PLATFORM_ID_LINUX
				&& toInt(OsPlatform::Wasm) == __SPRT_PLATFORM_ID_WASM
				&& toInt(OsPlatform::Nuttx) == __SPRT_PLATFORM_ID_NUTTX
				&& toInt(OsPlatform::Embox) == __SPRT_PLATFORM_ID_EMBOX,
		"remote::OsPlatform no longer mirrors the runtime's platform ids");

static_assert(toInt(OsArch::Aarch64) == __SPRT_ARCH_ID_AARCH64
				&& toInt(OsArch::Arm) == __SPRT_ARCH_ID_ARM
				&& toInt(OsArch::X86) == __SPRT_ARCH_ID_X86
				&& toInt(OsArch::X86_64) == __SPRT_ARCH_ID_X86_64
				&& toInt(OsArch::E2k) == __SPRT_ARCH_ID_E2K
				&& toInt(OsArch::Wasm64) == __SPRT_ARCH_ID_WASM64
				&& toInt(OsArch::Wasm32) == __SPRT_ARCH_ID_WASM32
				&& toInt(OsArch::Riscv64) == __SPRT_ARCH_ID_RISCV64
				&& toInt(OsArch::Riscv32) == __SPRT_ARCH_ID_RISCV32
				&& toInt(OsArch::Loongarch64) == __SPRT_ARCH_ID_LOONGARCH64
				&& toInt(OsArch::Loongarch32) == __SPRT_ARCH_ID_LOONGARCH32,
		"remote::OsArch no longer mirrors the runtime's arch ids");

// A build always knows exactly one of each, so there is nothing to detect at runtime.
static constexpr OsPlatform kLocalPlatform = OsPlatform(__SPRT_PLATFORM_ID);
static constexpr OsArch kLocalArch = OsArch(__SPRT_ARCH_ID);

StringView getOsPlatformName(OsPlatform p) {
	switch (p) {
	case OsPlatform::MacOs: return StringView("macOS");
	case OsPlatform::Ios: return StringView("iOS");
	case OsPlatform::Darwin: return StringView("Darwin");
	case OsPlatform::Windows: return StringView("Windows");
	case OsPlatform::Android: return StringView("Android");
	case OsPlatform::Linux: return StringView("Linux");
	case OsPlatform::Wasm: return StringView("wasm");
	case OsPlatform::Nuttx: return StringView("NuttX");
	case OsPlatform::Embox: return StringView("Embox");
	case OsPlatform::Unknown: break;
	}
	return StringView("unknown");
}

StringView getOsArchName(OsArch a) {
	switch (a) {
	case OsArch::Aarch64: return StringView("aarch64");
	case OsArch::Arm: return StringView("arm");
	case OsArch::X86: return StringView("x86");
	case OsArch::X86_64: return StringView("x86_64");
	case OsArch::E2k: return StringView("e2k");
	case OsArch::Wasm64: return StringView("wasm64");
	case OsArch::Wasm32: return StringView("wasm32");
	case OsArch::Riscv64: return StringView("riscv64");
	case OsArch::Riscv32: return StringView("riscv32");
	case OsArch::Loongarch64: return StringView("loongarch64");
	case OsArch::Loongarch32: return StringView("loongarch32");
	case OsArch::Unknown: break;
	}
	return StringView("unknown");
}

StringView getWindowSubsystemName(WindowSubsystem w) {
	switch (w) {
	case WindowSubsystem::Headless: return StringView("headless");
	case WindowSubsystem::Xcb: return StringView("xcb");
	case WindowSubsystem::Wayland: return StringView("wayland");
	case WindowSubsystem::Win32: return StringView("win32");
	case WindowSubsystem::Cocoa: return StringView("cocoa");
	case WindowSubsystem::UiKit: return StringView("uikit");
	case WindowSubsystem::Android: return StringView("android");
	case WindowSubsystem::Canvas: return StringView("canvas");
	case WindowSubsystem::Display: return StringView("display");
	case WindowSubsystem::Unknown: break;
	}
	return StringView("unknown");
}

WindowSubsystem toWindowSubsystem(sprt::window::SurfaceBackend b) {
	using sprt::window::SurfaceBackend;
	switch (b) {
	case SurfaceBackend::Headless: return WindowSubsystem::Headless;
	case SurfaceBackend::Xcb:
	case SurfaceBackend::XLib: return WindowSubsystem::Xcb;
	case SurfaceBackend::Wayland: return WindowSubsystem::Wayland;
	case SurfaceBackend::Win32: return WindowSubsystem::Win32;
	case SurfaceBackend::MacOS:
	case SurfaceBackend::Metal: return WindowSubsystem::Cocoa;
	case SurfaceBackend::IOS: return WindowSubsystem::UiKit;
	case SurfaceBackend::Android:
	case SurfaceBackend::GoogleGames: return WindowSubsystem::Android;
	case SurfaceBackend::Canvas: return WindowSubsystem::Canvas;
	case SurfaceBackend::Display: return WindowSubsystem::Display;
	// Surface / DirectFb / Fuchsia / VI / QNX / OpenHarmony have no engine window path yet; saying
	// "unknown" is the honest answer, and it is what a client tests against before assuming.
	default: break;
	}
	return WindowSubsystem::Unknown;
}

PeerInfo PeerInfo::makeLocal() {
	PeerInfo ret;
	ret.engineVersion = String(getVersionString());
	ret.abi = getLocalAbiTag();
#if DEBUG
	ret.debug = true;
#endif
	ret.platform = kLocalPlatform;
	ret.arch = kLocalArch;
	ret.globalCodes = kSupportedGlobalCodes;
	ret.windowCodes = kSupportedWindowCodes;
	ret.dataCodes = kSupportedDataCodes;
	ret.fontCodes = kSupportedFontCodes;
	return ret;
}

bool PeerInfo::supports(Domain domain, uint8_t code) const {
	uint64_t mask = 0;
	switch (domain) {
	case Domain::Global: mask = globalCodes; break;
	case Domain::Window: mask = windowCodes; break;
	case Domain::Data: mask = dataCodes; break;
	case Domain::Font: mask = fontCodes; break;
	default: return false;
	}
	if (mask == 0) {
		// Said nothing -- which is what a peer built before this field does. Assuming the worst here
		// would refuse to send it anything at all; assuming the best leaves it exactly where it was,
		// answering NotImplemented to what it does not know.
		return true;
	}
	return (mask & codeBit(code)) != 0;
}

void PeerInfo::describeMissingCodes(const PeerInfo &other, const Callback<void(StringView)> &out)
		const {
	auto report = [&](Domain d, StringView name) {
		uint64_t mine = getSupportedCodes(d);
		uint64_t theirs = 0;
		switch (d) {
		case Domain::Global: theirs = other.globalCodes; break;
		case Domain::Window: theirs = other.windowCodes; break;
		case Domain::Data: theirs = other.dataCodes; break;
		case Domain::Font: theirs = other.fontCodes; break;
		default: break;
		}
		if (theirs == 0) {
			return; // said nothing; not the same as missing everything
		}
		auto missing = mine & ~theirs;
		if (!missing) {
			return;
		}
		out << name << ":";
		for (uint8_t i = 0; i < 64; ++i) {
			if (missing & codeBit(i)) {
				out << " " << uint32_t(i);
			}
		}
		out << "; ";
	};
	report(Domain::Global, "global");
	report(Domain::Window, "window");
	report(Domain::Data, "data");
	report(Domain::Font, "font");
}

void PeerInfo::description(const Callback<void(StringView)> &out) const {
	out << getOsPlatformName(platform) << "/" << getOsArchName(arch);
	if (wm != WindowSubsystem::Unknown) {
		out << " " << getWindowSubsystemName(wm);
	}
	if (api != InstanceApi::None) {
		out << " " << core::getInstanceApiName(api);
		if (apiVersion) {
			out << " " << getVersionDescription(apiVersion);
		}
	}
	out << ", engine " << engineVersion << (debug ? " (debug)" : "");
	out << ", abi " << base16::encode<Interface>(BytesView((const uint8_t *)&abi, sizeof(abi)));
	if (!transportScheme.empty()) {
		out << ", over " << transportScheme;
	}
}

Value serializePeerInfo(const PeerInfo &info) {
	Value ret;

	Value &engine = ret.emplace("engine");
	engine.setString(info.engineVersion, "version");
	// CBOR has no unsigned 64-bit integer in data::Value; the tag is an opaque bit pattern, so it
	// travels reinterpreted rather than clamped.
	engine.setInteger(int64_t(info.abi), "abi");
	engine.setBool(info.debug, "debug");

	Value &os = ret.emplace("os");
	os.setInteger(toInt(info.platform), "platform");
	os.setInteger(toInt(info.arch), "arch");

	ret.setInteger(toInt(info.wm), "wm");

	if (info.api != InstanceApi::None) {
		Value &gapi = ret.emplace("gapi");
		gapi.setInteger(toInt(info.api), "api");
		gapi.setInteger(info.apiVersion, "version");
	}

	ret.setInteger(int64_t(toInt(info.features)), "features");

	Value &transport = ret.emplace("transport");
	transport.setString(info.transportScheme, "scheme");
	transport.setInteger(toInt(info.transportCaps), "caps");

	// One key per domain rather than an array: the domains are named things, not positions, and a
	// domain added later must not shift the meaning of the others.
	Value &codes = ret.emplace("codes");
	codes.setInteger(int64_t(info.globalCodes), "g");
	codes.setInteger(int64_t(info.windowCodes), "w");
	codes.setInteger(int64_t(info.dataCodes), "d");
	codes.setInteger(int64_t(info.fontCodes), "f");

	return ret;
}

PeerInfo deserializePeerInfo(const Value &val) {
	PeerInfo ret;

	// Absent for a peer that predates the field; the zeros that leaves mean "said nothing", which is
	// what PeerInfo::supports reads them as.
	const Value &codes = val.getValue("codes");
	ret.globalCodes = uint64_t(codes.getInteger("g"));
	ret.windowCodes = uint64_t(codes.getInteger("w"));
	ret.dataCodes = uint64_t(codes.getInteger("d"));
	ret.fontCodes = uint64_t(codes.getInteger("f"));

	const Value &engine = val.getValue("engine");
	ret.engineVersion = engine.getString("version");
	ret.abi = uint64_t(engine.getInteger("abi"));
	ret.debug = engine.getBool("debug");

	const Value &os = val.getValue("os");
	ret.platform = OsPlatform(os.getInteger("platform"));
	ret.arch = OsArch(os.getInteger("arch"));

	ret.wm = WindowSubsystem(val.getInteger("wm"));

	const Value &gapi = val.getValue("gapi");
	ret.api = InstanceApi(gapi.getInteger("api"));
	ret.apiVersion = uint32_t(gapi.getInteger("version"));

	ret.features = PeerFeatures(uint64_t(val.getInteger("features")));

	const Value &transport = val.getValue("transport");
	ret.transportScheme = transport.getString("scheme");
	ret.transportCaps = TransportCaps(transport.getInteger("caps"));

	return ret;
}

} // namespace stappler::xenolith::remote
