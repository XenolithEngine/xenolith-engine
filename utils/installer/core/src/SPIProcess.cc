#include "SPIProcess.h"

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>

extern char **environ;
extern "C" pid_t waitpid(pid_t, int *, int);

#ifndef WIFEXITED
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

std::pair<String, String> split_env(const char *e) {
	const char *eq = std::strchr(e, '=');
	if (eq) {
		return {String(e, eq), String(eq + 1)};
	}
	return {String(e), String()};
}

// Build the KEY=VAL string list for the child. When merging, start from the inherited environ and
// apply `opts.env` on top (later entries override); when replacing, use only `opts.env`.
Vector<String> build_env(const ProcessSpawn &opts) {
	Vector<std::pair<String, String>> merged;
	if (!opts.envReplace) {
		for (char **e = environ; e && *e; ++e) {
			merged.push_back(split_env(*e));
		}
	}
	for (const auto &kv : opts.env) {
		bool found = false;
		for (auto &m : merged) {
			if (m.first == kv.first) {
				m.second = kv.second;
				found = true;
				break;
			}
		}
		if (!found) {
			merged.push_back(kv);
		}
	}
	Vector<String> out;
	out.reserve(merged.size());
	for (const auto &m : merged) {
		out.push_back(m.first + "=" + m.second);
	}
	return out;
}

} // namespace

int run_process(const Vector<String> &argv, const ProcessSpawn &opts) {
	if (argv.empty()) {
		return -1;
	}

	Vector<const char *> args;
	args.reserve(argv.size() + 1);
	for (const auto &a : argv) {
		args.push_back(a.c_str());
	}
	args.push_back(nullptr);

	// execvp reads the global `environ`; swap in our merged environment for the child, then
	// restore the parent's immediately after fork (the child got its own copy).
	Vector<String> envStorage = build_env(opts);
	Vector<const char *> envp;
	envp.reserve(envStorage.size() + 1);
	for (const auto &s : envStorage) {
		envp.push_back(s.c_str());
	}
	envp.push_back(nullptr);

	char **savedEnv = environ;
	environ = const_cast<char **>(envp.data());

	pid_t pid = fork();
	if (pid < 0) {
		environ = savedEnv;
		return -1;
	}
	if (pid == 0) {
		// child
		if (!opts.cwd.empty() && ::chdir(opts.cwd.c_str()) != 0) {
			::_exit(127);
		}
		::execvp(args[0], const_cast<char *const *>(args.data()));
		::_exit(127); // exec failed
	}

	environ = savedEnv;

	int status = 0;
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR) {
			return -1;
		}
	}
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

} // namespace stappler::xenolith::installer
