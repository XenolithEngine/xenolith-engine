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

#include "SPIState.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstalledState InstalledState::load(StringView path) {
	InstalledState st;
	// const on purpose: the NON-const getValue() asserts when the key is missing (it would have to
	// return the shared null sentinel, which is read-only), and "components" is optional — a
	// manifest with a schema and no components is a legitimate empty registry.
	const auto v = data::readFile<mem_std::Interface>(FileInfo(path));
	if (!v.hasValue("schema")) {
		return st; // missing or non-object → empty/default
	}
	st.schema = static_cast<uint32_t>(v.getInteger("schema", kStateSchemaVersion));

	const auto &comps = v.getValue("components");
	if (comps.isArray()) {
		for (const auto &item : comps.getArray()) {
			InstalledComponent c;
			c.id = item.getString("id");
			c.triple = item.getString("triple");
			if (item.hasValue("variant")) {
				c.variant = item.getString("variant");
			}
			c.kind = parseKind(item.getString("kind"));
			c.release = item.getString("release");
			if (item.hasValue("sha256")) {
				c.sha256 = item.getString("sha256");
			}
			c.installedAt = item.getString("installed_at");
			c.path = item.getString("path");
			st.components.emplace_back(sp::move(c));
		}
	}
	return st;
}

bool InstalledState::save(StringView path) const {
	Value v;
	v.setInteger(static_cast<int64_t>(schema), "schema");

	Value arr;
	for (const auto &c : components) {
		Value item;
		item.setString(c.id, "id");
		item.setString(c.triple, "triple");
		if (!c.variant.empty()) {
			item.setString(c.variant, "variant");
		}
		item.setString(getKindName(c.kind), "kind");
		item.setString(c.release, "release");
		if (!c.sha256.empty()) {
			item.setString(c.sha256, "sha256");
		}
		item.setString(c.installedAt, "installed_at");
		item.setString(c.path, "path");
		arr.addValue(sp::move(item));
	}
	v.setValue(sp::move(arr), "components");

	// The config dir may not exist yet: the registry is the first thing written into it.
	filesystem::mkdir_recursive(FileInfo(filepath::root(path)));

	return data::save(v, FileInfo(path), data::EncodeFormat::Pretty);
}

void InstalledState::upsert(InstalledComponent c) {
	for (auto &it : components) {
		if (it.id == c.id && it.kind == c.kind) {
			it = sp::move(c);
			return;
		}
	}
	components.emplace_back(sp::move(c));
}

bool InstalledState::remove(StringView id, Kind kind) {
	for (auto it = components.begin(); it != components.end(); ++it) {
		if (it->id == id && it->kind == kind) {
			components.erase(it);
			return true;
		}
	}
	return false;
}

const InstalledComponent *InstalledState::get(StringView id, Kind kind) const {
	for (auto &c : components) {
		if (c.id == id && c.kind == kind) {
			return &c;
		}
	}
	return nullptr;
}

Vector<const InstalledComponent *> InstalledState::getInvalid(
		const Function<bool(StringView)> &exists) const {
	Vector<const InstalledComponent *> out;
	for (auto &c : components) {
		if (!exists(c.path)) {
			out.emplace_back(&c);
		}
	}
	return out;
}

} // namespace stappler::xenolith::installer
