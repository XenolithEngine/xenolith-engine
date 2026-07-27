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

#ifndef STAPPLER_GIT_SPGITSUBMODULE_H_
#define STAPPLER_GIT_SPGITSUBMODULE_H_

#include "SPGit.h"

// Submodule support helpers: parse the `.gitmodules` config, resolve relative
// submodule URLs against the superproject URL, and URL host comparison. All pure
// (no network / no fs).

namespace STAPPLER_VERSIONIZED stappler::git {

struct SubmoduleSpec {
	String name;
	String path;
	String url;
};

// Parse a `.gitmodules` file body (git-config/INI subset) into submodule specs.
SP_PUBLIC Vector<SubmoduleSpec> parseGitmodules(BytesView);

// Resolve a submodule URL against the superproject base URL. Absolute http(s)
// URLs pass through; `../` / `./` relative URLs are resolved git-style (each
// `../` strips one trailing path segment of the base). Non-http, non-relative
// URLs (e.g. scp-like `git@host:path`) are returned unchanged.
SP_PUBLIC String resolveSubmoduleUrl(StringView baseUrl, StringView subUrl);

// True if `a` and `b` share scheme + host + port.
SP_PUBLIC bool sameHost(StringView a, StringView b);

// True if the URL uses the http or https scheme.
SP_PUBLIC bool isHttpUrl(StringView);

} // namespace stappler::git

#endif /* STAPPLER_GIT_SPGITSUBMODULE_H_ */
