#include "SPIBuild.h"
#include "SPITriple.h"
#include "SPIManifest.h"
#include "SPIInstall.h"
#include "SPIEngineSource.h"
#include "SPIProcess.h"
#include "SPIScaffold.h" // sanitize_project_name

#include "SPFilesystem.h"
#include "SPFilepath.h"

#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

bool build_is_dir(StringView path) {
	struct stat st;
	return ::stat(path.data(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool build_is_file(StringView path) {
	struct stat st;
	return ::stat(path.data(), &st) == 0 && S_ISREG(st.st_mode);
}

// The subdir the engine nests build output under — named after the host compiler binary.
StringView host_cc_subdir() {
	return StringView("cc");
}

unsigned available_parallelism() {
	long n = ::sysconf(_SC_NPROCESSORS_ONLN);
	return n > 0 ? static_cast<unsigned>(n) : 4u;
}

// target_base = target before any '+' variant suffix.
String target_base(StringView target) {
	auto pos = target.find('+');
	if (pos < target.size()) {
		return toString(StringView(target.data(), pos));
	}
	return toString(target);
}

String canonicalize(StringView path) {
	char buf[4096];
	if (::realpath(path.data(), buf)) {
		return toString(buf);
	}
	return toString(path);
}

// Run a freshly-built binary. macOS → .app bundle, otherwise a plain executable.
BuildResult run_built(StringView projPath, StringView target, bool release) {
	BuildResult r;
	String name = sanitize_project_name(
			[&]() -> StringView {
				auto pos = projPath.rfind('/');
				return (pos < projPath.size())
						? StringView(projPath.data() + pos + 1, projPath.size() - pos - 1)
						: projPath;
			}());
	String buildType = release ? toString("release") : toString("debug");
	String outDir = toString(projPath) + "/stappler-build/" + toString(target) + "/" + buildType
			+ "/" + toString(host_cc_subdir());

	const char *os = native_os().data();
	String exe;
	if (os == StringView("macos")) {
		exe = outDir + "/" + name + ".app/Contents/MacOS/" + name;
	} else if (os == StringView("windows")) {
		exe = outDir + "/" + name + ".exe";
	} else {
		exe = outDir + "/" + name;
	}
	if (!build_is_file(StringView(exe))) {
		r.status = Status::ErrorNotFound;
		r.error = toString("built binary not found: ") + exe;
		return r;
	}
	exe = canonicalize(StringView(exe));
	String cwd = canonicalize(projPath);

	ProcessSpawn opts;
	opts.cwd = cwd;
	int code = run_process({exe}, opts);
	char buf[64];
	snprintf(buf, sizeof(buf), "ran %s (exit %d)", exe.c_str(), code);
	r.message = toString(buf);
	return r;
}

} // namespace

BuildResult build_project(StringView path, const Layout &layout, const BuildOptions &opts,
		const String *engineOverride) {
	BuildResult r;

	if (!build_is_file(toString(path) + "/Makefile")) {
		r.status = Status::ErrorNotFound;
		r.error = toString("no Makefile in ") + toString(path);
		return r;
	}

	auto host = resolve_host(native_arch(), native_os());
	if (host.native.empty()) {
		r.status = Status::ErrorUnknown;
		char buf[96];
		snprintf(buf, sizeof(buf), "no SDK host for %s-%s", toString(native_arch()).c_str(),
				toString(native_os()).c_str());
		r.error = toString(buf);
		return r;
	}

	String target = opts.target.empty() ? host.native : opts.target;
	bool engineOk = false;
	String engineRoot = resolve_engine_root(layout, engineOverride, &engineOk);
	if (!engineOk) {
		r.status = Status::ErrorUnknown;
		r.error = toString("engine not found at ") + engineRoot;
		return r;
	}

	String hostBin = component_dir(layout, Kind::Host, host.native) + "/bin";
	if (!build_is_dir(StringView(hostBin))) {
		r.status = Status::ErrorUnknown;
		r.error = toString("host toolchain '") + host.native + toString("' not installed");
		return r;
	}
	if (!build_is_dir(component_dir(layout, Kind::Target, StringView(target)))) {
		r.status = Status::ErrorUnknown;
		r.error = toString("target '") + target + toString("' not installed");
		return r;
	}

	// Heal stale toolchain links (e.g. after a data-root move), then link into this engine.
	relink_all_engines(layout);
	link_toolchains_into_engine_path(layout, StringView(engineRoot));

	// PATH = host toolchain bin first, then the inherited PATH (so make/cc resolve to the toolchain).
	ProcessSpawn spawn;
	spawn.cwd = toString(path);
	const char *pathEnv = std::getenv("PATH");
	spawn.env.emplace_back(toString("PATH"),
			hostBin + ":" + toString(pathEnv ? pathEnv : ""));
	spawn.env.emplace_back(toString("STAPPLER_ROOT"), toString(engineRoot));
	spawn.env.emplace_back(toString("LC_ALL"), toString("C"));
	spawn.env.emplace_back(toString("LANG"), toString("C"));

	unsigned jobs = available_parallelism();

	Vector<String> argv;
	argv.push_back(toString("make"));
	char jb[16];
	snprintf(jb, sizeof(jb), "-j%u", jobs);
	argv.push_back(toString(jb));

	bool runnable = (target_base(StringView(target)) == host.native);
	if (!runnable) {
		argv.push_back(toString("install"));
		argv.push_back(toString("STAPPLER_TARGET=") + target);
	} else if (target != host.native) {
		argv.push_back(toString("STAPPLER_TARGET=") + target);
	}
	if (opts.release) {
		argv.push_back(toString("RELEASE=1"));
	}

	const char *bt = opts.release ? "release" : "debug";
	std::cerr << "• Building " << toString(path).c_str() << " for " << target.c_str() << " (" << bt
			  << ", -j" << jobs << ")…\n";

	int code = run_process(argv, spawn);
	if (code != 0) {
		r.status = Status::ErrorUnknown;
		char buf[48];
		snprintf(buf, sizeof(buf), "build failed (exit %d)", code);
		r.error = toString(buf);
		return r;
	}

	if (opts.run && !runnable) {
		r.message = toString("built ") + toString(path) + toString(" for ") + target
				+ toString(" — cross-compiled, cannot run on this host");
		return r;
	}
	if (opts.run) {
		return run_built(path, StringView(target), opts.release);
	}
	r.message = toString("built ") + toString(path) + toString(" for ") + target
			+ toString(" (") + toString(bt) + toString(")");
	return r;
}

} // namespace stappler::xenolith::installer
