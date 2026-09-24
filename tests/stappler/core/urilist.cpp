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


// text/uri-list as a drop from another application carries it: the list is split into URIs, and a
// `file:` URI into the local path it names.

#include "SPCommon.h"
#include "SPUrl.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

static memory::StandardInterface::VectorType<memory::StandardInterface::StringType> readUris(
		StringView list) {
	memory::StandardInterface::VectorType<memory::StandardInterface::StringType> ret;
	UrlView::readUriList(list,
			[&](StringView uri) { ret.emplace_back(uri.str<memory::StandardInterface>()); });
	return ret;
}

void performUriListTests() {
	sprt::cout << "\n== stappler core uri-list tests ==\n";

	{
		// CRLF is what RFC 2483 asks for; comments and blank lines are not entries
		auto uris = readUris("# comment\r\nfile:///tmp/a.txt\r\n\r\nfile:///tmp/b.txt\r\n");
		check(uris.size() == 2, "uri-list: two entries with CRLF");
		if (uris.size() == 2) {
			checkEq(StringView(uris[0]), StringView("file:///tmp/a.txt"), "uri-list: first entry");
			checkEq(StringView(uris[1]), StringView("file:///tmp/b.txt"), "uri-list: second entry");
		}
	}

	{
		// Some sources end lines with a bare LF, and some omit the last terminator
		auto uris = readUris("file:///tmp/a.txt\nfile:///tmp/b.txt");
		check(uris.size() == 2, "uri-list: bare LF and no final terminator");
	}

	checkEq(StringView(UrlView::readFilePath<mem_std::Interface>("file:///tmp/a%20b.txt")),
			StringView("/tmp/a b.txt"), "file uri: percent-decoded space");

	checkEq(StringView(UrlView::readFilePath<mem_std::Interface>(
					"file:///tmp/%D0%BF%D1%80%D0%B8%D0%B2%D0%B5%D1%82.png")),
			StringView("/tmp/привет.png"), "file uri: percent-decoded UTF-8");

	checkEq(StringView(UrlView::readFilePath<mem_std::Interface>("file://localhost/etc/hosts")),
			StringView("/etc/hosts"), "file uri: localhost is this machine");

	checkEq(StringView(UrlView::readFilePath<mem_std::Interface>("FILE:///tmp/x")),
			StringView("/tmp/x"), "file uri: the scheme is case-insensitive");

	check(UrlView::readFilePath<mem_std::Interface>("file://server/share/x").empty(),
			"file uri: a remote host has no local path");

	check(UrlView::readFilePath<mem_std::Interface>("https://example.com/x").empty(),
			"file uri: another scheme has no local path");

	checkEq(StringView(UrlView::readFilePath<mem_std::Interface>("file:///C:/Users/a%20b")),
			StringView("/c/Users/a b"), "file uri: a Windows drive becomes the runtime form");

	checkEq(StringView(UrlView::readFilePath<mem_std::Interface>("file:///tmp/a?x=1#frag")),
			StringView("/tmp/a"), "file uri: query and fragment are not part of the path");
}

} // namespace STAPPLER_VERSIONIZED stappler
