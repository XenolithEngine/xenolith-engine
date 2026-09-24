/**
Copyright (c) 2016-2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPUrl.h"
#include "SPString.h"
#include "SPSharedModule.h"

namespace STAPPLER_VERSIONIZED stappler {

template <typename Vector>
auto _parsePath(StringView str, Vector &ret) {
	StringView s(str);
	do {
		if (s.is('/')) {
			s++;
		}
		auto path = s.readUntil<StringView::Chars<'/', '?', ';', '&', '#'>>();
		if (path == "..") {
			if (!ret.empty()) {
				ret.pop_back();
			}
		} else if (path == ".") {
			// skip this component
		} else {
			if (!path.empty()) {
				ret.push_back(path);
			}
		}
	} while (!s.empty() && s.is('/'));
}

void UrlView::readUriList(StringView list, const Callback<void(StringView)> &cb) {
	StringView r(list);
	while (!r.empty()) {
		auto line = r.readUntil<StringView::Chars<'\r', '\n'>>();
		r.skipChars<StringView::Chars<'\r', '\n'>>();

		line.trimChars<StringView::WhiteSpace>();
		if (!line.empty() && !line.is('#')) {
			cb(line);
		}
	}
}

template <typename Interface>
static auto UrlView_readFilePath(StringView uri) -> typename Interface::StringType {
	StringView r(uri);
	r.trimChars<StringView::WhiteSpace>();

	static constexpr auto Scheme = StringView("file:");
	if (r.size() < Scheme.size()) {
		return typename Interface::StringType();
	}
	for (size_t i = 0; i < Scheme.size(); ++i) {
		auto c = r[i];
		if (c >= 'A' && c <= 'Z') {
			c = c - 'A' + 'a';
		}
		if (c != Scheme[i]) {
			return typename Interface::StringType();
		}
	}
	r += Scheme.size();

	if (r.starts_with("//")) {
		r += 2;
		auto host = r.readUntil<StringView::Chars<'/'>>();
		if (!host.empty() && host != "localhost") {
			return typename Interface::StringType();
		}
	}

	if (!r.is('/')) {
		return typename Interface::StringType();
	}

	auto path = string::urldecode<Interface>(r.readUntil<StringView::Chars<'?', '#'>>());

	// "/C:/dir" -> "/c/dir"
	if (path.size() >= 3 && path[0] == '/' && path[2] == ':'
			&& ((path[1] >= 'A' && path[1] <= 'Z') || (path[1] >= 'a' && path[1] <= 'z'))
			&& (path.size() == 3 || path[3] == '/')) {
		auto drive = path[1];
		if (drive >= 'A' && drive <= 'Z') {
			drive = drive - 'A' + 'a';
		}
		typename Interface::StringType ret;
		ret.push_back('/');
		ret.push_back(drive);
		ret.append(path.data() + 3, path.size() - 3);
		return ret;
	}

	return path;
}

template <>
auto UrlView::readFilePath<mem_std::Interface>(StringView uri) -> mem_std::Interface::StringType {
	return UrlView_readFilePath<mem_std::Interface>(uri);
}

template <>
auto UrlView::readFilePath<memory::PoolInterface>(StringView uri)
		-> memory::PoolInterface::StringType {
	return UrlView_readFilePath<memory::PoolInterface>(uri);
}

template <>
auto UrlView::parsePath<mem_std::Interface>(StringView str)
		-> mem_std::Interface::VectorType<StringView> {
	mem_std::Interface::VectorType<StringView> ret;
	_parsePath(str, ret);
	return ret;
}

template <>
auto UrlView::parsePath<memory::PoolInterface>(StringView str)
		-> memory::PoolInterface::VectorType<StringView> {
	memory::PoolInterface::VectorType<StringView> ret;
	_parsePath(str, ret);
	return ret;
}

#if MODULE_STAPPLER_DATA

template <>
auto UrlView::parseArgs<memory::PoolInterface>(StringView str, size_t maxVarSize)
		-> data::ValueTemplate<memory::PoolInterface> {
	if (str.empty()) {
		return data::ValueTemplate<memory::PoolInterface>();
	}
	StringView r(str);
	if (r.front() == '?' || r.front() == '&' || r.front() == ';') {
		++r;
	}

	auto fn = SharedModule::acquireTypedSymbol<
			decltype(&data::readUrlencoded<memory::PoolInterface>)>(
			buildconfig::MODULE_STAPPLER_DATA_NAME, "readUrlencoded");
	if (!fn) {
		log::source().error("UrlView",
				"Module MODULE_STAPPLER_DATA declared, but not available in runtime");
		return data::ValueTemplate<memory::PoolInterface>();
	}
	return fn(r, maxVarSize);
}

template <>
auto UrlView::parseArgs<mem_std::Interface>(StringView str, size_t maxVarSize)
		-> data::ValueTemplate<mem_std::Interface> {
	if (str.empty()) {
		return data::ValueTemplate<mem_std::Interface>();
	}
	StringView r(str);
	if (r.front() == '?' || r.front() == '&' || r.front() == ';') {
		++r;
	}

	auto fn =
			SharedModule::acquireTypedSymbol< decltype(&data::readUrlencoded<mem_std::Interface>)>(
					buildconfig::MODULE_STAPPLER_DATA_NAME, "readUrlencoded");
	if (!fn) {
		log::source().error("UrlView",
				"Module MODULE_STAPPLER_DATA declared, but not available in runtime");
		return data::ValueTemplate<mem_std::Interface>();
	}
	return fn(r, maxVarSize);
}

#endif


} // namespace STAPPLER_VERSIONIZED stappler
