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

#ifndef UTILS_INSTALLER_CORE_SRC_SPITRANSPORT_H_
#define UTILS_INSTALLER_CORE_SRC_SPITRANSPORT_H_

#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Transport for fetching the toolchain catalogue and archives. Both HTTPS (preferred) and FTP
// (fallback) are handled transparently by stappler_network (libcurl): the URL scheme selects
// the protocol — `ftp://` for the legacy FTP catalogue, `https://` for a future manifest.json.

struct SP_PUBLIC TransportResult : OperationResult {
	long responseCode = 0;
};

// Fetch a text resource (FTP directory listing or HTTPS manifest.json) into `out`.
SP_PUBLIC TransportResult fetchText(StringView url, String &out);

// Fetch a binary resource (a .tar.xz archive) into `out` with optional progress
// (downloaded, total).
SP_PUBLIC TransportResult fetchBytes(StringView url, Bytes &out,
		const Function<void(int64_t, int64_t)> &progress = {});

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPITRANSPORT_H_
