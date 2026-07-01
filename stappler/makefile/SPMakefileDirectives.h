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

#ifndef CORE_MAKEFILE_SPMAKEFILEDIRECTIVES_H_
#define CORE_MAKEFILE_SPMAKEFILEDIRECTIVES_H_

#include "SPCommon.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// The engine's own version string. Exposed as the make variable XLMAKE_VERSION by every Makefile
// (Makefile::setupBuiltinVariables), which the Stappler build system keys on to select its
// engine-native `init-xlmake.mk` path. xlmake reuses this for its own version display.
static constexpr StringView XlmakeVersion("1.0");

// Internal markers that the predefined $(WRITE) / $(APPEND) / $(MKDIR) / $(REMOVE) / $(CP) / $(ECHO)
// variables expand to. A leading \x01 (SOH) can never begin a real shell command, so a recipe line
// whose first token is one of these is unambiguously an in-process directive: an executor performs
// it directly (via the sp::filesystem API / a file write) instead of spawning a shell. The engine
// is the single source of truth: Makefile::setupBuiltinVariables assigns these to the $(WRITE)…
// variables (the producer) and an executor recognizes the expanded values (the detector), so the
// two can never drift apart.
static constexpr StringView WriteDirectiveMarker("\x01xlmake-write");
static constexpr StringView AppendDirectiveMarker("\x01xlmake-append");
static constexpr StringView MkdirDirectiveMarker("\x01xlmake-mkdir");
static constexpr StringView RemoveDirectiveMarker("\x01xlmake-remove");
static constexpr StringView CopyDirectiveMarker("\x01xlmake-copy");
static constexpr StringView EchoDirectiveMarker("\x01xlmake-echo");

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILEDIRECTIVES_H_ */
