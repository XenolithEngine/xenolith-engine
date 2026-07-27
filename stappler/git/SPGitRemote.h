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

#ifndef STAPPLER_GIT_SPGITREMOTE_H_
#define STAPPLER_GIT_SPGITREMOTE_H_

#include "SPGit.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::git {

// Asynchronous handle to a remote git repository reachable over HTTP(S).
//
// The blocking network/checkout work runs on the target Looper's worker pool
// (via Looper::performAsync), so the caller never blocks and no dedicated thread
// is spawned; results and progress are delivered back on the Looper thread. If
// no target Looper is provided, operations run synchronously inline.
//
// Current phase of a clone, reported through the progress callback.
enum class CloneStage {
	Connecting, // requesting the refs advertisement
	Downloading, // receiving the packfile
	Unpacking, // decoding pack objects
	CheckingOut, // writing the working tree
};

struct CloneProgress {
	CloneStage stage = CloneStage::Connecting;
	String url; // repository currently being processed
	uint32_t submoduleDepth = 0; // 0 = top-level repo, >0 = nested submodule
	int64_t bytesReceived = 0; // meaningful during Downloading
	int64_t bytesTotal = 0; // 0 if the server didn't advertise a size
};

using CloneProgressCallback = Function<void(const CloneProgress &)>;

// Options for a shallow (history-less) clone of a single ref.
struct CloneOptions {
	Oid oid; // commit/tag oid to fetch (obtained from listRefs)
	String ref; // optional ref name, for reporting only
	String targetDir; // absolute directory to write the working tree into
	uint32_t depth = 1; // 1 = shallow (no history); 0 = full history
	bool recurseSubmodules = false; // also clone submodules recursively

	// Optional progress callback. Fires on the target looper thread (or the
	// worker thread if no target looper was set), same as the completion callback.
	CloneProgressCallback progress;
};

struct CloneResult {
	Status status = Status::Ok;
	long httpCode = 0;
	Oid commit; // resolved commit
	size_t objectsReceived = 0;
	size_t filesWritten = 0;
	size_t bytesWritten = 0;
	Oid firstBlobOid; // first regular file written (for integrity checks)
	String firstBlobPath;
	// Submodule accounting (recursive):
	size_t submodulesFound = 0;
	size_t submodulesCloned = 0;
	size_t submodulesFailed = 0;
	size_t submodulesSkipped = 0; // non-http / not listed in .gitmodules
};

// A Remote instance performs a single operation (create one per query).
class SP_PUBLIC Remote : public Ref {
public:
	using RefListCallback = Function<void(RefListResult &&)>;
	using CloneCallback = Function<void(CloneResult &&)>;

	virtual ~Remote() = default;

	// `url` is the repository base URL, e.g. https://host/user/repo(.git)
	// `target` (optional) is the Looper whose worker pool runs the operation and
	// on whose thread the callbacks fire. The Looper must have workers
	// (LooperInfo::workersCount > 0). Without a target, work runs synchronously.
	bool init(StringView url, sprt::dispatch::Looper *target = nullptr);

	// HTTP Basic credentials. Applied only to requests whose host matches the
	// repository host (submodules on other hosts are fetched anonymously).
	// For GitHub, `password` is a personal access token.
	void setCredentials(StringView user, StringView password);

	// Asynchronously discover refs (on the Looper worker pool).
	void listRefs(RefListCallback &&cb);

	// Asynchronously shallow-clone `opts.oid` into `opts.targetDir`.
	void clone(CloneOptions &&opts, CloneCallback &&cb);

	StringView getUrl() const { return _url; }

protected:
	RefListResult performListRefs();
	CloneResult performClone();

	// Recursive clone worker: fetch+checkout `want` from `baseUrl` into `destDir`,
	// then (if enabled) resolve and clone submodules. Aggregates into `agg`.
	Status cloneInto(StringView baseUrl, const Oid &want, StringView destDir, uint32_t recursion,
			CloneResult &agg);

	// Emit a progress event to the caller (marshaled like the result callback).
	void emitProgress(const CloneProgress &);

	// Blocking HTTP helpers (run on the worker thread). `pathSuffix` is appended
	// to `baseUrl`. Credentials are applied when the host matches. `onDownload`
	// (if set) receives (bytesReceived, bytesTotal) during transfer. Return
	// Status::Ok with httpCode/body filled in.
	Status httpGet(StringView baseUrl, StringView pathSuffix, Bytes &out, long &httpCode);
	Status httpPost(StringView baseUrl, StringView pathSuffix, BytesView body,
			StringView contentType, Bytes &out, long &httpCode,
			const Function<void(int64_t, int64_t)> &onDownload = {});

	// Deliver a result to the caller: on the target looper thread if set,
	// otherwise inline on the worker thread.
	template <typename T>
	void deliverResult(Function<void(T &&)> &cb, T &&res) {
		if (_target) {
			Rc<Remote> self(this);
			auto cbCopy = cb;
			T r = sp::move(res);
			_target->performOnThread([self, cbCopy = sp::move(cbCopy), r = sp::move(r)]() mutable {
				if (cbCopy) {
					cbCopy(sp::move(r));
				}
			}, this);
			_target->wakeup();
		} else if (cb) {
			cb(sp::move(res));
		}
	}

	String _url;
	String _authUser;
	String _authPassword;
	sprt::dispatch::Looper *_target = nullptr;
	RefListCallback _refsCallback;
	CloneCallback _cloneCallback;
	CloneOptions _cloneOptions;
};

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGITREMOTE_H_ */
