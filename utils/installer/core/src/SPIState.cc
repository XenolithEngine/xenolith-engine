#include "SPIState.h"

#include <algorithm> // remove_if/erase

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstalledState InstalledState::load(StringView path) {
	InstalledState st;
	auto v = data::readFile<memory::StandartInterface>(FileInfo(StringView(path)));
	if (!v.hasValue("schema")) {
		return st; // missing or non-object → empty/default
	}
	st.schema = static_cast<uint32_t>(v.getInteger("schema", kStateSchemaVersion));

	auto comps = v.getValue("components");
	if (comps.isArray()) {
		for (const auto &item : comps.getArray()) {
			InstalledComponent c;
			c.id = item.getString("id");
			c.triple = item.getString("triple");
			if (item.hasValue("variant")) {
				c.variant = item.getString("variant");
			}
			c.kind = kind_from_string(item.getString("kind"));
			c.release = item.getString("release");
			if (item.hasValue("sha256")) {
				c.sha256 = item.getString("sha256");
			}
			c.installedAt = item.getString("installed_at");
			c.path = item.getString("path");
			st.components.push_back(sp::move(c));
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
		item.setString(kind_to_string(c.kind), "kind");
		item.setString(c.release, "release");
		if (!c.sha256.empty()) {
			item.setString(c.sha256, "sha256");
		}
		item.setString(c.installedAt, "installed_at");
		item.setString(c.path, "path");
		arr.addValue(sp::move(item));
	}
	v.setValue(sp::move(arr), "components");

	return data::save(v, FileInfo(StringView(path)), data::EncodeFormat::Pretty);
}

void InstalledState::upsert(InstalledComponent c) {
	for (auto &it : components) {
		if (it.id == c.id && it.kind == c.kind) {
			it = sp::move(c);
			return;
		}
	}
	components.push_back(sp::move(c));
}

bool InstalledState::remove(StringView id, Kind kind) {
	auto before = components.size();
	components.erase(std::remove_if(components.begin(), components.end(),
							[&](const InstalledComponent &c) { return c.id == id && c.kind == kind; }),
			components.end());
	return components.size() != before;
}

const InstalledComponent *InstalledState::get(StringView id, Kind kind) const {
	for (auto &c : components) {
		if (c.id == id && c.kind == kind) {
			return &c;
		}
	}
	return nullptr;
}

Vector<const InstalledComponent *> InstalledState::invalid(
		const Function<bool(StringView)> &exists) const {
	Vector<const InstalledComponent *> out;
	for (auto &c : components) {
		if (!exists(c.path)) {
			out.push_back(&c);
		}
	}
	return out;
}

} // namespace stappler::xenolith::installer
