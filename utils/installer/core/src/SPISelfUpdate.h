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


#ifndef UTILS_INSTALLER_CORE_SRC_SPISELFUPDATE_H_
#define UTILS_INSTALLER_CORE_SRC_SPISELFUPDATE_H_

#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The CLI's own releases: `cli-v*` tags on GitHub, one `xenolith-cli-<triple>.tar.gz` asset per
// platform with a `.sha256` sidecar beside it.

// "0.2.1", "v0.2.1" and "cli-v0.2.1" all become "cli-v0.2.1"; "" for anything that is not a
// dotted run of numbers, such as the "dev" of a local build.
SP_PUBLIC String normalizeCliTag(StringView version);

// Numeric, component by component, missing components read as 0. An invalid version sorts below
// every valid one.
SP_PUBLIC int compareCliVersions(StringView a, StringView b);

// The asset this binary updates from: the released Linux build is musl-linked, whatever the
// distribution.
SP_PUBLIC String getCliAssetTriple();

struct SP_PUBLIC CliReleaseQuery : OperationResult {
	String tag;
};

// The highest non-draft, non-prerelease cli-v* release.
SP_PUBLIC CliReleaseQuery findLatestCliRelease();

struct SP_PUBLIC SelfUpdateResult : OperationResult {
	String path; // the binary that was replaced
	String version; // what the new binary reports
	String warning; // something left behind that did not stop the update
};

// Download `tag` for getCliAssetTriple(), verify its checksum, check that it runs, and put it in
// place of the running executable. $XENOLITH_RELEASE_BASE replaces the download base, as it does
// for install.sh.
SP_PUBLIC SelfUpdateResult installCliRelease(StringView tag,
		const Function<void(int64_t, int64_t)> &progress = {});

// Remove what a previous update left next to the executable: on Windows the replaced binary can
// only be deleted once it no longer runs.
SP_PUBLIC void cleanupSelfUpdate();

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPISELFUPDATE_H_
