#ifndef INSTALLER_CORE_SPITRANSPORT_H_
#define INSTALLER_CORE_SPITRANSPORT_H_
#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Transport for fetching the toolchain catalogue and archives. Both HTTPS (preferred) and FTP
// (fallback) are handled transparently by stappler_network (libcurl): the URL scheme selects
// the protocol — `ftp://` for the legacy FTP catalogue, `https://` for a future manifest.json.

struct TransportResult {
	Status status = Status::Ok;
	long responseCode = 0;
	String error;
};

// Fetch a text resource (FTP directory listing or HTTPS manifest.json) into `out`.
TransportResult fetch_text(StringView url, String &out);

// Fetch a binary resource (a .tar.xz archive) into `out` with optional progress
// (downloaded, total).
TransportResult fetch_bytes(StringView url, Bytes &out,
		const Function<void(int64_t, int64_t)> &progress = {});

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPITRANSPORT_H_
