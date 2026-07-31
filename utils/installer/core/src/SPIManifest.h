#ifndef INSTALLER_CORE_SPIMANIFEST_H_
#define INSTALLER_CORE_SPIMANIFEST_H_
#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// What an installable artifact is: a build host toolchain (hosts/) or a cross target sysroot
// (targets/). (Minimal slice of the manifest model for now — full Component/catalogue comes later.)
enum class Kind {
	Host,
	Target,
};

inline StringView kind_dir(Kind k) { return k == Kind::Host ? StringView("hosts") : StringView("targets"); }
inline Kind kind_from_string(StringView s) { return s == "host" ? Kind::Host : Kind::Target; }
inline StringView kind_to_string(Kind k) { return k == Kind::Host ? StringView("host") : StringView("target"); }

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPIMANIFEST_H_
