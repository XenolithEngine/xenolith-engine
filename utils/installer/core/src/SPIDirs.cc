#include "SPIDirs.h"

#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// join two path segments with exactly one '/' between them
String join(StringView a, StringView b) {
	String r;
	r.reserve(a.size() + b.size() + 1);
	r.append(a.data(), a.size());
	if (!r.empty() && r.back() != '/') {
		r += '/';
	}
	r.append(b.data(), b.size());
	return r;
}

} // namespace

Layout Layout::from_home(StringView home) {
	return Layout{join(home, "config"), join(home, "data"), join(home, "cache")};
}

Layout Layout::system() {
#if SPRT_WINDOWS
	const char *base = std::getenv("LOCALAPPDATA");
	if (!base || !*base) {
		base = "C:/Users/Public";
	}
	return from_home(join(StringView(base), "xenolith"));
#else
	// macOS/Linux: ~/.local/share/xenolith (avoid ~/Library/Application Support — it has a SPACE,
	// which GNU make cannot handle in STAPPLER_ROOT/include paths).
	const char *home = std::getenv("HOME");
	if (!home || !*home) {
		home = "/tmp";
	}
	return from_home(join(join(StringView(home), ".local/share"), "xenolith"));
#endif
}

Layout Layout::resolve(const String *prefix, const String *envHome) {
	if (prefix) {
		return from_home(*prefix);
	}
	if (envHome && !envHome->empty()) {
		return from_home(*envHome);
	}
	return system();
}

Layout Layout::resolve_from_env(const String *prefix) {
	const char *e = std::getenv("XENOLITH_HOME");
	String env;
	if (e && *e) {
		env = e;
	}
	return resolve(prefix, env.empty() ? nullptr : &env);
}

String Layout::installed_manifest() const { return join(config, "installed.json"); }
String Layout::toolchains_store_dir() const { return join(data, "toolchains"); }
String Layout::toolchains_hosts_dir() const { return join(toolchains_store_dir(), "hosts"); }
String Layout::toolchains_targets_dir() const { return join(toolchains_store_dir(), "targets"); }
String Layout::toolchain_dir(Kind kind, StringView id) const {
	return join(kind == Kind::Host ? toolchains_hosts_dir() : toolchains_targets_dir(), id);
}
String Layout::engines_dir() const { return join(data, "engines"); }
String Layout::engine_dir(StringView ref) const { return join(engines_dir(), ref); }
String Layout::download_tmp() const { return join(cache, "downloads"); }

} // namespace stappler::xenolith::installer
