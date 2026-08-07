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

#include "SPICatalogue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// The archive suffix the catalogue is built from; the id is the file name without it.
constexpr StringView kArchiveSuffix = ".tar.xz";

// vsFTPd LIST line: perms links owner group size month day time name — 8 whitespace-separated
// fields, then the name (which may itself contain spaces).
constexpr uint32_t kListingFieldCount = 8;
constexpr uint32_t kListingSizeField = 4;

// Compare two `sdk-v…` tags so multi-digit components compare numerically (beta10 > beta2,
// v2 < v10), not lexically. Strips a shared `sdk-v` prefix, then walks alternating digit /
// non-digit runs: digit runs by numeric value, non-digit runs lexically.
int compareSdkTags(StringView a, StringView b) {
	static constexpr StringView Prefix = "sdk-v";
	auto strip = [](StringView s) -> StringView {
		return s.starts_with(Prefix)
				? StringView(s.data() + Prefix.size(), s.size() - Prefix.size())
				: s;
	};
	auto x = strip(a);
	auto y = strip(b);
	auto isDigit = [](char c) -> bool { return c >= '0' && c <= '9'; };

	while (!x.empty() && !y.empty()) {
		const bool xd = isDigit(x.data()[0]);
		const bool yd = isDigit(y.data()[0]);
		if (xd != yd) {
			return xd ? -1 : 1; // a digit run sorts before a non-digit run
		}
		size_t xn = 0;
		size_t yn = 0;
		while (xn < x.size() && isDigit(x.data()[xn]) == xd) { ++xn; }
		while (yn < y.size() && isDigit(y.data()[yn]) == yd) { ++yn; }
		const StringView xr(x.data(), xn);
		const StringView yr(y.data(), yn);
		x = StringView(x.data() + xn, x.size() - xn);
		y = StringView(y.data() + yn, y.size() - yn);
		if (xd) {
			// numeric: leading zeros carry no value ("v01" == "v1"), so trim them before comparing
			// by length — otherwise the longer run wins and "sdk-v01" would outrank "sdk-v1".
			auto trimZeros = [](StringView r) {
				while (r.size() > 1 && r.data()[0] == '0') {
					r = StringView(r.data() + 1, r.size() - 1);
				}
				return r;
			};
			const auto xv = trimZeros(xr);
			const auto yv = trimZeros(yr);
			// a longer digit run is the larger number; equal length ⇒ lexical == numeric
			if (xv.size() != yv.size()) {
				return xv.size() < yv.size() ? -1 : 1;
			}
			if (xv != yv) {
				return xv < yv ? -1 : 1;
			}
		} else if (xr != yr) {
			return xr < yr ? -1 : 1;
		}
	}
	if (x.empty() && y.empty()) {
		return 0;
	}
	return x.empty() ? -1 : 1; // the shorter (a prefix of the other) sorts first
}

} // namespace

Vector<RemoteEntry> parseListing(StringView text) {
	Vector<RemoteEntry> out;
	text.split<StringView::Chars<'\n'>>([&](StringView line) {
		line.trimChars<StringView::Chars<'\r'>>();
		if (line.empty() || line.starts_with("total ")) {
			return;
		}

		StringView reader(line);
		StringView fields[kListingFieldCount];
		for (uint32_t i = 0; i < kListingFieldCount; ++i) {
			reader.skipChars<StringView::WhiteSpace>();
			fields[i] = reader.readUntil<StringView::WhiteSpace>();
			if (fields[i].empty()) {
				return; // not a listing line
			}
		}
		reader.skipChars<StringView::WhiteSpace>();
		if (reader.empty()) {
			return;
		}

		RemoteEntry e;
		e.isDir = fields[0].is('d');
		e.size = uint64_t(fields[kListingSizeField].readInteger(10).get(0));
		e.name = reader.str<mem_std::Interface>();
		out.emplace_back(sp::move(e));
	});
	return out;
}

Vector<CatalogueComponent> buildCatalogue(StringView hostsText, StringView targetsText) {
	Vector<CatalogueComponent> out;

	auto buildKind = [&](StringView listing, Kind kind) {
		auto entries = parseListing(listing);
		for (const auto &e : entries) {
			StringView name(e.name);
			if (!name.ends_with(kArchiveSuffix)) {
				continue;
			}

			// The signature rule is the security gate: an archive without a matching .sig is not
			// presented at all.
			auto sigName = toString(name, ".sig");
			bool isSigned = false;
			for (const auto &s : entries) {
				if (s.name == sigName) {
					isSigned = true;
					break;
				}
			}
			if (!isSigned) {
				continue;
			}

			// id = file name without the suffix; it splits into <triple>[+<variant>]
			StringView id(name.data(), name.size() - kArchiveSuffix.size());
			StringView reader(id);
			StringView triple = reader.readUntil<StringView::Chars<'+'>>();
			reader.skipChars<StringView::Chars<'+'>>();

			CatalogueComponent c;
			c.id = id.str<mem_std::Interface>();
			c.triple = triple.str<mem_std::Interface>();
			c.variant = reader.str<mem_std::Interface>();
			c.kind = kind;
			c.size = e.size;
			c.isSigned = true;
			out.emplace_back(sp::move(c));
		}
	};

	buildKind(hostsText, Kind::Host);
	buildKind(targetsText, Kind::Target);
	return out;
}

String resolveActiveRelease(StringView releasesListing) {
	String best = toString(getDefaultRelease());
	for (const auto &e : parseListing(releasesListing)) {
		if (!e.isDir) {
			continue;
		}
		StringView name(e.name);
		if (!name.starts_with("sdk-v")) {
			continue;
		}
		if (compareSdkTags(name, best) > 0) {
			best = name.str<mem_std::Interface>();
		}
	}
	return best;
}

} // namespace stappler::xenolith::installer
