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

#include "SPISettings.h"
#include "SPICatalogue.h"
#include "SPIEngineSource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

String SourceConfig::getEngineRepoUrl() const {
	return engineRepoUrl.empty() ? getDefaultEngineRepoUrl().str<mem_std::Interface>()
								 : engineRepoUrl;
}

String SourceConfig::getReleasesRoot() const {
	if (releasesRoot.empty()) {
		return getDefaultReleasesRoot();
	}
	// Stored without a trailing slash or with one, depending on what the user typed; every consumer
	// appends a path segment, so normalize here instead of at each call site.
	if (releasesRoot.back() != '/') {
		return releasesRoot + "/";
	}
	return releasesRoot;
}

String SourceConfig::getReleaseBase(StringView release) const {
	const auto rel = release.empty() ? getDefaultRelease() : release;
	return getReleasesRoot() + rel.str<mem_std::Interface>();
}

Settings Settings::load(StringView path) {
	Settings st;
	// const on purpose: the NON-const getValue() asserts when the key is missing (it would have to
	// return the shared null sentinel, which is read-only), and every key here is optional.
	const auto v = data::readFile<mem_std::Interface>(FileInfo(path));
	if (!v.hasValue("schema")) {
		return st; // missing or non-object -> defaults
	}
	st.schema = static_cast<uint32_t>(v.getInteger("schema", kSettingsSchemaVersion));

	st.sources.engineRepoUrl = v.getString("engineRepoUrl");
	st.sources.releasesRoot = v.getString("releaseSourceUrl");
	st.enginePath = v.getString("enginePath");
	st.toolchainsPath = v.getString("toolchainsPath");
	st.lang = v.getString("lang");

	const auto &autoUpdate = v.getValue("autoUpdate");
	if (autoUpdate.isDictionary()) {
		st.autoUpdateInstaller = autoUpdate.getBool("installer");
		st.autoUpdateEngine = autoUpdate.getBool("engine");
		st.autoUpdateReleases = autoUpdate.getBool("releases");
	}

	const auto &tools = v.getValue("tools");
	if (tools.isArray()) {
		for (const auto &item : tools.getArray()) {
			ToolAutoUpdate t;
			t.kind = parseKind(item.getString("kind"));
			t.id = item.getString("id");
			t.enabled = item.getBool("autoUpdate");
			if (!t.id.empty()) {
				st.tools.emplace_back(sp::move(t));
			}
		}
	}
	return st;
}

bool Settings::save(StringView path) const {
	Value v;
	v.setInteger(static_cast<int64_t>(schema), "schema");

	// Empty means "the built-in default": written out as absent rather than as the resolved value,
	// so a user who never chose a mirror keeps following the default when it changes.
	if (!sources.engineRepoUrl.empty()) {
		v.setString(sources.engineRepoUrl, "engineRepoUrl");
	}
	if (!sources.releasesRoot.empty()) {
		v.setString(sources.releasesRoot, "releaseSourceUrl");
	}
	if (!enginePath.empty()) {
		v.setString(enginePath, "enginePath");
	}
	if (!toolchainsPath.empty()) {
		v.setString(toolchainsPath, "toolchainsPath");
	}
	if (!lang.empty()) {
		v.setString(lang, "lang");
	}

	Value autoUpdate;
	autoUpdate.setBool(autoUpdateInstaller, "installer");
	autoUpdate.setBool(autoUpdateEngine, "engine");
	autoUpdate.setBool(autoUpdateReleases, "releases");
	v.setValue(sp::move(autoUpdate), "autoUpdate");

	if (!tools.empty()) {
		Value arr;
		for (const auto &t : tools) {
			Value item;
			item.setString(getKindName(t.kind), "kind");
			item.setString(t.id, "id");
			item.setBool(t.enabled, "autoUpdate");
			arr.addValue(sp::move(item));
		}
		v.setValue(sp::move(arr), "tools");
	}

	// The config dir may not exist yet - settings can be the first thing written into it.
	filesystem::mkdir_recursive(FileInfo(filepath::root(path)));

	return data::save(v, FileInfo(path), data::EncodeFormat::Pretty);
}

void Settings::applyTo(Layout &layout) const {
	// An empty field leaves the layout alone rather than writing "" into it: the layout's own
	// members are the defaults, and blanking them would produce paths rooted at nothing.
	if (!enginePath.empty()) {
		layout.engine = enginePath;
	}
	if (!toolchainsPath.empty()) {
		layout.toolchains = toolchainsPath;
	}
}

// The order here is the order the settings form and `xenolith-cli config` present: sources first,
// then the local paths, then the auto-update switches, then the language.
static constexpr SettingsField s_settingsFields[] = {
	{StringView("engineRepoUrl"), false, StringView("git URL the engine is cloned from")},
	{StringView("releaseSourceUrl"), false,
		StringView("FTP/HTTP root the binary releases are fetched from")},
	{StringView("enginePath"), false,
		StringView("engine checkout to build against; below --engine and $XENOLITH_ENGINE")},
	{StringView("toolchainsPath"), false,
		StringView("directory hosts/targets are searched in and installed into")},
	{StringView("autoUpdateInstaller"), true, StringView("update the installer itself")},
	{StringView("autoUpdateEngine"), true, StringView("update the engine checkouts")},
	{StringView("autoUpdateReleases"), true,
		StringView("update toolchains (per-tool overrides win)")},
	{StringView("lang"), false, StringView("interface language: en | ru | zh")},
};

SpanView<SettingsField> Settings::getFields() { return s_settingsFields; }

const SettingsField *Settings::getField(StringView key) {
	for (const auto &f : s_settingsFields) {
		if (f.key == key) {
			return &f;
		}
	}
	return nullptr;
}

Value Settings::getFieldValue(StringView key) const {
	// The RAW stored value, not the resolved one: a caller that wants "what would actually be used"
	// asks SourceConfig / the Layout, and a form that showed the resolved default in an empty field
	// would save it back as an explicit choice on the next blur.
	if (key == "engineRepoUrl") {
		return Value(sources.engineRepoUrl);
	} else if (key == "releaseSourceUrl") {
		return Value(sources.releasesRoot);
	} else if (key == "enginePath") {
		return Value(enginePath);
	} else if (key == "toolchainsPath") {
		return Value(toolchainsPath);
	} else if (key == "autoUpdateInstaller") {
		return Value(autoUpdateInstaller);
	} else if (key == "autoUpdateEngine") {
		return Value(autoUpdateEngine);
	} else if (key == "autoUpdateReleases") {
		return Value(autoUpdateReleases);
	} else if (key == "lang") {
		return Value(lang);
	}
	return Value();
}

bool Settings::setFieldValue(StringView key, const Value &value) {
	if (key == "engineRepoUrl") {
		sources.engineRepoUrl = value.getString();
	} else if (key == "releaseSourceUrl") {
		sources.releasesRoot = value.getString();
	} else if (key == "enginePath") {
		enginePath = value.getString();
	} else if (key == "toolchainsPath") {
		toolchainsPath = value.getString();
	} else if (key == "autoUpdateInstaller") {
		autoUpdateInstaller = value.getBool();
	} else if (key == "autoUpdateEngine") {
		autoUpdateEngine = value.getBool();
	} else if (key == "autoUpdateReleases") {
		autoUpdateReleases = value.getBool();
	} else if (key == "lang") {
		lang = value.getString();
	} else {
		return false;
	}
	return true;
}

bool Settings::getToolAutoUpdate(Kind kind, StringView id) const {
	for (const auto &t : tools) {
		if (t.kind == kind && t.id == id) {
			return t.enabled;
		}
	}
	return autoUpdateReleases;
}

void Settings::setToolAutoUpdate(Kind kind, StringView id, bool enabled) {
	for (auto &t : tools) {
		if (t.kind == kind && t.id == id) {
			t.enabled = enabled;
			return;
		}
	}
	ToolAutoUpdate t;
	t.kind = kind;
	t.id = id.str<mem_std::Interface>();
	t.enabled = enabled;
	tools.emplace_back(sp::move(t));
}

} // namespace stappler::xenolith::installer
