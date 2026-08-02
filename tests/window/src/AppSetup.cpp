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

#include "XLCommon.h" // IWYU pragma: keep
#include "XLContext.h"
#include "XLContextInfo.h"
#include "XLEntryPoint.h"

#include "LiveReloadAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The --watch <dir> project directory, captured from the command line by parseConfigCmd and read by
// makeAppThread. A file-local static: there is exactly one app instance, and both seams run before
// the app thread starts.
static mem_std::Interface::StringType s_watchDir;

// Custom command-line parse (Context::SymbolParseConfigCmd): --watch <dir> is an app-level flag, not
// an engine ContextConfig option, so we peel it off here and hand the remaining arguments to the
// standard ContextConfig parser.
static ContextConfig parseConfigCmd(int argc, const char **argv) {
	mem_std::Interface::VectorType<const char *> filtered;
	filtered.reserve(size_t(argc));
	for (int i = 0; i < argc; ++i) {
		StringView a(argv[i]);
		if (a == "--watch") {
			if (i + 1 < argc) {
				s_watchDir = argv[++i];
			}
			continue;
		}
		if (a.starts_with("--watch=")) {
			s_watchDir.assign(a.data() + 8, a.size() - 8);
			continue;
		}
		filtered.emplace_back(argv[i]);
	}
	return ContextConfig(int(filtered.size()), filtered.data());
}

SP_USED static SharedExtension s_parseConfigCmdSymbol(buildconfig::MODULE_APPCOMMON_NAME,
		Context::SymbolParseConfigCmdName, &parseConfigCmd);

// Custom app thread (Context::SymbolMakeAppThread, via DEFINE_APP_THREAD_CONSTRUCTOR): the
// live-reload observer host, seeded with the --watch directory captured above. Construction goes
// through the factory in LiveReloadAppThread.cpp (which has the complete types). The engine downcasts
// the returned AppThread to ServerAppThread, which LiveReloadAppThread is.
static Rc<AppThread> makeAppThread(NotNull<Context> ctx) {
	return createLiveReloadAppThread(ctx, StringView(s_watchDir.data(), s_watchDir.size()));
}

DEFINE_APP_THREAD_CONSTRUCTOR(makeAppThread)

} // namespace stappler::xenolith::app
