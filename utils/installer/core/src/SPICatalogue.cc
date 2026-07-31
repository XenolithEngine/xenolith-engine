#include "SPICatalogue.h"

#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

Vector<StringView> tokenize(StringView line) {
	Vector<StringView> out;
	size_t i = 0;
	while (i < line.size()) {
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
			++i;
		}
		auto start = i;
		while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
			++i;
		}
		if (i > start) {
			out.emplace_back(line.data() + start, i - start);
		}
	}
	return out;
}

bool ends_with(StringView s, StringView suffix) {
	if (s.size() < suffix.size()) {
		return false;
	}
	auto off = s.size() - suffix.size();
	for (size_t i = 0; i < suffix.size(); ++i) {
		if (s[off + i] != suffix[i]) {
			return false;
		}
	}
	return true;
}

} // namespace

Vector<RemoteEntry> parse_listing(StringView text) {
	Vector<RemoteEntry> out;
	size_t pos = 0;
	while (pos < text.size()) {
		// find end of line
		size_t eol = pos;
		while (eol < text.size() && text[eol] != '\n') {
			++eol;
		}
		StringView line(text.data() + pos, eol - pos);
		pos = (eol < text.size()) ? eol + 1 : eol;

		if (line.empty() || line.starts_with("total ")) {
			continue;
		}

		auto tokens = tokenize(line);
		if (tokens.size() < 9) {
			continue;
		}

		RemoteEntry e;
		e.isDir = tokens[0].size() > 0 && tokens[0][0] == 'd';
		e.size = std::strtoull(toString(tokens[4]).c_str(), nullptr, 10);

		// name = everything after the 8th whitespace group (after time field)
		size_t nameStart = 0;
		int spaces = 0;
		for (size_t i = 0; i < line.size(); ++i) {
			if (line[i] == ' ' || line[i] == '\t') {
				++spaces;
				while (i + 1 < line.size() && (line[i + 1] == ' ' || line[i + 1] == '\t')) {
					++i;
				}
				if (spaces == 8) {
					nameStart = i + 1;
					break;
				}
			}
		}
		if (nameStart > 0 && nameStart < line.size()) {
			auto len = line.size() - nameStart;
			if (len > 0 && line[nameStart + len - 1] == '\r') {
				--len; // trim trailing \r
			}
			e.name = toString(StringView(line.data() + nameStart, len));
		}
		if (!e.name.empty()) {
			out.push_back(sp::move(e));
		}
	}
	return out;
}

Vector<CatalogueComponent> build_catalogue(StringView hostsText, StringView targetsText) {
	Vector<CatalogueComponent> out;

	auto buildKind = [&](StringView listing, Kind kind) {
		auto entries = parse_listing(listing);
		for (const auto &e : entries) {
			StringView name(e.name);
			if (!ends_with(name, ".tar.xz")) {
				continue;
			}

			// check for a matching .sig
			String sigName = toString(name) + ".sig";
			bool signed_ = false;
			for (const auto &s : entries) {
				if (s.name == sigName) {
					signed_ = true;
					break;
				}
			}
			if (!signed_) {
				continue; // drop unsigned
			}

			// id = filename without ".tar.xz" (7 chars)
			String id = toString(StringView(name.data(), name.size() - 7));

			// split triple + variant at "+"
			String triple = id;
			String variant;
			auto plus = id.find('+');
			if (plus != String::npos) {
				triple = id.substr(0, plus);
				variant = id.substr(plus + 1);
			}

			CatalogueComponent c;
			c.id = sp::move(id);
			c.triple = sp::move(triple);
			c.variant = sp::move(variant);
			c.kind = kind;
			c.size = e.size;
			c.signed_ = true;
			out.push_back(sp::move(c));
		}
	};

	buildKind(hostsText, Kind::Host);
	buildKind(targetsText, Kind::Target);
	return out;
}

} // namespace stappler::xenolith::installer
