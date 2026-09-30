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

#ifndef UTILS_INSTALLER_CORE_SRC_SPICATALOGUE_H_
#define UTILS_INSTALLER_CORE_SRC_SPICATALOGUE_H_

#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// One line of an FTP LIST response.
struct SP_PUBLIC RemoteEntry {
	String name;
	uint64_t size = 0;
	bool isDir = false;
};

// An installable component: a signed .tar.xz for one triple.
struct SP_PUBLIC CatalogueComponent {
	String id; // full id incl. variant, e.g. "aarch64-apple-macosx+sprt"
	String triple; // triple without +variant
	String variant; // variant after "+", empty if none
	Kind kind = Kind::Target;
	uint64_t size = 0;
	bool isSigned = false; // has a matching .tar.xz.sig
};

// Parse an FTP LIST response into entries (vsFTPd format).
SP_PUBLIC Vector<RemoteEntry> parseListing(StringView text);

// Build the component catalogue from host + target listings. Archives without a matching .sig
// are DROPPED (security rule: never present an unsigned artifact).
SP_PUBLIC Vector<CatalogueComponent> buildCatalogue(StringView hostsText, StringView targetsText);

// Default FTP server + release fallback when discovery fails.
inline StringView getDefaultServer() { return "stappler.dev"; }

// The built-in releases root, used when the user has configured no mirror. Prefer
// SourceConfig::getReleasesRoot() (SPISettings.h), which falls back to this — reaching for the
// default directly bypasses whatever the user chose.
inline String getDefaultReleasesRoot() {
	return toString("ftp://", getDefaultServer(), "/releases/");
}

// The newest `sdk-v*` directory in an FTP LIST of /releases/, or EMPTY when the listing carries
// none. There is deliberately NO compiled-in fallback release: a baked-in name goes stale with
// every SDK release, and a caller that cannot name a release must fail rather than silently offer
// the toolchains of a long-gone era. A mirror that cannot be listed at all is the user's to fix —
// by pinning `sdkRelease` in the config (SPISettings) or repairing the mirror.
SP_PUBLIC String resolveActiveRelease(StringView releasesListing);

/* The outcome of release resolution: which release directory the catalogue should be read from.

`pinned` and `discovered` separate the two ways a selection can come about — the user's config or
the server. An empty selection with an error set means "no adequate release could be determined"
and is NOT a usable fallback: there is none. */
struct SP_PUBLIC ReleaseSelection : OperationResult {
	String release; // directory name under the releases root, e.g. "sdk-v0rc0"
	String base; // full URL of that directory, with the trailing slash; empty on error
	bool pinned = false; // came from the stored sdkRelease setting, not the server
	bool discovered = false; // read off the server
};

// One network round trip: list `releasesRoot` and pick the newest `sdk-v*` in it. Errors (with an
// empty release/base) when the listing cannot be fetched or carries no sdk-v* directory. The
// adequate-selection entry point is SourceConfig::selectRelease() (SPISettings.h), which honours
// the user's pin first — both front ends go through that one.
SP_PUBLIC ReleaseSelection discoverRelease(StringView releasesRoot);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPICATALOGUE_H_
