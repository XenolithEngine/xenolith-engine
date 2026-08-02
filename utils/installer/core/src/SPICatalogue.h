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

// Default FTP server + release.
inline StringView getDefaultServer() { return "stappler.dev"; }

inline StringView getDefaultRelease() { return "sdk-v0beta1"; }

inline String getFtpReleaseBase() {
	return toString("ftp://", getDefaultServer(), "/releases/", getDefaultRelease());
}

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPICATALOGUE_H_
