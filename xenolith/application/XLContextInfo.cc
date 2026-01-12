/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLContextInfo.h"

#include <sprt/runtime/window/types.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Опции для аргументов командной строки

CommandLineParser<ContextConfig> ContextConfig::getCommandLineParser() {
	return CommandLineParser<ContextConfig>({
		CommandLineOption<ContextConfig>{.patterns = {"-v", "--verbose"},
			.description = "Produce more verbose output",
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.flags |= CommonFlags::Verbose;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-h", "--help"},
			.description = StringView("Show help message and exit"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.flags |= CommonFlags::Help;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-q", "--quiet"},
			.description = StringView("Disable verbose output"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.flags |= CommonFlags::Quiet;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-W<#>", "--width <#>"},
			.description = StringView("Window width"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->rect.width = uint32_t(StringView(args[0]).readInteger(10).get(0));
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-H<#>", "--height <#>"},
			.description = StringView("Window height"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->rect.height = uint32_t(StringView(args[0]).readInteger(10).get(0));
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-D<#.#>", "--density <#.#>"},
			.description = StringView("Pixel density for a window"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->density = float(StringView(args[0]).readFloat().get(0));
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--l <locale>", "--locale <locale>"},
			.description = StringView("User language locale"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.context->userLanguage = StringView(args[0]).str<sprt::window::String>();
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--bundle <bundle-name>"},
			.description = StringView("Application bundle name"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.context->bundleName = StringView(args[0]).str<sprt::window::String>();
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--renderdoc"},
			.description = StringView("Open connection for renderdoc"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.instance) {
			target.instance = Rc<core::InstanceInfo>::alloc();
		}
		target.instance->flags |= core::InstanceFlags::RenderDoc;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--novalidation"},
			.description = StringView("Force-disable Vulkan validation layers"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.instance) {
			target.instance = Rc<core::InstanceInfo>::alloc();
		}
		target.instance->flags |= core::InstanceFlags::Validation;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--decor <decoration-description>"},
			.description = StringView("Define window decoration paddings"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		auto values = StringView(args[0]);
		float f[4] = {nan(), nan(), nan(), nan()};
		uint32_t i = 0;
		values.split<StringView::Chars<','>>([&](StringView val) {
			if (i < 4) {
				f[i] = val.readFloat().get(nan());
			}
			++i;
		});
		if (!isnan(f[0])) {
			if (isnan(f[1])) {
				f[1] = f[0];
			}
			if (isnan(f[2])) {
				f[2] = f[0];
			}
			if (isnan(f[3])) {
				f[3] = f[1];
			}
			if (!target.window) {
				target.window = Rc<WindowInfo>::alloc();
			}
			target.window->decorationInsets = Padding(f[0], f[1], f[2], f[3]);
			return true;
		}
		return false;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--device <#>"},
			.description = StringView("Force-disable Vulkan validation layers"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.loop) {
			target.loop = Rc<core::LoopInfo>::alloc();
		}
		target.loop->deviceIdx =
				StringView(args.at(0)).readInteger(10).get(core::InstanceDefaultDevice);
		return true;
	}},
	});
}

bool ContextConfig::readFromCommandLine(ContextConfig &ret, int argc, const char *argv[],
		const Callback<void(StringView)> &cb) {
	return getCommandLineParser().parse(ret, argc, argv,
			cb ? Callback<void(ContextConfig &, StringView)>(
						 [&](ContextConfig &, StringView arg) { cb(arg); })
			   : nullptr);
}

ContextConfig::ContextConfig(int argc, const char *argv[]) : ContextConfig() {
	readFromCommandLine(*this, argc, argv);
	sprt::window::ContextController::acquireDefaultConfig(*this, nullptr);
}

ContextConfig::ContextConfig(NativeContextHandle *handle) : ContextConfig() {
	native = handle;
	sprt::window::ContextController::acquireDefaultConfig(*this, handle);
}

ContextConfig::ContextConfig() {
	context = Rc<ContextInfo>::alloc();
	window = Rc<WindowInfo>::alloc();
	instance = Rc<core::InstanceInfo>::alloc();
	loop = Rc<core::LoopInfo>::alloc();

	if (auto str = getAppconfigBundleName()) {
		context->bundleName = str;
	}
	if (auto str = getAppconfigAppName()) {
		context->appName = str;
	}
	context->appVersionCode = getAppconfigVersionIndex();
	context->appVersion = sprt::StreamTraits<char>::toString(getAppconfigVersionVariant(), ".",
			getAppconfigVersionApi(), ".", getAppconfigVersionRev(), ".",
			getAppconfigVersionBuild());

	window->id = context->bundleName;
	window->title = context->appName;
}

Value encodeContextInfo(const ContextInfo &info) {
	Value ret;
	ret.setString(info.bundleName, "bundleName");
	ret.setString(info.appName, "appName");
	ret.setString(info.appVersion, "appVersion");
	ret.setString(info.userLanguage, "userLanguage");
	ret.setString(info.userAgent, "userAgent");
	ret.setString(info.launchUrl, "launchUrl");
	ret.setInteger(info.appVersionCode, "appVersionCode");
	ret.setInteger(info.appUpdateInterval, "appUpdateInterval");

	ret.setInteger(info.appThreadsCount, "appThreadsCount");
	ret.setInteger(info.mainThreadsCount, "mainThreadsCount");

	Value f;
	if (hasFlag(info.flags, ContextFlags::DestroyWhenAllWindowsClosed)) {
		f.addString("DestroyWhenAllWindowsClosed");
	}
	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value ContextConfig::encode() const {
	Value ret;

	if (auto act = encodeContextInfo(*context)) {
		ret.setValue(act, "activity");
	}

	if (auto win = encodeWindowInfo(*window)) {
		ret.setValue(win, "window");
	}

	if (auto inst = core::encodeInstanceInfo(*instance)) {
		ret.setValue(inst, "instance");
	}

	if (auto l = core::encodeLoopInfo(*loop)) {
		ret.setValue(l, "loop");
	}

	Value f;
	if (hasFlag(flags, CommonFlags::Help)) {
		f.addString("Help");
	}
	if (hasFlag(flags, CommonFlags::Verbose)) {
		f.addString("Verbose");
	}
	if (hasFlag(flags, CommonFlags::Quiet)) {
		f.addString("Quiet");
	}
	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value encodeDecorationInfo(const DecorationInfo &info) {
	Value ret;
	ret.setValue(info.borderRadius, "borderRadius");
	ret.setValue(info.shadowWidth, "shadowWidth");
	ret.setValue(info.shadowMinValue, "shadowMinValue");
	ret.setValue(info.shadowMaxValue, "shadowMaxValue");
	ret.setValue(info.resizeInset, "resizeInset");
	ret.setValue(Value{Value(info.shadowOffset.x), Value(info.shadowOffset.y)}, "shadowOffset");
	return ret;
}

Value encodeThemeInfo(const ThemeInfo &info) {
	Value ret;
	ret.setValue(info.colorScheme, "colorScheme");
	ret.setValue(info.systemTheme, "systemTheme");
	ret.setValue(info.systemFontName, "systemFontName");
	ret.setValue(info.cursorSize, "cursorSize");
	ret.setValue(info.cursorScaling, "cursorScaling");
	ret.setValue(info.textScaling, "textScaling");
	ret.setValue(info.scrollModifier, "scrollModifier");
	ret.setValue(info.leftHandedMouse, "leftHandedMouse");
	ret.setValue(info.doubleClickInterval, "doubleClickInterval");
	ret.setValue(encodeDecorationInfo(info.decorations), "decorations");
	return ret;
}

const CallbackStream &operator<<(const CallbackStream &stream, NetworkFlags t) {
	for (auto it : flags(t)) {
		stream << " ";
		switch (it) {
		case NetworkFlags::None: break;
		case NetworkFlags::Internet: stream << "NetworkFlags::Internet"; break;
		case NetworkFlags::Congested: stream << "NetworkFlags::Congested"; break;
		case NetworkFlags::Metered: stream << "NetworkFlags::Metered"; break;
		case NetworkFlags::Restricted: stream << "NetworkFlags::Restricted"; break;
		case NetworkFlags::Roaming: stream << "NetworkFlags::Roaming"; break;
		case NetworkFlags::Suspended: stream << "NetworkFlags::Suspended"; break;
		case NetworkFlags::Vpn: stream << "NetworkFlags::Vpn"; break;
		case NetworkFlags::PrioritizeBandwidth:
			stream << "NetworkFlags::PrioritizeBandwidth";
			break;
		case NetworkFlags::PrioritizeLatency: stream << "NetworkFlags::PrioritizeLatency"; break;
		case NetworkFlags::TemporarilyNotMetered:
			stream << "NetworkFlags::TemporarilyNotMetered";
			break;
		case NetworkFlags::Trusted: stream << "NetworkFlags::Trusted"; break;
		case NetworkFlags::Validated: stream << "NetworkFlags::Validated"; break;
		case NetworkFlags::WifiP2P: stream << "NetworkFlags::WifiP2P"; break;
		case NetworkFlags::CaptivePortal: stream << "NetworkFlags::CaptivePortal"; break;
		case NetworkFlags::Local: stream << "NetworkFlags::Local"; break;
		case NetworkFlags::Wired: stream << "NetworkFlags::Wired"; break;
		case NetworkFlags::WLAN: stream << "NetworkFlags::WLAN"; break;
		case NetworkFlags::WWAN: stream << "NetworkFlags::WWAN"; break;
		}
	}
	return stream;
}

std::ostream &operator<<(std::ostream &stream, NetworkFlags t) {
	memory::makeCallback(stream) << t;
	return stream;
}

} // namespace stappler::xenolith
