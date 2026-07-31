#ifndef INSTALLER_CORE_SPICATALOGUE_H_
#define INSTALLER_CORE_SPICATALOGUE_H_
#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// One line of an FTP LIST response.
struct RemoteEntry {
	String name;
	uint64_t size = 0;
	bool isDir = false;
};

// An installable component: a signed .tar.xz for one triple.
struct CatalogueComponent {
	String id;       // full id incl. variant, e.g. "aarch64-apple-macosx+sprt"
	String triple;   // triple without +variant
	String variant;  // variant after "+", empty if none
	Kind kind;       // Host or Target
	uint64_t size = 0;
	bool signed_ = false; // has a matching .tar.xz.sig
};

// Parse an FTP LIST response into entries (vsFTPd format).
Vector<RemoteEntry> parse_listing(StringView text);

// Build the component catalogue from host + target listings. Archives without a matching .sig
// are DROPPED (security rule: never present an unsigned artifact).
Vector<CatalogueComponent> build_catalogue(StringView hostsText, StringView targetsText);

// Default FTP server + release (the C++ equivalent of the Rust CLI defaults).
inline StringView default_server() { return "stappler.dev"; }
inline StringView default_release() { return "sdk-v0beta1"; }
inline String ftp_release_base() {
	return String("ftp://") + toString(default_server()) + "/releases/"
			+ toString(default_release());
}

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPICATALOGUE_H_
