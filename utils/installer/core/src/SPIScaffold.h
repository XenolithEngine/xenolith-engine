#ifndef INSTALLER_CORE_SPISCAFFOLD_H_
#define INSTALLER_CORE_SPISCAFFOLD_H_
#include "SPICommon.h"
#include "SPIDirs.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

struct ScaffoldResult {
	Status status = Status::Ok;
	String error;
	String path; // created project directory
};

// A valid project name: non-empty, letters/digits/`-`/`_` only (no spaces) — used verbatim as the
// folder and executable name.
bool is_valid_project_name(StringView name);
// Sanitize into a make-safe executable identifier (replace invalid chars with `_`, default `app`).
String sanitize_project_name(StringView name);

// Scaffold a buildable graphical project `name` inside `location`: writes the vendored minimal
// labelled scene (`src/ExampleScene.{h,cpp}`), a generated `Makefile` (STAPPLER_ROOT wired to the
// resolved engine, never overwriting an existing one), and carries the engine's `.clang-format`.
// Requires the native host toolchain to be installed and the engine present (clone or --engine).
ScaffoldResult scaffold_project(StringView name, StringView location, const Layout &layout,
		const String *engineOverride);

} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPISCAFFOLD_H_
