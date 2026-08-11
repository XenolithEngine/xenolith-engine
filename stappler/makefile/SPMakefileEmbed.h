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

#ifndef STAPPLER_MAKEFILE_SPMAKEFILEEMBED_H_
#define STAPPLER_MAKEFILE_SPMAKEFILEEMBED_H_

#include "SPCommon.h"
#include "SPFilepath.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

/*
	BundleFS generator: turns a directory into a translation unit that stappler_filesystem
	serves under FileCategory::Embedded.

	This backs xlmake's in-process $(EMBED) directive. It is build-tool code and never ends up
	in an application: the format itself lives in stappler/filesystem/SPFilesystemEmbedded.h,
	which both this writer and the reader include.

	make/embed/embedfs.sh is the GNU-make fallback and produces byte-identical output for an
	uncompressed bundle — the two are compared against each other in the tests.
*/
struct EmbedConfig {
	StringView output; // path of the .cpp to write
	StringView bundleName; // mount name; the first path component at runtime
	StringView sourceDir; // directory to embed
	bool compress = false; // compress entries that actually get smaller
};

// Writes the translation unit. On failure, reports why through `err` (one message per line)
// and returns a failing Status.
SP_PUBLIC Status generateEmbeddedSource(const EmbedConfig &, const Callback<void(StringView)> &err);

} // namespace stappler::makefile

#endif /* STAPPLER_MAKEFILE_SPMAKEFILEEMBED_H_ */
