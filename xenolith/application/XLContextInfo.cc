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
#include "platform/XLContextController.h"

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
		target.context->userLanguage = StringView(args[0]).str<Interface>();
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--bundle <bundle-name>"},
			.description = StringView("Application bundle name"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.context->bundleName = StringView(args[0]).str<Interface>();
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
	platform::ContextController::acquireDefaultConfig(*this, nullptr);
}

ContextConfig::ContextConfig(NativeContextHandle *handle) : ContextConfig() {
	native = handle;
	platform::ContextController::acquireDefaultConfig(*this, handle);
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
	context->appVersion = toString(getAppconfigVersionVariant(), ".", getAppconfigVersionApi(), ".",
			getAppconfigVersionRev(), ".", getAppconfigVersionBuild());

	window->id = context->bundleName;
	window->title = context->appName;
}

Value ContextInfo::encode() const {
	Value ret;
	ret.setString(bundleName, "bundleName");
	ret.setString(appName, "appName");
	ret.setString(appVersion, "appVersion");
	ret.setString(userLanguage, "userLanguage");
	ret.setString(userAgent, "userAgent");
	ret.setString(launchUrl, "launchUrl");
	ret.setInteger(appVersionCode, "appVersionCode");
	ret.setInteger(appUpdateInterval.toMicros(), "appUpdateInterval");
	ret.setValue(extra, "extra");

	ret.setInteger(appThreadsCount, "appThreadsCount");
	ret.setInteger(mainThreadsCount, "mainThreadsCount");

	Value f;
	if (hasFlag(flags, ContextFlags::DestroyWhenAllWindowsClosed)) {
		f.addString("DestroyWhenAllWindowsClosed");
	}
	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value ContextConfig::encode() const {
	Value ret;

	if (auto act = context->encode()) {
		ret.setValue(act, "activity");
	}

	if (auto win = window->encode()) {
		ret.setValue(win, "window");
	}

	if (auto inst = instance->encode()) {
		ret.setValue(inst, "instance");
	}

	if (auto l = loop->encode()) {
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

void DecorationInfo::decode(const Value &val) {
	for (auto &vIt : val.asDict()) {
		if (vIt.first == "borderRadius") {
			borderRadius = vIt.second.getDouble();
		} else if (vIt.first == "shadowWidth") {
			shadowWidth = vIt.second.getDouble();
		} else if (vIt.first == "shadowMinValue") {
			shadowMinValue = vIt.second.getDouble();
		} else if (vIt.first == "shadowMaxValue") {
			shadowMaxValue = vIt.second.getDouble();
		} else if (vIt.first == "resizeInset") {
			resizeInset = vIt.second.getDouble();
		} else if (vIt.first == "shadowOffset") {
			shadowOffset = Vec2(vIt.second.getDouble(0), vIt.second.getDouble(1));
		}
	}
}

Value DecorationInfo::encode() const {
	Value ret;
	ret.setValue(borderRadius, "borderRadius");
	ret.setValue(shadowWidth, "shadowWidth");
	ret.setValue(shadowMinValue, "shadowMinValue");
	ret.setValue(shadowMaxValue, "shadowMaxValue");
	ret.setValue(resizeInset, "resizeInset");
	ret.setValue(Value{Value(shadowOffset.x), Value(shadowOffset.y)}, "shadowOffset");
	return ret;
}

void ThemeInfo::decode(const Value &val) {
	for (auto &it : val.asDict()) {
		if (it.first == "colorScheme") {
			colorScheme = it.second.getString();
		} else if (it.first == "systemTheme") {
			systemTheme = it.second.getString();
		} else if (it.first == "systemFontName") {
			systemFontName = it.second.getString();
		} else if (it.first == "cursorSize") {
			cursorSize = it.second.getInteger();
		} else if (it.first == "cursorScaling") {
			cursorScaling = it.second.getDouble();
		} else if (it.first == "textScaling") {
			textScaling = it.second.getDouble();
		} else if (it.first == "scrollModifier") {
			scrollModifier = it.second.getDouble();
		} else if (it.first == "leftHandedMouse") {
			leftHandedMouse = it.second.getBool();
		} else if (it.first == "doubleClickInterval") {
			doubleClickInterval = it.second.getInteger();
		} else if (it.first == "decorations") {
			decorations.decode(it.second);
		}
	}
}

Value ThemeInfo::encode() const {
	Value ret;
	ret.setValue(colorScheme, "colorScheme");
	ret.setValue(systemTheme, "systemTheme");
	ret.setValue(systemFontName, "systemFontName");
	ret.setValue(cursorSize, "cursorSize");
	ret.setValue(cursorScaling, "cursorScaling");
	ret.setValue(textScaling, "textScaling");
	ret.setValue(scrollModifier, "scrollModifier");
	ret.setValue(leftHandedMouse, "leftHandedMouse");
	ret.setValue(doubleClickInterval, "doubleClickInterval");
	ret.setValue(decorations.encode(), "decorations");
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
