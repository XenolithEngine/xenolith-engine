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

#include "SPCommon.h"
#include "SPGit.h"
#include "SPGitObject.h"
#include "SPGitProtocol.h"
#include "SPGitPack.h"
#include "SPGitSubmodule.h"
#include "SPGitRemote.h"
#include "SPFilesystem.h"

#include <sprt/runtime/stream.h>
#include <sprt/runtime/dispatch/looper.h>

#include <zlib.h> // compress (for the inflate roundtrip test)
#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::git {

namespace dispatch = sprt::dispatch;

static constexpr auto DEFAULT_URL = "https://github.com/XenolithEngine/xenolith-engine";

static int s_failures = 0;

static void check(bool cond, StringView name) {
	sprt::cout << (cond ? "[ OK ] " : "[FAIL] ") << name << "\n";
	if (!cond) {
		++s_failures;
	}
}

static bool bytesEq(BytesView b, StringView s) {
	if (b.size() != s.size()) {
		return false;
	}
	for (size_t i = 0; i < b.size(); ++i) {
		if (b.data()[i] != uint8_t(s.data()[i])) {
			return false;
		}
	}
	return true;
}

// Deterministic, offline checks of the pkt-line framing and v2 parsers.
static void runOfflineTests() {
	sprt::cout << "-- offline protocol tests --\n";

	// (1) The ls-refs request must match git's exact pkt-line framing.
	auto req = buildLsRefsRequest(ObjectFormat::Sha1);
	StringView expected = "0014command=ls-refs\n"
						  "0017object-format=sha1\n"
						  "0001"
						  "0009peel\n"
						  "000csymrefs\n"
						  "0014ref-prefix HEAD\n"
						  "001bref-prefix refs/heads/\n"
						  "001aref-prefix refs/tags/\n"
						  "0000";
	check(bytesEq(BytesView(req.data(), req.size()), expected), "build ls-refs request framing");

	// (2) Service advertisement parsing.
	StringView adv = "001e# service=git-upload-pack\n"
					 "0000"
					 "000eversion 2\n"
					 "000cls-refs\n"
					 "0017object-format=sha1\n"
					 "0000";
	auto sa = parseServiceAdvertisement(
			BytesView(reinterpret_cast<const uint8_t *>(adv.data()), adv.size()));
	check(sa.status == Status::Ok, "advertisement status Ok");
	check(sa.version == 2, "advertisement version is 2");
	check(sa.format == ObjectFormat::Sha1, "advertisement object-format sha1");
	check(sa.hasCapability("ls-refs"), "advertisement has ls-refs capability");

	// (3) ls-refs response parsing (HEAD symref, branch, annotated tag with peeled).
	String oidHead(40, '0');
	oidHead[39] = '1';
	String oidTag(40, '0');
	oidTag[39] = 'a';
	String oidPeeled(40, '0');
	oidPeeled[39] = 'b';

	PktLineWriter w;
	String l0 = oidHead;
	l0 += " HEAD symref-target:refs/heads/main\n";
	String l1 = oidHead;
	l1 += " refs/heads/main\n";
	String l2 = oidTag;
	l2 += " refs/tags/v1.0 peeled:";
	l2 += oidPeeled;
	l2 += "\n";
	w.writeLine(StringView(l0));
	w.writeLine(StringView(l1));
	w.writeLine(StringView(l2));
	w.writeFlush();
	auto resp = w.takeData();

	Vector<RefInfo> refs;
	auto st = parseLsRefsResponse(BytesView(resp.data(), resp.size()), ObjectFormat::Sha1, refs);
	check(st == Status::Ok, "ls-refs parse status Ok");
	check(refs.size() == 3, "ls-refs parsed 3 refs");
	if (refs.size() == 3) {
		check(refs[0].isHead() && StringView(refs[0].symref) == "refs/heads/main",
				"HEAD symref target parsed");
		check(refs[0].oid.str() == oidHead, "oid hex roundtrip");
		check(refs[1].isBranch() && StringView(refs[1].name) == "refs/heads/main",
				"branch parsed");
		check(refs[2].isTag() && !refs[2].peeled.empty() && refs[2].peeled.str() == oidPeeled,
				"annotated tag with peeled parsed");
	}
}

static void appendStr(String &s, StringView v) { s.append(v.data(), v.size()); }

// Deterministic, offline checks of the packfile / object / delta machinery.
static void runOfflineStage2Tests() {
	sprt::cout << "-- offline stage-2 tests --\n";

	// (1) fetch request framing (shallow depth 1).
	Oid oid;
	oid.format = ObjectFormat::Sha1;
	oid.bytes[19] = 1;
	String hex;
	oid.toHex([&](StringView h) { hex = h.str<memory::StandartInterface>(); });

	auto req = buildFetchRequest(oid, 1, ObjectFormat::Sha1);
	String exp;
	appendStr(exp, "0012command=fetch\n");
	appendStr(exp, "0017object-format=sha1\n");
	appendStr(exp, "0001");
	appendStr(exp, "0010no-progress\n");
	appendStr(exp, "000eofs-delta\n");
	appendStr(exp, "000ddeepen 1\n");
	appendStr(exp, "0032want ");
	appendStr(exp, hex);
	appendStr(exp, "\n");
	appendStr(exp, "0009done\n");
	appendStr(exp, "0000");
	check(bytesEq(BytesView(req.data(), req.size()), StringView(exp)), "build fetch request framing");

	// (2) git object identity hashes (well-known empty objects).
	uint8_t none[1] = {0};
	check(hashObject(ObjectType::Blob, BytesView(none, 0)).str()
					== "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391",
			"empty blob hash");
	check(hashObject(ObjectType::Tree, BytesView(none, 0)).str()
					== "4b825dc642cb6eb9a060e54bf8d69288fbee4904",
			"empty tree hash");
	check(parseTree(BytesView(none, 0), ObjectFormat::Sha1).empty(), "empty tree has no entries");

	// (3) delta application (copy + insert + copy).
	uint8_t base[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
	uint8_t delta[] = {0x08, 0x08, 0x90, 0x04, 0x02, 'X', 'Y', 0x91, 0x06, 0x02};
	Bytes dout;
	auto dst = applyDelta(BytesView(base, 8), BytesView(delta, sizeof(delta)), dout);
	check(dst == Status::Ok && bytesEq(BytesView(dout.data(), dout.size()), "abcdXYgh"),
			"delta copy+insert application");

	// (4) zlib inflate roundtrip.
	StringView msg("The quick brown fox jumps over the lazy dog. "
				   "data data data data data data data data data data.");
	uLong bound = compressBound(uLong(msg.size()));
	Bytes comp;
	comp.resize(bound);
	int zr = compress(reinterpret_cast<Bytef *>(comp.data()), &bound,
			reinterpret_cast<const Bytef *>(msg.data()), uLong(msg.size()));
	Bytes inflated;
	size_t consumed = 0;
	auto ist = inflateStream(comp.data(), bound, inflated, consumed);
	check(zr == Z_OK && ist == Status::Ok
					&& bytesEq(BytesView(inflated.data(), inflated.size()), msg),
			"zlib inflate roundtrip");
}

// Deterministic, offline checks of the submodule / auth helpers.
static void runOfflineStage3Tests() {
	sprt::cout << "-- offline stage-3 tests (submodule helpers) --\n";

	StringView gm = "[submodule \"libs/foo\"]\n"
					"\tpath = libs/foo\n"
					"\turl = ../foo.git\n"
					"[submodule \"bar\"]\n"
					"\tpath = third/bar\n"
					"\turl = https://other.example/bar.git\n";
	auto specs = parseGitmodules(BytesView(reinterpret_cast<const uint8_t *>(gm.data()), gm.size()));
	check(specs.size() == 2, "parse .gitmodules: 2 submodules");
	if (specs.size() == 2) {
		check(StringView(specs[0].path) == "libs/foo" && StringView(specs[0].url) == "../foo.git",
				"submodule 0 path/url");
		check(StringView(specs[1].path) == "third/bar"
						&& StringView(specs[1].url) == "https://other.example/bar.git",
				"submodule 1 path/url");
	}

	check(resolveSubmoduleUrl("https://github.com/U/repo.git", "../other.git")
					== "https://github.com/U/other.git",
			"resolve ../ relative url");
	check(resolveSubmoduleUrl("https://github.com/U/repo.git", "../../G/x.git")
					== "https://github.com/G/x.git",
			"resolve ../../ relative url");
	check(resolveSubmoduleUrl("https://github.com/U/repo.git", "https://h/x.git")
					== "https://h/x.git",
			"resolve absolute passthrough");

	check(sameHost("https://github.com/a/b.git", "https://github.com/c/d.git"), "sameHost true");
	check(!sameHost("https://github.com/a", "https://gitlab.com/a"), "sameHost false");
	check(isHttpUrl("https://x/y") && !isHttpUrl("git@github.com:a/b.git") && !isHttpUrl("git://x/y"),
			"isHttpUrl classification");
}

// Live connection to a real public repository: discover refs and print them.
static void runLiveTest(StringView url) {
	sprt::cout << "-- live ls-refs against " << url << " --\n";

	auto looper = dispatch::Looper::acquire(dispatch::LooperInfo{
		.name = StringView("git-test"),
		.workersCount = 2, // Remote runs its work on the Looper worker pool
	});
	if (!looper) {
		sprt::cerr << "failed to initialize the event loop\n";
		++s_failures;
		return;
	}

	auto remote = Rc<Remote>::create(url, looper);
	if (!remote) {
		sprt::cerr << "invalid repository url\n";
		++s_failures;
		return;
	}

	bool done = false;
	RefListResult result;
	remote->listRefs([&](RefListResult &&r) {
		result = sp::move(r);
		done = true;
		looper->wakeup();
	});

	// Drive the loop until the worker thread delivers the result (with an overall
	// timeout so a hung connection can't wedge the test forever).
	int guard = 0;
	while (!done && guard < 600) {
		looper->run(dispatch::TimeInterval::milliseconds(100));
		++guard;
	}

	if (!done) {
		sprt::cerr << "timed out waiting for ls-refs\n";
		++s_failures;
		return;
	}

	sprt::cout << "ls-refs result: http " << result.httpCode << ", status "
			   << int32_t(result.status) << "\n";
	check(result.status == Status::Ok, "ls-refs succeeded");
	if (result.status != Status::Ok) {
		return;
	}

	size_t branches = 0, tags = 0;
	bool hasHead = false;
	for (auto &ref : result.refs) {
		if (ref.isHead()) {
			hasHead = true;
		} else if (ref.isBranch()) {
			++branches;
		} else if (ref.isTag()) {
			++tags;
		}
	}

	sprt::cout << "discovered " << result.refs.size() << " refs (" << branches << " branches, "
			   << tags << " tags), object-format="
			   << (result.format == ObjectFormat::Sha256 ? "sha256" : "sha1") << "\n";

	// Print a compact listing (HEAD + first refs) for eyeballing.
	size_t printed = 0;
	for (auto &ref : result.refs) {
		if (!ref.isHead() && printed >= 12) {
			continue;
		}
		ref.oid.toHex([&](StringView hex) { sprt::cout << "  " << hex << "  " << ref.name; });
		if (!ref.symref.empty()) {
			sprt::cout << " -> " << ref.symref;
		}
		if (!ref.peeled.empty()) {
			ref.peeled.toHex([&](StringView hex) { sprt::cout << " (peeled " << hex << ")"; });
		}
		sprt::cout << "\n";
		if (!ref.isHead()) {
			++printed;
		}
	}

	check(!result.refs.empty(), "ref list is non-empty");
	check(hasHead, "HEAD is present");
	check(branches > 0, "at least one branch present");

	// --- shallow clone of the resolved HEAD (or first branch) ---

	Oid target;
	String targetRef;
	for (auto &ref : result.refs) {
		if (ref.isHead() && !ref.oid.empty()) {
			target = ref.oid;
			targetRef = ref.name;
			break;
		}
	}
	if (target.empty()) {
		for (auto &ref : result.refs) {
			if (ref.isBranch()) {
				target = ref.oid;
				targetRef = ref.name;
				break;
			}
		}
	}
	if (target.empty()) {
		check(false, "found a ref to clone");
		return;
	}

	String destDir("/tmp/stappler-git-clone-test");
	filesystem::remove(FileInfo(StringView(destDir)), true); // clean any previous run

	sprt::cout << "-- shallow clone (" << targetRef << ") into " << destDir << " --\n";

	auto cloneRemote = Rc<Remote>::create(url, looper);

	size_t progressEvents = 0;
	bool sawDownloading = false, sawCheckingOut = false, sawSubmoduleStage = false;
	int lastStage = -1;
	int64_t lastBytes = 0;

	CloneOptions opts;
	opts.oid = target;
	opts.ref = targetRef;
	opts.targetDir = destDir;
	opts.depth = 1;
	opts.recurseSubmodules = true;
	opts.progress = [&](const CloneProgress &p) {
		++progressEvents;
		if (p.stage == CloneStage::Downloading) {
			sawDownloading = true;
		}
		if (p.stage == CloneStage::CheckingOut) {
			sawCheckingOut = true;
		}
		if (p.submoduleDepth > 0) {
			sawSubmoduleStage = true;
		}
		static const char *names[] = {"connecting", "downloading", "unpacking", "checking-out"};
		bool stageChanged = int(p.stage) != lastStage;
		bool bigDownload = p.stage == CloneStage::Downloading
				&& p.bytesReceived - lastBytes >= 8 * 1024 * 1024;
		if (stageChanged || bigDownload) {
			lastStage = int(p.stage);
			lastBytes = p.bytesReceived;
			sprt::cout << "  [progress] depth=" << p.submoduleDepth << " " << names[int(p.stage)];
			if (p.stage == CloneStage::Downloading && p.bytesReceived > 0) {
				sprt::cout << " " << (p.bytesReceived / 1024) << "KB";
			}
			sprt::cout << " " << p.url << "\n";
		}
	};

	bool cdone = false;
	CloneResult cres;
	cloneRemote->clone(sp::move(opts), [&](CloneResult &&r) {
		cres = sp::move(r);
		cdone = true;
		looper->wakeup();
	});

	int cguard = 0;
	while (!cdone && cguard < 1800) {
		looper->run(dispatch::TimeInterval::milliseconds(100));
		++cguard;
	}
	if (!cdone) {
		sprt::cerr << "timed out waiting for clone\n";
		++s_failures;
		return;
	}

	sprt::cout << "clone: http " << cres.httpCode << ", status " << int32_t(cres.status) << ", objects "
			   << cres.objectsReceived << ", files " << cres.filesWritten << ", bytes "
			   << cres.bytesWritten << "\n";
	check(cres.status == Status::Ok, "shallow clone succeeded");
	if (cres.status != Status::Ok) {
		return;
	}
	check(cres.objectsReceived > 0, "clone received objects");
	check(cres.filesWritten > 0, "clone wrote files");

	// Integrity: re-read the first written blob and hash it back to its tree oid.
	if (!cres.firstBlobPath.empty()) {
		auto data = filesystem::readIntoMemory<memory::StandartInterface>(
				FileInfo(StringView(cres.firstBlobPath)));
		auto rehash = hashObject(ObjectType::Blob, BytesView(data.data(), data.size()));
		check(rehash.str() == cres.firstBlobOid.str(),
				"checked-out blob re-hashes to its tree oid");
		sprt::cout << "  verified " << cres.firstBlobPath << "\n";
	}

	// Spot-check that an expected top-level directory materialized.
	String topDir = destDir;
	appendStr(topDir, "/stappler");
	check(filesystem::exists(FileInfo(StringView(topDir))), "top-level 'stappler' dir exists");

	// --- recursive submodules ---
	sprt::cout << "submodules: found " << cres.submodulesFound << ", cloned " << cres.submodulesCloned
			   << ", skipped " << cres.submodulesSkipped << ", failed " << cres.submodulesFailed
			   << "\n";
	check(cres.submodulesFound >= 1, "at least one submodule discovered (.gitmodules gitlink)");
	check(cres.submodulesFailed == 0, "no submodule hard failures");
	check(cres.submodulesCloned >= 1, "at least one submodule cloned (musl-libc)");

	// --- progress callback ---
	sprt::cout << "progress: " << progressEvents << " events\n";
	check(progressEvents > 0, "progress callback fired");
	check(sawDownloading, "progress reported Downloading stage");
	check(sawCheckingOut, "progress reported CheckingOut stage");
	check(sawSubmoduleStage, "progress reported a submodule (depth > 0)");
}

// Optional: Basic-auth clone of a private repo, gated by env credentials.
static void runAuthTest(StringView url, StringView user, StringView pass) {
	sprt::cout << "-- Basic-auth ls-refs against " << url << " --\n";

	auto looper = dispatch::Looper::acquire(dispatch::LooperInfo{
		.name = StringView("git-auth"),
		.workersCount = 2, // Remote runs its work on the Looper worker pool
	});
	auto remote = Rc<Remote>::create(url, looper);
	if (!remote) {
		check(false, "auth: valid url");
		return;
	}
	remote->setCredentials(user, pass);

	bool done = false;
	RefListResult result;
	remote->listRefs([&](RefListResult &&r) {
		result = sp::move(r);
		done = true;
		looper->wakeup();
	});
	int guard = 0;
	while (!done && guard < 600) {
		looper->run(dispatch::TimeInterval::milliseconds(100));
		++guard;
	}
	sprt::cout << "auth ls-refs: http " << result.httpCode << ", status " << int32_t(result.status)
			   << ", refs " << result.refs.size() << "\n";
	check(done && result.status == Status::Ok && !result.refs.empty(),
			"authenticated ls-refs succeeded");
}

static int run(int argc, const char *argv[]) {
	runOfflineTests();
	runOfflineStage2Tests();
	runOfflineStage3Tests();

	if (::getenv("GIT_SKIP_LIVE")) {
		sprt::cout << "-- live test skipped (GIT_SKIP_LIVE set) --\n";
	} else {
		const char *env = ::getenv("GIT_REMOTE_TEST_URL");
		StringView url = (argc > 1) ? StringView(argv[1])
				: (env ? StringView(env) : StringView(DEFAULT_URL));
		runLiveTest(url);
	}

	// Optional authenticated clone against a private repo.
	const char *authUrl = ::getenv("GIT_AUTH_URL");
	const char *authUser = ::getenv("GIT_AUTH_USER");
	const char *authPass = ::getenv("GIT_AUTH_PASS");
	if (authUrl && authUser && authPass) {
		runAuthTest(StringView(authUrl), StringView(authUser), StringView(authPass));
	}

	sprt::cout << "\ntotal failures: " << s_failures << "\n";
	return s_failures;
}

} // namespace stappler::git

int main(int argc, const char *argv[]) {
	using namespace stappler;
	return perform_main(argc, argv, [&]() -> int { return git::run(argc, argv); });
}
