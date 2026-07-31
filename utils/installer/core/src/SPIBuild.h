#ifndef INSTALLER_CORE_SPIBUILD_H_
#define INSTALLER_CORE_SPIBUILD_H_
#include "SPICommon.h"
#include "SPIDirs.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

struct BuildOptions {
	String target;    // empty → native host triple
	bool run = false; // run the freshly-built binary afterwards (native builds only)
	bool release = false;
};

struct BuildResult {
	Status status = Status::Ok;
	String error;
	String message;
};

// Build the project at `path` (must contain a Makefile) with the SDK toolchain: drives the host
// toolchain's GNU make with ENV STAPPLER_ROOT = resolved engine root and PATH headed by the host
// toolchain `bin/`. Cross targets (target_base != host) `make install`; native builds may `--run`.
BuildResult build_project(StringView path, const Layout &layout, const BuildOptions &opts,
		const String *engineOverride);

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPIBUILD_H_
