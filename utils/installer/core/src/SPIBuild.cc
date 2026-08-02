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

#include "SPIBuild.h"
#include "SPITriple.h"
#include "SPIManifest.h"
#include "SPIInstall.h"
#include "SPIEngineSource.h"
#include "SPIJob.h"
#include "SPIProcess.h"

#include "SPMakefile.h" // Makefile::getVariableValue, ErrorReporter, decodePathSpaces
#include "SPMakefileProject.h" // makefile::loadProject
#include "SPMakefileBuilder.h" // makefile::runBuild, makefile::BuildConfig

#include <stdlib.h> // getenv / setenv / unsetenv / realpath: no runtime wrapper exists
#include <unistd.h> // chdir

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

#if SPRT_WINDOWS
constexpr StringView kPathSeparator = ";";
#else
constexpr StringView kPathSeparator = ":";
#endif

// Environment variables the build needs and this file therefore overwrites.
constexpr const char *kManagedVariables[] = {"PATH", "LC_ALL", "LANG"};
constexpr uint32_t kManagedVariableCount = 3;

// ::chdir and ::setenv are process-global. They are only ever touched on the job thread, and this
// guard restores the process to its previous state on every exit path — including the early ones.
class ProcessEnvironmentGuard {
public:
	ProcessEnvironmentGuard() {
		_cwd = filesystem::currentDir<mem_std::Interface>();
		for (uint32_t i = 0; i < kManagedVariableCount; ++i) {
			if (const char *value = ::getenv(kManagedVariables[i])) {
				_values[i] = value;
				_hasValue[i] = true;
			}
		}
	}

	~ProcessEnvironmentGuard() {
		for (uint32_t i = 0; i < kManagedVariableCount; ++i) {
			if (_hasValue[i]) {
				::setenv(kManagedVariables[i], _values[i].data(), 1);
			} else {
				::unsetenv(kManagedVariables[i]);
			}
		}
		if (!_cwd.empty()) {
			::chdir(_cwd.data());
		}
	}

	StringView getValue(uint32_t idx) const {
		return _hasValue[idx] ? StringView(_values[idx]) : StringView();
	}

protected:
	String _cwd;
	String _values[kManagedVariableCount];
	bool _hasValue[kManagedVariableCount] = {false, false, false};
};

// The target before any '+' variant suffix — a variant of the host triple still builds natively.
StringView getTargetBase(StringView target) {
	StringView reader(target);
	return reader.readUntil<StringView::Chars<'+'>>();
}

// There is no runtime equivalent of realpath (the make engine falls back to libc for it too), and
// the build needs an absolute project dir for `cd` and for the reported artifact path.
String getCanonicalPath(StringView path) {
	char buf[4_KiB];
	if (::realpath(toString(path).data(), buf)) {
		return toString(StringView(buf));
	}
	return toString(path);
}

String readMakeVariable(makefile::Makefile *mk, StringView name, makefile::ErrorReporter &err) {
	mem_std::String raw;
	mk->getVariableValue(name, [&](StringView v) { raw.append(v.data(), v.size()); }, err);

	// A make-visible path may carry space placeholders.
	mem_std::String decoded;
	return makefile::decodePathSpaces(StringView(raw), decoded).str<mem_std::Interface>();
}

} // namespace

BuildResult buildProject(StringView path, const Layout &layout, const BuildOptions &opts,
		StringView engineOverride, const Callback<void(StringView)> *output) {
	BuildResult result;

	auto projectDir = getCanonicalPath(path);
	if (!isFile(mergePath(projectDir, "Makefile"))) {
		result.setError(Status::ErrorNotFound, "no Makefile in ", projectDir);
		return result;
	}

	auto host = resolveHost(getNativeArch(), getNativeOs());
	if (host.native.empty()) {
		result.setError(Status::ErrorNotSupported, "no SDK host for ", getNativeArch(), "-",
				getNativeOs());
		return result;
	}

	auto target = opts.target.empty() ? host.native : opts.target;

	bool engineOk = false;
	auto engineRoot = resolveEngineRoot(layout, engineOverride, &engineOk);
	if (!engineOk) {
		result.setError(Status::ErrorNotFound, "engine not found at ", engineRoot);
		return result;
	}

	auto hostBin = mergePath(getComponentDir(layout, Kind::Host, host.native), "bin");
	if (!isDirectory(hostBin)) {
		result.setError(Status::ErrorNotFound, "host toolchain '", host.native, "' not installed");
		return result;
	}
	if (!isDirectory(getComponentDir(layout, Kind::Target, StringView(target)))) {
		result.setError(Status::ErrorNotFound, "target '", target, "' not installed");
		return result;
	}

	// Heal stale toolchain links (e.g. after a data-root move), then link into this engine.
	relinkAllEngines(layout);
	linkToolchainsIntoEnginePath(layout, StringView(engineRoot));

	// A cross build cannot run here, and it collects its artifacts through the `install` goal.
	bool isCross = getTargetBase(StringView(target)) != host.native;

	auto jobStatus = runJob([&] {
		ProcessEnvironmentGuard env;

		// The toolchain itself is found through STAPPLER_ROOT, but recipes and parse-time
		// $(shell …) calls are shell children that inherit the environment: the host `bin/` must
		// lead PATH before the makefile is even read (macOS resolves its SDK with xcrun there).
		::setenv("PATH", toString(hostBin, kPathSeparator, env.getValue(0)).data(), 1);
		::setenv("LC_ALL", "C", 1);
		::setenv("LANG", "C", 1);

		// The build writes ./compile_commands.json and resolves LOCAL_OUTDIR / LOCAL_INSTALL_DIR
		// relative to the working directory, so it really has to run inside the project.
		if (::chdir(projectDir.data()) != 0) {
			result.setError(Status::ErrorInvalidArguemnt, "cannot enter ", projectDir);
			return;
		}

		makefile::ErrorReporter err(nullptr);
		err.ref = const_cast<Callback<void(StringView)> *>(output);
		err.callback = [](void *ref, log::LogType, StringView msg) {
			if (ref) {
				(*static_cast<const Callback<void(StringView)> *>(ref))(msg);
			}
		};

		// STAPPLER_ROOT goes in as a make variable rather than an environment one: nothing under
		// make/ reads it from the environment, and Origin::CommandLine beats the project's `?=`.
		Vector<makefile::ProjectVariable> variables;
		variables.emplace_back(
				makefile::ProjectVariable{StringView("STAPPLER_ROOT"), StringView(engineRoot)});
		if (target != host.native) {
			variables.emplace_back(
					makefile::ProjectVariable{StringView("STAPPLER_TARGET"), StringView(target)});
		}
		if (opts.release) {
			variables.emplace_back(
					makefile::ProjectVariable{StringView("RELEASE"), StringView("1")});
		}

		auto mk = makefile::loadProject(StringView(projectDir), variables, err);
		if (!mk) {
			result.setError(Status::ErrorInvalidArguemnt, "failed to load the project makefile in ",
					projectDir);
			return;
		}

		makefile::BuildConfig cfg;
		cfg.targets.emplace_back(isCross ? StringView("install") : StringView("all"));
		cfg.jobs = opts.jobs; // 0 → hardware concurrency
		cfg.rootDir = StringView(projectDir);
		cfg.output = output;

		result.exitCode = makefile::runBuild(mk, cfg, err);
		if (result.exitCode != 0) {
			result.setError(Status::ErrorUnknown, "build failed (exit ", result.exitCode, ")");
			return;
		}

		// Ask make where the artifact went instead of reconstructing the output layout here.
		// BUILD_EXECUTABLE is absolute; BUILD_INSTALL_EXECUTABLE is relative to the project.
		auto exe = readMakeVariable(mk,
				isCross ? StringView("BUILD_INSTALL_EXECUTABLE") : StringView("BUILD_EXECUTABLE"),
				err);
		if (!exe.empty()) {
			result.executable = filepath::isAbsolute(exe) ? exe : mergePath(projectDir, exe);
		}
	});

	if (!isSuccessful(jobStatus) && result.valid()) {
		result.setError(jobStatus, "failed to run the build thread");
	}
	if (!result) {
		return result;
	}

	const auto buildType = opts.release ? StringView("release") : StringView("debug");

	if (opts.run && isCross) {
		result.message = toString("built ", projectDir, " for ", target,
				" — cross-compiled, cannot run on this host");
		return result;
	}

	if (opts.run) {
		if (result.executable.empty() || !isFile(result.executable)) {
			result.setError(Status::ErrorNotFound, "built binary not found: ", result.executable);
			return result;
		}

		StringView argv[] = {StringView(result.executable)};
		auto proc = runCommand(argv, StringView(projectDir), output);
		result.runExitCode = proc.exitCode;
		result.message = toString("ran ", result.executable, " (exit ", proc.exitCode, ")");
		return result;
	}

	result.message = toString("built ", projectDir, " for ", target, " (", buildType, ")");
	return result;
}

} // namespace stappler::xenolith::installer
