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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIMANIFEST_H_
#define UTILS_INSTALLER_CORE_SRC_SPIMANIFEST_H_

#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// What an installable artifact is: a build host toolchain (hosts/) or a cross target sysroot
// (targets/). (Minimal slice of the manifest model for now — full Component/catalogue comes later.)
enum class Kind {
	Host,
	Target,
};

// Directory segment, both on the server and in the local store.
inline StringView getKindDirName(Kind k) {
	return k == Kind::Host ? StringView("hosts") : StringView("targets");
}

inline Kind parseKind(StringView s) { return s == "host" ? Kind::Host : Kind::Target; }

inline StringView getKindName(Kind k) {
	return k == Kind::Host ? StringView("host") : StringView("target");
}

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIMANIFEST_H_
