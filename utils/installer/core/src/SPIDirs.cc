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

#include "SPIDirs.h"

#include <stdlib.h> // getenv: there is no runtime wrapper for the environment

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

Layout Layout::fromHome(StringView home) {
	Layout l;
	l.config = mergePath(home, "config");
	l.data = mergePath(home, "data");
	l.cache = mergePath(home, "cache");
	return l;
}

Layout Layout::system() {
#if SPRT_WINDOWS
	const char *base = ::getenv("LOCALAPPDATA");
	if (!base || !*base) {
		base = "C:/Users/Public";
	}
	return fromHome(mergePath(StringView(base), "xenolith"));
#else
	// macOS/Linux: ~/.local/share/xenolith (avoid ~/Library/Application Support — it has a SPACE,
	// which the build cannot handle in STAPPLER_ROOT/include paths).
	const char *home = ::getenv("HOME");
	if (!home || !*home) {
		home = "/tmp";
	}
	return fromHome(mergePath(StringView(home), ".local/share", "xenolith"));
#endif
}

Layout Layout::resolve(StringView prefix, StringView envHome) {
	if (!prefix.empty()) {
		return fromHome(prefix);
	}
	if (!envHome.empty()) {
		return fromHome(envHome);
	}
	return system();
}

Layout Layout::resolveFromEnv(StringView prefix) {
	const char *e = ::getenv("XENOLITH_HOME");
	return resolve(prefix, (e && *e) ? StringView(e) : StringView());
}

String Layout::getInstalledManifest() const { return mergePath(config, "installed.json"); }

String Layout::getToolchainsDir() const { return mergePath(data, "toolchains"); }

String Layout::getHostsDir() const { return mergePath(getToolchainsDir(), "hosts"); }

String Layout::getTargetsDir() const { return mergePath(getToolchainsDir(), "targets"); }

String Layout::getToolchainDir(Kind kind, StringView id) const {
	return mergePath(kind == Kind::Host ? getHostsDir() : getTargetsDir(), id);
}

String Layout::getEnginesDir() const { return mergePath(data, "engines"); }

String Layout::getEngineDir(StringView ref) const { return mergePath(getEnginesDir(), ref); }

String Layout::getDownloadDir() const { return mergePath(cache, "downloads"); }

} // namespace stappler::xenolith::installer
