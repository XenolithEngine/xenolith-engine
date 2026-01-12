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

#ifndef XENOLITH_APPLICATION_XLCONTEXTINFO_H_
#define XENOLITH_APPLICATION_XLCONTEXTINFO_H_

#include "XLWindowInfo.h"
#include "XLCoreLoop.h" // IWYU pragma: keep
#include "XLCoreInstance.h"
#include "SPCommandLineParser.h"

#include <sprt/runtime/window/controller.h>
#include <sprt/runtime/window/context_info.h>
#include <sprt/runtime/window/notifications.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;

using sprt::window::NativeContextHandle;
using sprt::window::SystemNotification;
using sprt::window::NetworkFlags;
using sprt::window::ContextFlags;
using sprt::window::ContextInfo;

struct SP_PUBLIC UpdateTime final {
	uint64_t global = 0; // global OS timer in microseconds
	uint64_t app = 0; // microseconds since application was started
	uint64_t delta = 0; // microseconds since last update
	float dt = 0.0f; // seconds since last update
};

enum class CommonFlags : uint32_t {
	None = 0,
	Help = 1 << 0,
	Verbose = 1 << 1,
	Quiet = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(CommonFlags)

struct ContextConfig final : public sprt::window::ContextConfig {
	static CommandLineParser<ContextConfig> getCommandLineParser();

	static bool readFromCommandLine(ContextConfig &, int argc, const char *argv[],
			const Callback<void(StringView)> &cb = nullptr);

	CommonFlags flags = CommonFlags::None;

	ContextConfig(int argc, const char *argv[]);
	ContextConfig(NativeContextHandle *);

	Value encode() const;

private:
	ContextConfig();
};

using sprt::window::DecorationInfo;
using sprt::window::ThemeInfo;

SP_PUBLIC Value encodeContextInfo(const ContextInfo &);
SP_PUBLIC Value encodeDecorationInfo(const DecorationInfo &);
SP_PUBLIC Value encodeThemeInfo(const ThemeInfo &);

using OpacityValue = ValueWrapper<uint8_t, class OpacityTag>;
using ZOrder = ValueWrapper<int16_t, class ZOrderTag>;

SP_PUBLIC const CallbackStream &operator<<(const CallbackStream &stream, NetworkFlags t);
SP_PUBLIC std::ostream &operator<<(std::ostream &stream, NetworkFlags t);

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCONTEXTINFO_H_ */
