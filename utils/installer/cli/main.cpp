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

// Headless CLI front-end over the C++ installer core (utils/installer/core) — the C++ analogue of
// the Rust xenolith-cli. Parity commands: detect, paths, state, verify, list, install, new, build.

#include "SPICommon.h"
#include "SPIDirs.h"
#include "SPITriple.h"
#include "SPIManifest.h"
#include "SPIState.h"
#include "SPITransport.h"
#include "SPICatalogue.h"
#include "SPIEngineSource.h"
#include "SPIInstall.h"
#include "SPIScaffold.h"
#include "SPIBuild.h"

#include <sprt/runtime/platform.h>

#include <iostream>
#include <unistd.h>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <memory>

using namespace STAPPLER_VERSIONIZED stappler::xenolith::installer;
using namespace stappler;	   // makes `git::` (stappler::git) usable
using namespace sprt::status; // Status

// --- arg parsing helpers ----------------------------------------------------

struct Args {
	Vector<String> positional;
	String engine; // --engine <path> ("" = none)
	bool host = false;
	bool target = false;
	bool run = false;
	bool release = false;
	String optTarget; // --target <triple>
};

static bool eq(StringView a, const char *b) { return a == StringView(b); }

static Args parse_args(int argc, const char **argv, int first) {
	Args a;
	for (int i = first; i < argc; ++i) {
		StringView arg(argv[i]);
		if (eq(arg, "--host")) {
			a.host = true;
		} else if (eq(arg, "--target")) {
			if (i + 1 < argc && argv[i + 1][0] != '-' && a.optTarget.empty()) {
				// `--target <triple>` (build) — but only if next isn't a flag; ambiguous with the
				// install `--target` flag. Disambiguate by command context at the call site.
				a.optTarget = toString(argv[++i]);
			} else {
				a.target = true;
			}
		} else if (eq(arg, "--engine") && i + 1 < argc) {
			a.engine = toString(argv[++i]);
		} else if (eq(arg, "--run")) {
			a.run = true;
		} else if (eq(arg, "--release")) {
			a.release = true;
		} else if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
			// unknown long option → ignore
		} else {
			a.positional.push_back(toString(argv[i]));
		}
	}
	return a;
}

static const String *engine_override(const Args &a) {
	return a.engine.empty() ? nullptr : &a.engine;
}

// A coarse download-progress reporter (512 KiB steps), matching the Rust CLI's cadence.
static Function<void(int64_t, int64_t)> make_progress() {
	auto last = std::make_shared<uint64_t>(UINT64_MAX);
	return [last](int64_t bytes, int64_t total) {
		uint64_t step = static_cast<uint64_t>(bytes) / (512 * 1024);
		if (step == *last) {
			return;
		}
		*last = step;
		if (total > 0) {
			fprintf(stderr, "\r    %3.0f%%  (%llu / %llu MB)   ", bytes * 100.0 / total,
					(unsigned long long)bytes / 1000000, (unsigned long long)total / 1000000);
		} else {
			fprintf(stderr, "\r    %llu MB   ", (unsigned long long)bytes / 1000000);
		}
	};
}

// --- commands ---------------------------------------------------------------

static int cmdDetect() {
	auto h = resolve_host(native_arch(), native_os());
	if (!h.native.empty()) {
		std::cout << h.native.data();
		if (h.viaEmulation) {
			std::cout << " (host via " << h.hostArchive.data() << ")";
		}
		std::cout << "\n";
		return 0;
	}
	std::cout << "no SDK host available for " << toString(native_arch()).data() << "-"
			  << toString(native_os()).data() << "\n";
	return 1;
}

static int cmdPaths() {
	auto l = Layout::resolve_from_env(nullptr);
	std::cout << "config:     " << l.config.data() << "\n"
			  << "data:       " << l.data.data() << "\n"
			  << "cache:      " << l.cache.data() << "\n"
			  << "toolchains: " << l.toolchains_store_dir().data() << "\n"
			  << "engines:    " << l.engines_dir().data() << "\n";
	return 0;
}

static int cmdState() {
	auto layout = Layout::resolve_from_env(nullptr);
	auto st = InstalledState::load(layout.installed_manifest());
	std::cout << "schema: " << st.schema << ", components: " << st.components.size() << "\n";
	for (const auto &c : st.components) {
		std::cout << "  " << toString(kind_to_string(c.kind)).data() << "  " << c.id.data()
				  << "  release=" << c.release.data() << "  path=" << c.path.data() << "\n";
	}
	return 0;
}

static int cmdVerify() {
	auto layout = Layout::resolve_from_env(nullptr);
	auto st = InstalledState::load(layout.installed_manifest());
	auto bad = st.invalid([](StringView p) { return ::access(p.data(), F_OK) == 0; });
	if (bad.empty()) {
		std::cout << st.components.size() << " components OK\n";
		return 0;
	}
	std::cout << "INVALID:\n";
	for (auto *c : bad) {
		std::cout << "  " << toString(kind_to_string(c->kind)).data() << "  " << c->id.data()
				  << "  missing: " << c->path.data() << "\n";
	}
	return 1;
}

static int cmdFetch(int argc, const char **argv) {
	if (argc < 3) {
		std::cerr << "usage: xenolith-cli fetch <url>\n";
		return 2;
	}
	String text;
	auto r = fetch_text(toString(argv[2]), text);
	if (r.status != Status::Ok) {
		std::cerr << "error: " << r.error.data() << " (code " << r.responseCode << ")\n";
		return 1;
	}
	std::cout << text.data();
	return 0;
}

static int cmdList() {
	auto base = ftp_release_base();
	std::cerr << "Fetching catalogue from " << base.data() << " ...\n";

	String hostsText, targetsText;
	auto r1 = fetch_text(toString(base) + "/hosts/", hostsText);
	if (r1.status != Status::Ok) {
		std::cerr << "hosts: " << r1.error.data() << "\n";
		return 1;
	}
	auto r2 = fetch_text(toString(base) + "/targets/", targetsText);
	if (r2.status != Status::Ok) {
		std::cerr << "targets: " << r2.error.data() << "\n";
		return 1;
	}

	auto comps = build_catalogue(hostsText, targetsText);
	auto layout = Layout::resolve_from_env(nullptr);
	auto st = InstalledState::load(layout.installed_manifest());

	std::cout << "targets:\n";
	for (const auto &c : comps) {
		if (c.kind == Kind::Target) {
			bool installed = st.get(c.id, c.kind) != nullptr;
			std::cout << (installed ? "  [x] " : "  [ ] ") << c.id.data() << "  ("
					  << (c.size / 1000000) << " MB)\n";
		}
	}
	std::cout << "hosts:\n";
	for (const auto &c : comps) {
		if (c.kind == Kind::Host) {
			bool installed = st.get(c.id, c.kind) != nullptr;
			std::cout << (installed ? "  [x] " : "  [ ] ") << c.id.data() << "  ("
					  << (c.size / 1000000) << " MB)\n";
		}
	}
	std::cout << comps.size() << " components\n";
	return 0;
}

static int cmdEngineRefs() {
	String status;
	auto refs = list_engine_refs(&status);
	if (!status.empty()) {
		std::cerr << "error: " << status.data() << "\n";
		return 1;
	}
	std::cout << "branches:\n";
	for (auto &r : refs) {
		if (r.isBranch) {
			std::cout << "  " << r.name.data();
			if (r.name == engineDefaultRef()) {
				std::cout << " (default)";
			}
			std::cout << "\n";
		}
	}
	std::cout << "tags:\n";
	for (auto &r : refs) {
		if (r.isTag) {
			std::cout << "  " << r.name.data() << "\n";
		}
	}
	return 0;
}

static int cmdEngineInstall(int argc, const char **argv) {
	String ref = (argc > 2 && argv[2][0]) ? toString(argv[2]) : toString(engineDefaultRef());
	auto layout = Layout::resolve_from_env(nullptr);
	auto target = layout.engine_dir(ref);

	std::cerr << "Cloning engine '" << ref.data() << "' -> " << target.data()
			  << " (shallow, with submodules)...\n";

	git::CloneStage lastStage = static_cast<git::CloneStage>(-1);
	auto r = clone_engine(ref, layout, [&lastStage](const git::CloneProgress &p) {
		if (p.stage != lastStage) {
			lastStage = p.stage;
			const char *s = "?";
			switch (p.stage) {
			case git::CloneStage::Connecting: s = "connecting"; break;
			case git::CloneStage::Downloading: s = "downloading"; break;
			case git::CloneStage::Unpacking: s = "unpacking"; break;
			case git::CloneStage::CheckingOut: s = "checking out"; break;
			}
			std::cerr << "\n  " << s;
			if (p.submoduleDepth > 0) {
				std::cerr << " (submodule x" << p.submoduleDepth << ")";
			}
		}
		if (p.stage == git::CloneStage::Downloading && p.bytesTotal > 0) {
			std::cerr << "\r    " << (p.bytesReceived * 100 / p.bytesTotal) << "% ("
					  << (p.bytesReceived / 1000000) << "/" << (p.bytesTotal / 1000000)
					  << " MB)   " << std::flush;
		}
	});

	if (r.status != Status::Ok) {
		std::cerr << "\nfailed (status " << static_cast<int>(r.status) << ")\n";
		return 1;
	}
	std::cerr << "\ndone: " << r.filesWritten << " files, ~" << (r.bytesWritten / 1000000)
			  << " MB, commit " << r.commitHex.data() << ", submodules " << r.submodulesCloned
			  << "\n";
	return 0;
}

// `install engine`: ensure the engine is present (clone the default ref if no engine is resolvable),
// then link the toolchain store into it. Returns the engine label.
static int install_engine(const Args &a) {
	auto layout = Layout::resolve_from_env(nullptr);
	bool ok = false;
	auto root = resolve_engine_root(layout, engine_override(a), &ok);
	if (!ok) {
		std::cerr << "• Engine (cloning " << toString(engineDefaultRef()).data() << ")\n";
		auto cr = clone_engine(engineDefaultRef(), layout, [](const git::CloneProgress &p) {
			if (p.stage == git::CloneStage::Downloading && p.bytesTotal > 0) {
				fprintf(stderr, "\r    %llu%% (%llu/%llu MB)   ",
						(unsigned long long)p.bytesReceived * 100 / p.bytesTotal,
						(unsigned long long)p.bytesReceived / 1000000,
						(unsigned long long)p.bytesTotal / 1000000);
			}
		});
		if (cr.status != Status::Ok) {
			std::cerr << "\nengine clone failed (status " << static_cast<int>(cr.status) << ")\n";
			return 1;
		}
		root = layout.engine_dir(engineDefaultRef());
		std::cerr << "\r    ✓ engine " << cr.commitHex.data() << "                    \n";
	} else {
		std::cerr << "• Engine (" << root.data() << ")\n";
	}
	link_toolchains_into_engine_path(layout, StringView(root));
	std::cout << "engine ready at " << root.data()
			  << "\nAdd toolchains with `install <triple>`, or `install` to provision everything.\n";
	return 0;
}

// Run one install_component and print progress/ok.
static int install_one(const Layout &layout, StringView id, bool wantHost, bool wantTarget,
		const char *label) {
	std::cerr << "• " << label << ": " << toString(id).data() << "\n";
	auto r = install_component(id, layout, wantHost, wantTarget, make_progress());
	if (r.status != Status::Ok || r.installed.empty()) {
		std::cerr << "\nerror: " << r.error.data() << "\n";
		return 1;
	}
	for (const auto &o : r.installed) {
		std::cerr << "\r    ✓ " << o.id.data() << "                         \n";
	}
	return 0;
}

static int cmdInstall(int argc, const char **argv) {
	Args a = parse_args(argc, argv, 2);

	if (!a.positional.empty() && a.positional[0] == toString("engine") && !a.host && !a.target) {
		return install_engine(a);
	}

	auto layout = Layout::resolve_from_env(nullptr);

	if (a.positional.empty()) {
		// `install` with no id → provision the whole SDK: engine + native host + native target (+sprt)
		auto h = resolve_host(native_arch(), native_os());
		if (h.native.empty()) {
			std::cerr << "no SDK host for " << toString(native_arch()).data() << "-"
					  << toString(native_os()).data() << "\n";
			return 1;
		}
		if (int e = install_engine(a); e != 0) {
			return e;
		}
		if (int e = install_one(layout, h.native, true, false, "host toolchain"); e != 0) {
			return e;
		}
		if (int e = install_one(layout, h.native, false, true, "target"); e != 0) {
			return e;
		}
		String sprt = h.native + "+sprt";
		// Only install the +sprt target if it is present in the catalogue; ignore "not found".
		auto r = install_component(sprt, layout, false, true, make_progress());
		if (r.status == Status::Ok) {
			std::cerr << "\r    ✓ " << sprt.data() << "                         \n";
		}
		std::cout << "SDK ready for " << h.native.data()
				  << "\nNext: `xenolith-cli new <name>`, then `build <name> --run`.\n";
		return 0;
	}

	// `install <id>`
	const auto &id = a.positional[0];
	if (int e = install_one(layout, StringView(id), a.host, a.target, "component"); e != 0) {
		return e;
	}
	std::cout << "Installed " << id.data() << "\n";
	return 0;
}

static int cmdNew(int argc, const char **argv) {
	Args a = parse_args(argc, argv, 2);
	if (a.positional.empty()) {
		std::cerr << "usage: xenolith-cli new <name> [location]\n";
		return 2;
	}
	auto name = a.positional[0];
	String location = (a.positional.size() > 1) ? a.positional[1] : toString(".");

	auto layout = Layout::resolve_from_env(nullptr);
	auto r = scaffold_project(name, location, layout, engine_override(a));
	if (r.status != Status::Ok) {
		std::cerr << "error: " << r.error.data() << "\n";
		return 1;
	}
	std::cout << "created project at " << r.path.data()
			  << "\nBuild it: xenolith-cli build " << r.path.data() << " --run\n";
	return 0;
}

static int cmdBuild(int argc, const char **argv) {
	Args a = parse_args(argc, argv, 2);
	String path = a.positional.empty() ? toString(".") : a.positional[0];

	auto layout = Layout::resolve_from_env(nullptr);
	BuildOptions opts;
	opts.target = a.optTarget;
	opts.run = a.run;
	opts.release = a.release;

	auto r = build_project(path, layout, opts, engine_override(a));
	if (r.status != Status::Ok) {
		std::cerr << "error: " << r.error.data() << "\n";
		return 1;
	}
	std::cout << r.message.data() << "\n";
	return 0;
}

int main(int argc, const char **argv) {
	// The CLI doesn't run the xenolith app framework, so bring up the runtime platform explicitly
	// (filesystem writable-root registration, default FS interface). Without it,
	// filesystem::mkdir_recursive/write used by the git checkout resolve to nothing.
	int initRc = 0;
	sprt::initialize(sprt::AppConfig{StringView("org.stappler.xenolith.cli"),
							StringView("xenolith-cli"), StringView()},
			initRc);

	auto prog = argc > 0 ? argv[0] : "xenolith-cli";
	String cmd = argc > 1 ? toString(argv[1]) : String();

	if (cmd == "detect") {
		return cmdDetect();
	}
	if (cmd == "paths") {
		return cmdPaths();
	}
	if (cmd == "state") {
		return cmdState();
	}
	if (cmd == "verify") {
		return cmdVerify();
	}
	if (cmd == "fetch") {
		return cmdFetch(argc, argv);
	}
	if (cmd == "list") {
		return cmdList();
	}
	if (cmd == "install") {
		return cmdInstall(argc, argv);
	}
	if (cmd == "new") {
		return cmdNew(argc, argv);
	}
	if (cmd == "build") {
		return cmdBuild(argc, argv);
	}
	if (cmd == "engine-refs") {
		return cmdEngineRefs();
	}
	if (cmd == "engine-install") {
		return cmdEngineInstall(argc, argv);
	}

	std::cerr << "Xenolith SDK installer (C++ CLI)\n";
	std::cerr << "Usage: " << prog << " <command> [args]\n\n";
	std::cerr << "Commands:\n";
	std::cerr << "  detect        Print the detected native host triple\n";
	std::cerr << "  paths         Print the resolved install directories\n";
	std::cerr << "  state         Show the installed-state registry (installed.json)\n";
	std::cerr << "  verify        Validate installed components against the filesystem\n";
	std::cerr << "  list          List the toolchain catalogue with install status\n";
	std::cerr << "  install [id]  Install a component (<id>, `engine`, or nothing = provision all)\n";
	std::cerr << "                flags: --host, --target, --engine <path>\n";
	std::cerr << "  new <name> [location]   Scaffold a new buildable project\n";
	std::cerr << "  build [path]            Build a project (flags: --target, --run, --release, --engine)\n";
	std::cerr << "  fetch <url>   Fetch a text resource (FTP or HTTPS) — raw transport test\n";
	std::cerr << "  engine-refs   List the engine git branches and tags\n";
	std::cerr << "  engine-install [ref]  Clone the engine (default: master, with submodules)\n";
	return cmd.empty() ? 0 : 2;
}
