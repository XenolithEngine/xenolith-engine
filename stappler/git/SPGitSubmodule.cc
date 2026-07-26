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

#include "SPGitSubmodule.h"

#include <sprt/runtime/utils/urlview.h>

namespace STAPPLER_VERSIONIZED stappler::git {

// --- helpers ---------------------------------------------------------------

static bool isWs(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static StringView trimWs(StringView s) {
	while (!s.empty() && isWs(s.data()[0])) { s = StringView(s.data() + 1, s.size() - 1); }
	while (!s.empty() && isWs(s.data()[s.size() - 1])) { s = StringView(s.data(), s.size() - 1); }
	return s;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

static bool iequal(StringView a, StringView b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		if (lower(a.data()[i]) != lower(b.data()[i])) {
			return false;
		}
	}
	return true;
}

// --- parseGitmodules -------------------------------------------------------

Vector<SubmoduleSpec> parseGitmodules(BytesView data) {
	Vector<SubmoduleSpec> out;

	const char *p = reinterpret_cast<const char *>(data.data());
	size_t n = data.size();
	size_t i = 0;

	bool inSub = false;
	SubmoduleSpec cur;
	auto flush = [&]() {
		if (inSub && !cur.path.empty() && !cur.url.empty()) {
			out.emplace_back(sp::move(cur));
		}
		cur = SubmoduleSpec();
	};

	while (i < n) {
		size_t ls = i;
		while (i < n && p[i] != '\n') { ++i; }
		StringView line = trimWs(StringView(p + ls, i - ls));
		if (i < n) {
			++i; // skip newline
		}

		if (line.empty() || line.data()[0] == '#' || line.data()[0] == ';') {
			continue;
		}

		if (line.data()[0] == '[') {
			flush();
			inSub = line.starts_with("[submodule");
			if (inSub) {
				// extract the quoted name, if present
				size_t q1 = 0;
				while (q1 < line.size() && line.data()[q1] != '"') { ++q1; }
				if (q1 < line.size()) {
					size_t q2 = q1 + 1;
					while (q2 < line.size() && line.data()[q2] != '"') { ++q2; }
					if (q2 < line.size()) {
						cur.name = StringView(line.data() + q1 + 1, q2 - q1 - 1)
										   .str<memory::StandartInterface>();
					}
				}
			}
			continue;
		}

		if (!inSub) {
			continue;
		}

		// key = value
		size_t eq = 0;
		while (eq < line.size() && line.data()[eq] != '=') { ++eq; }
		if (eq >= line.size()) {
			continue;
		}
		StringView key = trimWs(StringView(line.data(), eq));
		StringView val = trimWs(StringView(line.data() + eq + 1, line.size() - eq - 1));
		if (key == "path") {
			cur.path = val.str<memory::StandartInterface>();
		} else if (key == "url") {
			cur.url = val.str<memory::StandartInterface>();
		}
	}
	flush();

	return out;
}

// --- url helpers -----------------------------------------------------------

bool isHttpUrl(StringView s) { return s.starts_with("http://") || s.starts_with("https://"); }

bool sameHost(StringView a, StringView b) {
	sprt::UrlView ua, ub;
	ua.parse(a);
	ub.parse(b);
	return iequal(ua.scheme, ub.scheme) && iequal(ua.host, ub.host) && ua.port == ub.port;
}

String resolveSubmoduleUrl(StringView baseUrl, StringView subUrl) {
	subUrl = trimWs(subUrl);

	if (isHttpUrl(subUrl)) {
		return subUrl.str<memory::StandartInterface>();
	}
	if (!(subUrl.starts_with("../") || subUrl.starts_with("./"))) {
		return subUrl.str<memory::StandartInterface>(); // leave as-is; caller filters non-http
	}

	sprt::UrlView u;
	u.parse(baseUrl);

	// split base path into segments
	Vector<StringView> segs;
	const char *pp = u.path.data();
	size_t pn = u.path.size();
	size_t i = 0;
	while (i < pn) {
		while (i < pn && pp[i] == '/') { ++i; }
		size_t s = i;
		while (i < pn && pp[i] != '/') { ++i; }
		if (i > s) {
			segs.emplace_back(StringView(pp + s, i - s));
		}
	}

	// consume leading ./ and ../ from the relative ref
	StringView ref = subUrl;
	for (;;) {
		if (ref.starts_with("../")) {
			ref = StringView(ref.data() + 3, ref.size() - 3);
			if (!segs.empty()) {
				segs.pop_back();
			}
		} else if (ref.starts_with("./")) {
			ref = StringView(ref.data() + 2, ref.size() - 2);
		} else {
			break;
		}
	}

	String res;
	res.append(u.scheme.data(), u.scheme.size());
	res += "://";
	res.append(u.host.data(), u.host.size());
	if (!u.port.empty()) {
		res += ":";
		res.append(u.port.data(), u.port.size());
	}
	for (auto &seg : segs) {
		res += "/";
		res.append(seg.data(), seg.size());
	}
	res += "/";
	res.append(ref.data(), ref.size());
	return res;
}

} // namespace stappler::git
