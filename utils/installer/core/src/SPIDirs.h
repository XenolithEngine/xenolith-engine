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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIDIRS_H_
#define UTILS_INSTALLER_CORE_SRC_SPIDIRS_H_

#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Per-platform install directories. Precedence (highest first): explicit prefix → $XENOLITH_HOME →
// OS conventions. An override `home` puts everything under one root (home/{config,data,cache}); the
// OS path is deliberately space-free, since the build breaks on spaces in STAPPLER_ROOT and include
// paths.
struct SP_PUBLIC Layout {
	String config; // config + the installed-state registry (installed.json)
	String data; // where the SDK is unpacked (toolchains, engines)
	String cache; // partial downloads / extraction scratch

	static Layout fromHome(StringView home);
	static Layout system();

	// An empty `prefix`/`envHome` means "not set".
	static Layout resolve(StringView prefix, StringView envHome);
	static Layout resolveFromEnv(StringView prefix = StringView());

	String getInstalledManifest() const; // <config>/installed.json
	String getToolchainsDir() const; // <data>/toolchains
	String getHostsDir() const; // <data>/toolchains/hosts
	String getTargetsDir() const; // <data>/toolchains/targets

	// <data>/toolchains/<hosts|targets>/<id> — where a host/target archive is unpacked.
	String getToolchainDir(Kind kind, StringView id) const;

	String getEnginesDir() const; // <data>/engines
	String getEngineDir(StringView ref) const; // <data>/engines/<ref>
	String getDownloadDir() const; // <cache>/downloads
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIDIRS_H_
