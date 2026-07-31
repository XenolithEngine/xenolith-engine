#ifndef INSTALLER_CORE_SPIDIRS_H_
#define INSTALLER_CORE_SPIDIRS_H_
#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Per-platform install directories (port of the Rust `dirs::Layout`). Precedence (highest first):
// explicit --prefix → $XENOLITH_HOME → OS conventions. An override `home` puts everything under
// one root (home/{config,data,cache}); the OS path is deliberately space-free so GNU make (which
// breaks on spaces in STAPPLER_ROOT/include paths) stays happy.
struct Layout {
	String config; // config + the installed-state registry (installed.json)
	String data;   // where the SDK is unpacked (toolchains, engines)
	String cache;  // partial downloads / extraction scratch

	static Layout from_home(StringView home);
	static Layout system();
	static Layout resolve(const String *prefix, const String *envHome);
	static Layout resolve_from_env(const String *prefix);

	String installed_manifest() const;        // <config>/installed.json
	String toolchains_store_dir() const;      // <data>/toolchains
	String toolchains_hosts_dir() const;      // <data>/toolchains/hosts
	String toolchains_targets_dir() const;    // <data>/toolchains/targets
	// <data>/toolchains/<hosts|targets>/<id> — where a host/target archive is unpacked.
	String toolchain_dir(Kind kind, StringView id) const;
	String engines_dir() const;               // <data>/engines
	String engine_dir(StringView ref) const;  // <data>/engines/<ref>
	String download_tmp() const;              // <cache>/downloads
};

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPIDIRS_H_
