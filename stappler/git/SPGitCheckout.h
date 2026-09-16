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

#ifndef STAPPLER_GIT_SPGITCHECKOUT_H_
#define STAPPLER_GIT_SPGITCHECKOUT_H_

#include "SPGitPack.h"

// Materialize a working tree on disk from an in-memory object store: resolve the
// wanted oid to its commit, walk the root tree and write blobs/dirs/symlinks.

namespace STAPPLER_VERSIONIZED stappler::git {

// A submodule (gitlink) reference discovered during checkout.
struct GitlinkEntry {
	String path; // path relative to the repo root
	Oid oid; // pinned submodule commit
};

struct SP_PUBLIC CheckoutStats {
	Oid commit; // resolved commit oid
	size_t filesWritten = 0;
	size_t dirsCreated = 0;
	size_t symlinks = 0;
	size_t bytesWritten = 0;

	// The first regular file written (for an end-to-end integrity check):
	// re-reading `firstBlobPath` from disk must hash back to `firstBlobOid`.
	Oid firstBlobOid;
	String firstBlobPath;

	// Gitlink (submodule) entries and the root `.gitmodules` content, for the
	// recursive submodule clone driven by the caller.
	Vector<GitlinkEntry> gitlinks;
	Bytes gitmodules;
};

// Write the working tree for `want` (a commit, or a tag peeled to a commit) into
// the absolute directory `destDir`. Returns Status::Ok on success.
SP_PUBLIC Status checkout(const ObjectStore &, const Oid &want, StringView destDir,
		CheckoutStats &);

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGITCHECKOUT_H_ */
