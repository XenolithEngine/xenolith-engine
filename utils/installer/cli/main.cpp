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

// Headless CLI front-end over the installer core (utils/installer/core), sharing every operation
// with the GUI. Commands: detect, paths, config, state, verify, list, fetch, install, new, build,
// engine-refs, engine-install.

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
#include "SPISettings.h"

#include "SPCommandLineParser.h"

#include <sprt/runtime/platform.h>
#include <sprt/runtime/stream.h> // sprt::cout / sprt::cerr

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// --- command line -----------------------------------------------------------

struct CliArgs {
	Vector<String> positional;
	String engine; // --engine <path>
	String target; // build: --target <triple>
	bool wantHost = false; // install: --host
	bool wantTarget = false; // install: --target
	bool run = false;
	bool release = false;
	uint32_t jobs = 0;
};

// `--target` means two different things, so each command gets its own parser: for `install` it
// selects a component kind, for `build` it names a triple. One parser per command is what keeps
// those apart without guessing.
using CliOption = CommandLineOption<CliArgs>;

static CommandLineParser<CliArgs> getEngineParser() {
	return CommandLineParser<CliArgs>({
		CliOption{.patterns = {"--engine <path>"},
			.description = StringView("Use the engine checkout at <path> as STAPPLER_ROOT"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.engine = toString(args[0]);
		return true;
	}},
	});
}

static CommandLineParser<CliArgs> getInstallParser() {
	auto parser = getEngineParser();
	parser.add({
		CliOption{.patterns = {"--host"},
			.description = StringView("Install only the host toolchain for the given id"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.wantHost = true;
		return true;
	}},
		CliOption{.patterns = {"--target"},
			.description = StringView("Install only the target sysroot for the given id"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.wantTarget = true;
		return true;
	}},
	});
	return parser;
}

static CommandLineParser<CliArgs> getBuildParser() {
	auto parser = getEngineParser();
	parser.add({
		CliOption{.patterns = {"--target <triple>"},
			.description = StringView("Build for <triple> instead of the native host"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.target = toString(args[0]);
		return true;
	}},
		CliOption{.patterns = {"--run"},
			.description = StringView("Run the freshly-built binary (native builds only)"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.run = true;
		return true;
	}},
		CliOption{.patterns = {"--release"},
			.description = StringView("Build the release configuration"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.release = true;
		return true;
	}},
		CliOption{.patterns = {"-j<#>", "--jobs <#>"},
			.description = StringView("Parallel jobs (default: hardware concurrency)"),
			.callback = [](CliArgs &target, StringView pattern, SpanView<StringView> args) -> bool {
		target.jobs = uint32_t(StringView(args[0]).readInteger(10).get(0));
		return true;
	}},
	});
	return parser;
}

// Parse the arguments that follow `<prog> <command>`.
static bool parseArgs(const CommandLineParser<CliArgs> &parser, CliArgs &args, int argc,
		const char *argv[]) {
	if (argc <= 2) {
		return true; // command with no arguments
	}
	return parser.parse(args, argc - 2, argv + 2,
			Callback<void(CliArgs &, StringView)>([](CliArgs &target, StringView arg) {
		target.positional.emplace_back(toString(arg));
	}));
}

// --- progress reporting -----------------------------------------------------

// A coarse download-progress reporter (512 KiB steps).
static Function<void(int64_t, int64_t)> makeProgressReporter(uint64_t &lastStep) {
	return [&lastStep](int64_t bytes, int64_t total) {
		auto step = uint64_t(bytes) / (512_KiB);
		if (step == lastStep) {
			return;
		}
		lastStep = step;
		if (total > 0) {
			sprt::cerr << "\r    " << (bytes * 100 / total) << "%  (" << (bytes / 1'000'000)
					   << " / " << (total / 1'000'000) << " MB)   ";
		} else {
			sprt::cerr << "\r    " << (bytes / 1'000'000) << " MB   ";
		}
	};
}

/* Everything a command needs to know about this machine: where the store is, and what the user
configured about it.

The two are resolved TOGETHER because they are not independent - the settings file is found through
the layout's config dir, and two of its fields (enginePath, toolchainsPath) then reshape that same
layout. Splitting them is how a command ends up honouring the mirror the user chose and ignoring the
engine path they chose, in the same run.

Read fresh from <config>/settings.json per command rather than cached in a global: the CLI is a
short-lived process, the file is tiny, and a global would mean every entry point silently depends on
"settings were loaded first". A missing file yields the built-in defaults, which is what the CLI did
before preferences existed.

This is the whole reason the settings store lives in installer_core and not in the GUI: a user who
points the installer at a mirror, or at their own engine checkout, must reach the same one from
`xenolith-cli list` and `xenolith-cli build`. */
struct CliEnv {
	Layout layout;
	Settings settings;
};

static CliEnv getEnv() {
	CliEnv env;
	env.layout = Layout::resolveFromEnv();
	env.settings = Settings::load(env.layout.getSettingsManifest());
	env.settings.applyTo(env.layout);
	return env;
}

// --- commands ---------------------------------------------------------------

static int cmdDetect() {
	auto h = resolveHost(getNativeArch(), getNativeOs());
	if (!h.native.empty()) {
		sprt::cout << h.native;
		if (h.viaEmulation) {
			sprt::cout << " (host via " << h.hostArchive << ")";
		}
		sprt::cout << "\n";
		return 0;
	}
	sprt::cout << "no SDK host available for " << getNativeArch() << "-" << getNativeOs() << "\n";
	return 1;
}

static int cmdPaths() {
	auto env = getEnv();
	const auto &l = env.layout;

	// `toolchains` and `engine` are the two a configured path can move, so each says where it came
	// from — "the store is not where I expected" is otherwise a silent wrong answer.
	bool engineOk = false;
	auto engineRoot = resolveEngineRoot(l, StringView(), &engineOk);

	sprt::cout << "config:     " << l.config << "\n"
			   << "data:       " << l.data << "\n"
			   << "cache:      " << l.cache << "\n"
			   << "toolchains: " << l.getToolchainsDir()
			   << (l.toolchains.empty() ? "" : "  (configured)") << "\n"
			   << "engines:    " << l.getEnginesDir() << "\n"
			   << "engine:     " << engineRoot << (engineOk ? "" : "  (not a valid engine root)")
			   << "\n";
	return 0;
}

// --- config -----------------------------------------------------------------

/* What the tools would actually USE for `key` right now, which is not always what is stored: an
empty field means "the built-in default", $XENOLITH_ENGINE outranks the stored engine path, and a
release root is normalized. Printed next to the stored value so "(default)" is never a dead end -
the point of `config` is to answer "which mirror/engine/store am I on", and a blank line does not. */
static String getEffectiveSetting(const CliEnv &env, StringView key) {
	if (key == "engineRepoUrl") {
		return env.settings.sources.getEngineRepoUrl();
	} else if (key == "releaseSourceUrl") {
		return env.settings.sources.getReleasesRoot();
	} else if (key == "enginePath") {
		bool ok = false;
		auto root = resolveEngineRoot(env.layout, StringView(), &ok);
		return ok ? root : toString(root, " (not a valid engine root)");
	} else if (key == "toolchainsPath") {
		return env.layout.getToolchainsDir();
	} else if (key == "lang") {
		return toString("en");
	}
	return String();
}

static void printSetting(const CliEnv &env, const SettingsField &field, bool verbose) {
	constexpr size_t kKeyColumn = 20;

	const Value stored = env.settings.getFieldValue(field.key);
	sprt::cout << "  " << field.key;
	for (size_t i = field.key.size(); i < kKeyColumn; ++i) { sprt::cout << " "; }

	if (field.boolean) {
		sprt::cout << (stored.getBool() ? "true" : "false") << "\n";
	} else if (stored.getString().empty()) {
		// The resolved value in parentheses: it is a fact about this machine, not something the user
		// chose, and writing it back would turn a default that tracks the tool into a pinned one.
		auto effective = getEffectiveSetting(env, field.key);
		sprt::cout << "(default";
		if (!effective.empty()) {
			sprt::cout << ": " << effective;
		}
		sprt::cout << ")\n";
	} else {
		sprt::cout << stored.getString();
		// The stored value is not always the one that wins - $XENOLITH_ENGINE beats enginePath, and
		// a release root gets a trailing slash - so say so when the two differ.
		auto effective = getEffectiveSetting(env, field.key);
		if (!effective.empty() && effective != stored.getString()) {
			sprt::cout << "  (in effect: " << effective << ")";
		}
		sprt::cout << "\n";
	}

	if (verbose) {
		sprt::cout << "      " << field.description << "\n";
	}
}

// Booleans are typed, not clicked, so accept every spelling a user reasonably reaches for.
static bool parseSettingsBool(StringView value, bool &out) {
	if (value == "true" || value == "1" || value == "yes" || value == "on") {
		out = true;
		return true;
	}
	if (value == "false" || value == "0" || value == "no" || value == "off") {
		out = false;
		return true;
	}
	return false;
}

/* `config` — read and write <config>/settings.json, the file the GUI's settings form writes.

Both front ends go through Settings::setFieldValue, so there is exactly one key→field mapping and a
key cannot become settable from one tool and invisible from the other. */
static int cmdConfig(int argc, const char *argv[]) {
	auto env = getEnv();

	if (argc <= 2) {
		sprt::cout << "settings: " << env.layout.getSettingsManifest() << "\n\n";
		for (const auto &field : Settings::getFields()) { printSetting(env, field, false); }
		sprt::cout
				<< "\nSet one with `config <key> <value>`, reset it with `config <key> --unset`.\n";
		return 0;
	}

	auto key = StringView(argv[2]);
	const auto *field = Settings::getField(key);
	if (!field) {
		sprt::cerr << "unknown setting '" << key << "'. Known settings:\n";
		for (const auto &f : Settings::getFields()) {
			sprt::cerr << "  " << f.key << " — " << f.description << "\n";
		}
		return 2;
	}

	if (argc == 3) {
		printSetting(env, *field, true);
		return 0;
	}

	auto raw = StringView(argv[3]);
	Value value;
	if (field->boolean) {
		bool parsed = false;
		if (!parseSettingsBool(raw, parsed)) {
			sprt::cerr << "'" << key << "' is a switch: expected true/false, got '" << raw << "'\n";
			return 2;
		}
		value = Value(parsed);
	} else if (raw == "--unset" || raw == "-") {
		value = Value(String()); // empty string == "back to the built-in default"
	} else {
		value = Value(toString(raw));
	}

	// setFieldValue cannot fail here - the key came out of getField above - but the write can, and
	// silently reporting success for a settings file that never landed is the one outcome that would
	// make this command untrustworthy.
	env.settings.setFieldValue(key, value);
	if (!env.settings.save(env.layout.getSettingsManifest())) {
		sprt::cerr << "failed to write " << env.layout.getSettingsManifest() << "\n";
		return 1;
	}

	// Re-resolve before printing: the value just written may itself have moved the store or the
	// engine root, and echoing the pre-write resolution would be a lie about what the next command
	// will do.
	env = getEnv();
	printSetting(env, *field, false);
	return 0;
}

// --- commands (continued) ---------------------------------------------------

static int cmdState() {
	auto layout = getEnv().layout;
	auto st = InstalledState::load(layout.getInstalledManifest());
	sprt::cout << "schema: " << st.schema << ", components: " << st.components.size() << "\n";
	for (const auto &c : st.components) {
		sprt::cout << "  " << getKindName(c.kind) << "  " << c.id << "  release=" << c.release
				   << "  path=" << c.path << "\n";
	}
	return 0;
}

static int cmdVerify() {
	auto layout = getEnv().layout;
	auto st = InstalledState::load(layout.getInstalledManifest());
	auto bad = st.getInvalid([](StringView p) { return filesystem::exists(FileInfo(p)); });
	if (bad.empty()) {
		sprt::cout << st.components.size() << " components OK\n";
		return 0;
	}
	sprt::cout << "INVALID:\n";
	for (auto *c : bad) {
		sprt::cout << "  " << getKindName(c->kind) << "  " << c->id << "  missing: " << c->path
				   << "\n";
	}
	return 1;
}

static int cmdFetch(int argc, const char *argv[]) {
	if (argc < 3) {
		sprt::cerr << "usage: xenolith-cli fetch <url>\n";
		return 2;
	}
	String text;
	auto r = fetchText(StringView(argv[2]), text);
	if (!r) {
		sprt::cerr << "error: " << r.error << " (code " << r.responseCode << ")\n";
		return 1;
	}
	sprt::cout << text;
	return 0;
}

static int cmdList() {
	auto env = getEnv();
	const auto &layout = env.layout;
	auto base = env.settings.sources.getReleaseBase(StringView());
	sprt::cerr << "Fetching catalogue from " << base << " ...\n";

	// URLs, not paths: the trailing slash is what makes the FTP server list a directory.
	String hostsText, targetsText;
	auto r1 = fetchText(toString(base, "/hosts/"), hostsText);
	if (!r1) {
		sprt::cerr << "hosts: " << r1.error << "\n";
		return 1;
	}
	auto r2 = fetchText(toString(base, "/targets/"), targetsText);
	if (!r2) {
		sprt::cerr << "targets: " << r2.error << "\n";
		return 1;
	}

	auto comps = buildCatalogue(hostsText, targetsText);
	auto st = InstalledState::load(layout.getInstalledManifest());

	auto printKind = [&](Kind kind) {
		for (const auto &c : comps) {
			if (c.kind == kind) {
				sprt::cout << (st.get(c.id, c.kind) ? "  [x] " : "  [ ] ") << c.id << "  ("
						   << (c.size / 1'000'000) << " MB)\n";
			}
		}
	};

	sprt::cout << "targets:\n";
	printKind(Kind::Target);
	sprt::cout << "hosts:\n";
	printKind(Kind::Host);
	sprt::cout << comps.size() << " components\n";
	return 0;
}

static int cmdEngineRefs() {
	OperationResult status;
	auto refs = listEngineRefs(getEnv().settings.sources, &status);
	if (!status) {
		sprt::cerr << "error: " << status.error << "\n";
		return 1;
	}
	sprt::cout << "branches:\n";
	for (auto &r : refs) {
		if (r.isBranch) {
			sprt::cout << "  " << r.name;
			if (r.name == getEngineDefaultRef()) {
				sprt::cout << " (default)";
			}
			sprt::cout << "\n";
		}
	}
	sprt::cout << "tags:\n";
	for (auto &r : refs) {
		if (r.isTag) {
			sprt::cout << "  " << r.name << "\n";
		}
	}
	return 0;
}

// Report clone progress on one line per stage.
static EngineProgressCallback makeCloneReporter(git::CloneStage &lastStage) {
	return [&lastStage](const git::CloneProgress &p) {
		if (p.stage != lastStage) {
			lastStage = p.stage;
			StringView name = "?";
			switch (p.stage) {
			case git::CloneStage::Connecting: name = "connecting"; break;
			case git::CloneStage::Downloading: name = "downloading"; break;
			case git::CloneStage::Unpacking: name = "unpacking"; break;
			case git::CloneStage::CheckingOut: name = "checking out"; break;
			}
			sprt::cerr << "\n  " << name;
			if (p.submoduleDepth > 0) {
				sprt::cerr << " (submodule x" << p.submoduleDepth << ")";
			}
		}
		if (p.stage == git::CloneStage::Downloading && p.bytesTotal > 0) {
			sprt::cerr << "\r    " << (p.bytesReceived * 100 / p.bytesTotal) << "% ("
					   << (p.bytesReceived / 1'000'000) << "/" << (p.bytesTotal / 1'000'000)
					   << " MB)   ";
		}
	};
}

static int cmdEngineInstall(int argc, const char *argv[]) {
	String ref = (argc > 2 && argv[2][0]) ? toString(argv[2]) : toString(getEngineDefaultRef());
	auto env = getEnv();
	const auto &layout = env.layout;

	sprt::cerr << "Cloning engine '" << ref << "' -> " << layout.getEngineDir(ref)
			   << " (shallow, with submodules)...\n";

	auto lastStage = static_cast<git::CloneStage>(-1);
	auto r = cloneEngine(env.settings.sources, ref, layout, makeCloneReporter(lastStage));
	if (!r) {
		sprt::cerr << "\nfailed: " << r.error << "\n";
		return 1;
	}
	sprt::cerr << "\ndone: " << r.filesWritten << " files, ~" << (r.bytesWritten / 1'000'000)
			   << " MB, commit " << r.commitHex << ", submodules " << r.submodulesCloned << "\n";
	return 0;
}

// `install engine`: ensure the engine is present (clone the default ref if none is resolvable),
// then link the toolchain store into it.
static int installEngine(const CliEnv &env, const CliArgs &args) {
	const auto &layout = env.layout;
	bool ok = false;
	auto root = resolveEngineRoot(layout, args.engine, &ok);
	if (!ok) {
		sprt::cerr << "• Engine (cloning " << getEngineDefaultRef() << ")\n";

		auto lastStage = static_cast<git::CloneStage>(-1);
		auto cr = cloneEngine(env.settings.sources, getEngineDefaultRef(), layout,
				makeCloneReporter(lastStage));
		if (!cr) {
			sprt::cerr << "\nengine clone failed: " << cr.error << "\n";
			return 1;
		}
		root = layout.getEngineDir(getEngineDefaultRef());
		sprt::cerr << "\r    ✓ engine " << cr.commitHex << "                    \n";
	} else {
		sprt::cerr << "• Engine (" << root << ")\n";
	}

	linkToolchainsIntoEnginePath(layout, StringView(root));
	sprt::cout << "engine ready at " << root
			   << "\nAdd toolchains with `install <triple>`, or `install` to provision "
				  "everything.\n";
	return 0;
}

// Run one installComponent and report progress/completion.
static int installOne(const CliEnv &env, StringView id, bool wantHost, bool wantTarget,
		StringView label) {
	sprt::cerr << "• " << label << ": " << id << "\n";

	uint64_t lastStep = maxOf<uint64_t>();
	auto r = installComponent(env.settings.sources, StringView(), id, env.layout, wantHost,
			wantTarget, makeProgressReporter(lastStep));
	if (!r || r.installed.empty()) {
		sprt::cerr << "\nerror: " << r.error << "\n";
		return 1;
	}
	for (const auto &o : r.installed) {
		sprt::cerr << "\r    ✓ " << o.id << "                         \n";
	}
	return 0;
}

static int cmdInstall(int argc, const char *argv[]) {
	CliArgs args;
	if (!parseArgs(getInstallParser(), args, argc, argv)) {
		return 2;
	}

	auto env = getEnv();

	if (!args.positional.empty() && args.positional[0] == "engine" && !args.wantHost
			&& !args.wantTarget) {
		return installEngine(env, args);
	}

	if (args.positional.empty()) {
		// `install` with no id → provision the whole SDK: engine + native host + native target
		auto h = resolveHost(getNativeArch(), getNativeOs());
		if (h.native.empty()) {
			sprt::cerr << "no SDK host for " << getNativeArch() << "-" << getNativeOs() << "\n";
			return 1;
		}
		if (int e = installEngine(env, args); e != 0) {
			return e;
		}
		if (int e = installOne(env, h.native, true, false, "host toolchain"); e != 0) {
			return e;
		}
		if (int e = installOne(env, h.native, false, true, "target"); e != 0) {
			return e;
		}

		// The +sprt target is optional — install it only when the catalogue has one.
		auto sprtTarget = toString(h.native, "+sprt");
		uint64_t lastStep = maxOf<uint64_t>();
		auto r = installComponent(env.settings.sources, StringView(), sprtTarget, env.layout, false,
				true, makeProgressReporter(lastStep));
		if (r) {
			sprt::cerr << "\r    ✓ " << sprtTarget << "                         \n";
		}

		sprt::cout << "SDK ready for " << h.native
				   << "\nNext: `xenolith-cli new <name>`, then `build <name> --run`.\n";
		return 0;
	}

	const auto &id = args.positional[0];
	if (int e = installOne(env, id, args.wantHost, args.wantTarget, "component"); e != 0) {
		return e;
	}
	sprt::cout << "Installed " << id << "\n";
	return 0;
}

static int cmdNew(int argc, const char *argv[]) {
	CliArgs args;
	if (!parseArgs(getEngineParser(), args, argc, argv)) {
		return 2;
	}
	if (args.positional.empty()) {
		sprt::cerr << "usage: xenolith-cli new <name> [location]\n";
		return 2;
	}

	auto layout = getEnv().layout;
	auto location = (args.positional.size() > 1) ? StringView(args.positional[1]) : StringView(".");

	auto r = scaffoldProject(args.positional[0], location, layout, args.engine);
	if (!r) {
		sprt::cerr << "error: " << r.error << "\n";
		return 1;
	}
	sprt::cout << "created project at " << r.path << "\nBuild it: xenolith-cli build " << r.path
			   << " --run\n";
	return 0;
}

static int cmdBuild(int argc, const char *argv[]) {
	CliArgs args;
	if (!parseArgs(getBuildParser(), args, argc, argv)) {
		return 2;
	}

	auto layout = getEnv().layout;
	BuildOptions opts;
	opts.target = args.target;
	opts.run = args.run;
	opts.release = args.release;
	opts.jobs = args.jobs;

	// The build streams its output (progress, compiler diagnostics, the child's own output) from
	// the job thread; the calling thread stays blocked, so there is nothing to interleave with.
	Callback<void(StringView)> sink([](StringView chunk) { sprt::cout << chunk; });

	auto path = args.positional.empty() ? StringView(".") : StringView(args.positional[0]);
	auto r = buildProject(path, layout, opts, args.engine, &sink);
	if (!r) {
		sprt::cerr << "error: " << r.error << "\n";
		return 1;
	}
	sprt::cout << r.message << "\n";
	return r.runExitCode > 0 ? r.runExitCode : 0;
}

static void printUsage(StringView prog) {
	sprt::cerr << "Xenolith SDK installer (CLI)\n";
	sprt::cerr << "Usage: " << prog << " <command> [args]\n\n";
	sprt::cerr << "Commands:\n";
	sprt::cerr << "  detect        Print the detected native host triple\n";
	sprt::cerr << "  paths         Print the resolved install directories\n";
	sprt::cerr << "  config [key [value]]    Show or change the stored settings\n";
	sprt::cerr << "  state         Show the installed-state registry (installed.json)\n";
	sprt::cerr << "  verify        Validate installed components against the filesystem\n";
	sprt::cerr << "  list          List the toolchain catalogue with install status\n";
	sprt::cerr << "  install [id]  Install a component (<id>, `engine`, or nothing = provision "
				  "all)\n";
	sprt::cerr << "  new <name> [location]   Scaffold a new buildable project\n";
	sprt::cerr << "  build [path]            Build a project\n";
	sprt::cerr << "  fetch <url>   Fetch a text resource (FTP or HTTPS) — raw transport test\n";
	sprt::cerr << "  engine-refs   List the engine git branches and tags\n";
	sprt::cerr << "  engine-install [ref]  Clone the engine (default: master, with submodules)\n";
	sprt::cerr << "\nOptions for `install`:\n";
	getInstallParser().describe([](StringView str) { sprt::cerr << str; });
	sprt::cerr << "\nOptions for `new`:\n";
	getEngineParser().describe([](StringView str) { sprt::cerr << str; });
	sprt::cerr << "\nOptions for `build`:\n";
	getBuildParser().describe([](StringView str) { sprt::cerr << str; });
}

static int run(int argc, const char *argv[]) {
	auto prog = argc > 0 ? StringView(argv[0]) : StringView("xenolith-cli");
	auto cmd = argc > 1 ? StringView(argv[1]) : StringView();

	if (cmd == "detect") {
		return cmdDetect();
	} else if (cmd == "paths") {
		return cmdPaths();
	} else if (cmd == "config") {
		return cmdConfig(argc, argv);
	} else if (cmd == "state") {
		return cmdState();
	} else if (cmd == "verify") {
		return cmdVerify();
	} else if (cmd == "fetch") {
		return cmdFetch(argc, argv);
	} else if (cmd == "list") {
		return cmdList();
	} else if (cmd == "install") {
		return cmdInstall(argc, argv);
	} else if (cmd == "new") {
		return cmdNew(argc, argv);
	} else if (cmd == "build") {
		return cmdBuild(argc, argv);
	} else if (cmd == "engine-refs") {
		return cmdEngineRefs();
	} else if (cmd == "engine-install") {
		return cmdEngineInstall(argc, argv);
	}

	printUsage(prog);
	return cmd.empty() ? 0 : 2;
}

} // namespace stappler::xenolith::installer

int main(int argc, const char *argv[]) {
	// The CLI does not run the application framework, so the runtime platform still has to be
	// brought up (writable-root registration, default filesystem interface) — without it
	// filesystem::mkdir_recursive/write, used by the git checkout, resolve to nothing.
	//
	// perform_main is what does it: it reads the appconfig module the build generates, so the
	// bundle name and APPCONFIG_APP_PATH_COMMON from this project's Makefile actually reach the
	// runtime. A hand-written AppConfig here used to bypass both, which pinned the tool to the
	// ExecutableRelative scheme (App* directories next to the binary) and gave it a bundle name of
	// its own — the two things that must match the GUI for both to share one SDK store.
	return stappler::perform_main(argc, argv,
			[&] { return stappler::xenolith::installer::run(argc, argv); });
}
