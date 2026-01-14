/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_URLVIEW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_URLVIEW_H_

#include <sprt/runtime/stringview.h>

namespace sprt {

struct SPRT_API UrlView {
	enum class UrlToken {
		Scheme,
		User,
		Password,
		Host,
		Port,
		Path,
		Query,
		Fragment,
		Blank,
	};

	StringView scheme;
	StringView user;
	StringView password;
	StringView host;
	StringView port;
	StringView path;
	StringView query;
	StringView fragment;

	StringView url;

	static bool validateScheme(const StringView &r);
	static bool validateHost(const StringView &r);
	static bool validateUserOrPassword(const StringView &r);

	static bool validateHost(StringView &r);
	static bool parseUrl(StringView &s, const callback<void(StringView, UrlView::UrlToken)> &cb);

	UrlView();
	UrlView(StringView);

	void clear();

	bool parse(const StringView &);
	bool parse(StringView &); // leaves unparsed part of the string in view

	void get(const callback<void(StringView)> &) const;

	bool isEmail() const;
	bool isPath() const;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_URLVIEW_H_
