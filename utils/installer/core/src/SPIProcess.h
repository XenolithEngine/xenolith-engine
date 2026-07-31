#ifndef INSTALLER_CORE_SPIPROCESS_H_
#define INSTALLER_CORE_SPIPROCESS_H_
#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The installer CLI is a host-native tool (full host libc), so it can shell out to system
// programs. Toolchain-archive extraction (`tar -xf`) and project builds (driving the toolchain's
// own GNU make 4.1) both delegate to external processes. Stdio is inherited so the child's output
// streams through unchanged.

struct ProcessSpawn {
	String cwd;                                      // empty → inherit the parent's cwd
	// Environment entries. When `envReplace` is false (default) these are MERGED into the
	// inherited environment (later entries override earlier ones); when true the environment is
	// exactly these entries. Used to prepend the toolchain `bin/` to PATH and set STAPPLER_ROOT.
	Vector<std::pair<String, String>> env;
	bool envReplace = false;
};

// Run `argv` (argv[0] searched on PATH via execvp). Inherits stdin/stdout/stderr. Returns the
// process exit code on success, or -1 if the process could not be spawned / failed abnormally.
int run_process(const Vector<String> &argv, const ProcessSpawn &opts = ProcessSpawn());

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPIPROCESS_H_
