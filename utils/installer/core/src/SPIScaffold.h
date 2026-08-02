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

#ifndef UTILS_INSTALLER_CORE_SRC_SPISCAFFOLD_H_
#define UTILS_INSTALLER_CORE_SRC_SPISCAFFOLD_H_

#include "SPICommon.h"
#include "SPIDirs.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

struct SP_PUBLIC ScaffoldResult : OperationResult {
	String path; // created project directory
};

// A valid project name: non-empty, letters/digits/`-`/`_` only (no spaces) — used verbatim as the
// folder and executable name.
SP_PUBLIC bool isValidProjectName(StringView name);

// Sanitize into a build-safe executable identifier (replace invalid chars with `_`, default `app`).
SP_PUBLIC String sanitizeProjectName(StringView name);

// Scaffold a buildable graphical project `name` inside `location`: writes the vendored minimal
// labelled scene (`src/ExampleScene.{h,cpp}`), a generated `Makefile` (STAPPLER_ROOT wired to the
// resolved engine, never overwriting an existing one), and carries the engine's `.clang-format`.
// Requires the native host toolchain to be installed and the engine present (clone or --engine).
SP_PUBLIC ScaffoldResult scaffoldProject(StringView name, StringView location, const Layout &layout,
		StringView engineOverride = StringView());

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPISCAFFOLD_H_
