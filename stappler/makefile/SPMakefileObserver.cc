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

// The poll-driven project change-observer. Part of the makefile unity build (included from
// SPMakefile.cpp). See SPMakefileObserver.h.

#include "SPMakefileObserver.h"
#include "SPMakefileProject.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

Rc<SourceObserver> SourceObserver::createForProject(StringView projectDir, StringView goal,
		ErrorReporter &err) {
	Rc<SourceObserver> result;
	// Own a temporary pool for the load so any transient allocations are freed here and any API that
	// expects an active memory context has one.
	auto pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	memory::perform([&] {
		auto mk = loadProject(projectDir, err);
		if (!mk) {
			return;
		}
		// Collect the source inputs; the paths point into the makefile's pool and are copied into the
		// observer's own heap storage by init() before the makefile is dropped below.
		mem_std::Interface::VectorType<StringView> paths;
		mk->getSourceInputs(goal.empty() ? StringView("all") : goal,
				[&](StringView p) { paths.emplace_back(p); }, err);
		result = Rc<SourceObserver>::create(SpanView<StringView>(paths.data(), paths.size()));
	}, pool);
	memory::pool::destroy(pool);
	return result;
}

bool SourceObserver::init(SpanView<StringView> watchedPaths) {
	_paths.reserve(watchedPaths.size());
	for (auto &p : watchedPaths) { _paths.emplace_back(p.data(), p.size()); }
	_fingerprint = computeFingerprint(); // baseline: check() then reports only subsequent changes
	return true;
}

uint64_t SourceObserver::computeFingerprint() const {
	// FNV-1a-style 64-bit fold over (file count, per-path name hash, per-path mtime). Folding the
	// path's name hash as well as its mtime means an add/remove/rename flips the result even when two
	// files share an mtime; a missing file folds in a 0 mtime. The path list is fixed after init(), so
	// the fingerprint is deterministic per call.
	constexpr uint64_t kOffset = 1'469'598'103'934'665'603ull;
	constexpr uint64_t kPrime = 1'099'511'628'211ull;

	uint64_t fp = kOffset;
	auto mix = [&](uint64_t v) {
		fp ^= v;
		fp *= kPrime;
	};

	mix(_paths.size());
	for (auto &p : _paths) {
		mix(sprt::hash64(p.data(), p.size()));
		filesystem::Stat st;
		uint64_t mtime = 0;
		if (filesystem::stat(FileInfo{StringView(p.data(), p.size())}, st)) {
			mtime = st.mtime.toMicros();
		}
		mix(mtime);
	}
	return fp;
}

bool SourceObserver::check() {
	auto fp = computeFingerprint();
	if (fp != _fingerprint) {
		_fingerprint = fp;
		return true;
	}
	return false;
}

} // namespace stappler::makefile
