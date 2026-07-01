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

#ifndef CORE_MAKEFILE_SPMAKEFILEOBSERVER_H_
#define CORE_MAKEFILE_SPMAKEFILEOBSERVER_H_

#include "SPMakefile.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// A poll-driven change-observer for a project. It holds the absolute paths of a goal's source inputs
// (see Makefile::getSourceInputs) and keeps a single 64-bit "fingerprint" over their modification
// times. The fingerprint is a unique number that changes whenever any watched file is edited, added,
// removed or reverted — the signal a live-reload consumer acts on.
//
// The observer owns NO thread: the consumer calls check() from its own loop, on whatever cadence it
// likes (e.g. once per app-update tick). After construction it only stat()s its own heap copy of the
// path list, so it is independent of the Makefile it was built from.
class SP_PUBLIC SourceObserver : public Ref {
public:
	virtual ~SourceObserver() = default;

	// Load `projectDir` for the current host target, collect the source inputs of `goal` (default
	// "all"), and arm the observer with the initial fingerprint. Returns null if the project makefile
	// cannot be found/loaded. Self-contained (uses its own temporary memory pool during the load).
	static Rc<SourceObserver> createForProject(StringView projectDir, StringView goal,
			ErrorReporter &);

	// Copy the watched paths into owned heap storage and compute the baseline fingerprint.
	bool init(SpanView<StringView> watchedPaths);

	// Re-stat every watched path and recompute the fingerprint. Returns true (and updates the stored
	// fingerprint) if it changed since the previous check()/init(); false when unchanged.
	bool check();

	// The current fingerprint (the baseline right after init()).
	uint64_t getFingerprint() const { return _fingerprint; }

	// The number of files being watched.
	size_t getWatchedCount() const { return _paths.size(); }

protected:
	// FNV fold over (count + per-path name hash + mtime). See SPMakefileObserver.cc.
	uint64_t computeFingerprint() const;

	mem_std::Vector<mem_std::String> _paths;
	uint64_t _fingerprint = 0;
};

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILEOBSERVER_H_ */
