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

#include "SPGitRemote.h"
#include "SPGitProtocol.h"
#include "SPGitPack.h"
#include "SPGitCheckout.h"
#include "SPGitSubmodule.h"
#include "SPNetworkHandle.h"
#include "SPFilepath.h"

namespace STAPPLER_VERSIONIZED stappler::git {

static constexpr auto GIT_USER_AGENT = "git/2.43.0 (stappler)";
static constexpr uint32_t MAX_SUBMODULE_DEPTH = 10;

// Strip trailing slashes from a repository URL so we can append protocol paths.
static String normalizeBaseUrl(StringView url) {
	while (!url.empty() && url.back() == '/') { url = StringView(url.data(), url.size() - 1); }
	return url.str<mem_std::Interface>();
}

bool Remote::init(StringView url, sprt::dispatch::Looper *target) {
	_url = normalizeBaseUrl(url);
	_target = target;
	return !_url.empty();
}

void Remote::setCredentials(StringView user, StringView password) {
	_authUser = user.str<mem_std::Interface>();
	_authPassword = password.str<mem_std::Interface>();
}

void Remote::listRefs(RefListCallback &&cb) {
	_refsCallback = sp::move(cb);
	if (_target) {
		// run the blocking work on the Looper's worker pool; keep `this` alive
		Rc<Remote> self(this);
		_target->performAsync([self]() {
			auto res = self->performListRefs();
			self->deliverResult(self->_refsCallback, sp::move(res));
		});
	} else {
		auto res = performListRefs();
		deliverResult(_refsCallback, sp::move(res));
	}
}

void Remote::clone(CloneOptions &&opts, CloneCallback &&cb) {
	_cloneOptions = sp::move(opts);
	_cloneCallback = sp::move(cb);
	if (_target) {
		Rc<Remote> self(this);
		_target->performAsync([self]() {
			auto res = self->performClone();
			self->deliverResult(self->_cloneCallback, sp::move(res));
		});
	} else {
		auto res = performClone();
		deliverResult(_cloneCallback, sp::move(res));
	}
}

void Remote::emitProgress(const CloneProgress &prog) {
	if (!_cloneOptions.progress) {
		return;
	}
	if (_target) {
		Rc<Remote> self(this);
		CloneProgress p = prog;
		_target->performOnThread([self, p = sp::move(p)]() {
			if (self->_cloneOptions.progress) {
				self->_cloneOptions.progress(p);
			}
		}, this);
		_target->wakeup();
	} else {
		_cloneOptions.progress(prog);
	}
}

// Attach Basic credentials when the target host matches the repository host.
static void applyAuth(mem_std::NetworkHandle &h, StringView baseUrl, StringView repoUrl,
		StringView user, StringView password) {
	if (user.empty()) {
		return;
	}
	if (sameHost(baseUrl, repoUrl)) {
		h.setAuthority(user, password, network::AuthMethod::Basic);
	}
}

Status Remote::httpGet(StringView baseUrl, StringView pathSuffix, Bytes &out, long &httpCode) {
	mem_std::NetworkHandle h;
	String url = baseUrl.str<mem_std::Interface>();
	url.append(pathSuffix.data(), pathSuffix.size());

	h.init(network::Method::Get, url);
	h.setUserAgent(GIT_USER_AGENT);
	h.addHeader("Git-Protocol", "version=2");
	applyAuth(h, baseUrl, StringView(_url), StringView(_authUser), StringView(_authPassword));
	h.setReceiveCallback([&out](char *data, size_t size) -> size_t {
		out.insert(out.end(), reinterpret_cast<uint8_t *>(data),
				reinterpret_cast<uint8_t *>(data) + size);
		return size;
	});

	if (!h.perform()) {
		return Status::ErrorNotPermitted;
	}
	httpCode = h.getResponseCode();
	return (httpCode == 200) ? Status::Ok : Status::ErrorNotFound;
}

Status Remote::httpPost(StringView baseUrl, StringView pathSuffix, BytesView body,
		StringView contentType, Bytes &out, long &httpCode,
		const Function<void(int64_t, int64_t)> &onDownload) {
	mem_std::NetworkHandle h;
	String url = baseUrl.str<mem_std::Interface>();
	url.append(pathSuffix.data(), pathSuffix.size());

	h.init(network::Method::Post, url);
	h.setUserAgent(GIT_USER_AGENT);
	h.addHeader("Git-Protocol", "version=2");
	h.addHeader("Accept", "application/x-git-upload-pack-result");
	applyAuth(h, baseUrl, StringView(_url), StringView(_authUser), StringView(_authPassword));
	if (onDownload) {
		h.setDownloadProgress([&onDownload](int64_t total, int64_t now) -> int {
			onDownload(now, total);
			return 0;
		});
	}
	h.setSendData(body, contentType);
	h.setReceiveCallback([&out](char *data, size_t size) -> size_t {
		out.insert(out.end(), reinterpret_cast<uint8_t *>(data),
				reinterpret_cast<uint8_t *>(data) + size);
		return size;
	});

	if (!h.perform()) {
		return Status::ErrorNotPermitted;
	}
	httpCode = h.getResponseCode();
	return (httpCode == 200) ? Status::Ok : Status::ErrorNotFound;
}

RefListResult Remote::performListRefs() {
	RefListResult res;

	// Step 1: GET the v2 service advertisement.
	Bytes advBody;
	res.status = httpGet(StringView(_url), StringView("/info/refs?service=git-upload-pack"),
			advBody, res.httpCode);
	if (res.status != Status::Ok) {
		return res;
	}

	auto adv = parseServiceAdvertisement(BytesView(advBody.data(), advBody.size()));
	if (adv.version < 2 || !adv.hasCapability("ls-refs")) {
		res.status = Status::ErrorNotSupported;
		return res;
	}
	res.format = adv.format;
	res.capabilities = sp::move(adv.capabilities);

	// Step 2: POST the ls-refs command.
	Bytes reqBody = buildLsRefsRequest(adv.format);
	Bytes respBody;
	res.status = httpPost(StringView(_url), StringView("/git-upload-pack"),
			BytesView(reqBody.data(), reqBody.size()),
			StringView("application/x-git-upload-pack-request"), respBody, res.httpCode);
	if (res.status != Status::Ok) {
		return res;
	}

	res.status =
			parseLsRefsResponse(BytesView(respBody.data(), respBody.size()), adv.format, res.refs);
	return res;
}

CloneResult Remote::performClone() {
	CloneResult res;

	if (_cloneOptions.oid.empty() || _cloneOptions.targetDir.empty()) {
		res.status = Status::ErrorInvalidArguemnt;
		return res;
	}

	res.status = cloneInto(StringView(_url), _cloneOptions.oid, StringView(_cloneOptions.targetDir),
			0, res);
	return res;
}

Status Remote::cloneInto(StringView baseUrl, const Oid &want, StringView destDir,
		uint32_t recursion, CloneResult &agg) {
	String base = normalizeBaseUrl(baseUrl);

	auto emitStage = [&](CloneStage stage, int64_t received, int64_t total) {
		CloneProgress p;
		p.stage = stage;
		p.url = base;
		p.submoduleDepth = recursion;
		p.bytesReceived = received;
		p.bytesTotal = total;
		emitProgress(p);
	};

	// Step 1: confirm v2 + fetch capability + object-format.
	emitStage(CloneStage::Connecting, 0, 0);
	Bytes advBody;
	long httpCode = 0;
	auto st = httpGet(StringView(base), StringView("/info/refs?service=git-upload-pack"), advBody,
			httpCode);
	agg.httpCode = httpCode;
	if (st != Status::Ok) {
		return st;
	}
	auto adv = parseServiceAdvertisement(BytesView(advBody.data(), advBody.size()));
	if (adv.version < 2 || !adv.hasCapability("fetch")) {
		return Status::ErrorNotSupported;
	}

	// Step 2: POST the fetch command (shallow) and demux the packfile.
	emitStage(CloneStage::Downloading, 0, 0);
	int64_t lastEmit = 0;
	Function<void(int64_t, int64_t)> onDownload = [&](int64_t received, int64_t total) {
		if (received - lastEmit >= 65'536 || (total > 0 && received >= total)) {
			lastEmit = received;
			emitStage(CloneStage::Downloading, received, total);
		}
	};

	Bytes reqBody = buildFetchRequest(want, _cloneOptions.depth, adv.format);
	Bytes respBody;
	st = httpPost(StringView(base), StringView("/git-upload-pack"),
			BytesView(reqBody.data(), reqBody.size()),
			StringView("application/x-git-upload-pack-request"), respBody, httpCode, onDownload);
	agg.httpCode = httpCode;
	if (st != Status::Ok) {
		return st;
	}

	Bytes pack;
	Vector<Oid> shallow;
	st = parseFetchResponse(BytesView(respBody.data(), respBody.size()), adv.format, pack, shallow);
	if (st != Status::Ok) {
		return st;
	}

	// Step 3: decode the packfile.
	emitStage(CloneStage::Unpacking, 0, 0);
	ObjectStore store;
	st = parsePack(BytesView(pack.data(), pack.size()), adv.format, store);
	if (st != Status::Ok) {
		return st;
	}
	agg.objectsReceived += store.size();

	// Step 4: check out the working tree.
	emitStage(CloneStage::CheckingOut, 0, 0);
	CheckoutStats stats;
	st = checkout(store, want, destDir, stats);
	if (st != Status::Ok) {
		return st;
	}
	if (recursion == 0) {
		agg.commit = stats.commit;
		agg.firstBlobOid = stats.firstBlobOid;
		agg.firstBlobPath = stats.firstBlobPath;
	}
	agg.filesWritten += stats.filesWritten;
	agg.bytesWritten += stats.bytesWritten;

	// Step 5: recurse into submodules.
	if (_cloneOptions.recurseSubmodules && !stats.gitlinks.empty()
			&& recursion < MAX_SUBMODULE_DEPTH) {
		auto specs = parseGitmodules(BytesView(stats.gitmodules.data(), stats.gitmodules.size()));
		for (auto &gl : stats.gitlinks) {
			++agg.submodulesFound;

			// find the .gitmodules entry whose path matches this gitlink
			StringView subUrlSpec;
			for (auto &s : specs) {
				if (StringView(s.path) == StringView(gl.path)) {
					subUrlSpec = StringView(s.url);
					break;
				}
			}
			if (subUrlSpec.empty()) {
				++agg.submodulesSkipped; // gitlink without a .gitmodules entry
				continue;
			}

			String subUrl = resolveSubmoduleUrl(StringView(base), subUrlSpec);
			if (!isHttpUrl(StringView(subUrl))) {
				++agg.submodulesSkipped; // ssh / git:// / unsupported scheme
				continue;
			}

			String subDir = filepath::merge<mem_std::Interface>(destDir, StringView(gl.path));
			auto subStatus =
					cloneInto(StringView(subUrl), gl.oid, StringView(subDir), recursion + 1, agg);
			if (subStatus == Status::Ok) {
				++agg.submodulesCloned;
			} else {
				++agg.submodulesFailed; // non-fatal: keep the parent clone successful
			}
		}
	}

	return Status::Ok;
}

} // namespace stappler::git
