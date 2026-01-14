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
#include "SPStringView.h"
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
				ret.push_back(str);
			}
		}
	} while (!s.empty() && s.is('/'));
}

template <>
auto UrlView::parsePath<memory::StandartInterface>(StringView str)
		-> memory::StandartInterface::VectorType<StringView> {
	memory::StandartInterface::VectorType<StringView> ret;
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
		data::ValueTemplate<memory::PoolInterface>();
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
	}
	return fn(str, maxVarSize);
}

template <>
auto UrlView::parseArgs<memory::StandartInterface>(StringView str, size_t maxVarSize)
		-> data::ValueTemplate<memory::StandartInterface> {
	if (str.empty()) {
		data::ValueTemplate<memory::StandartInterface>();
	}
	StringView r(str);
	if (r.front() == '?' || r.front() == '&' || r.front() == ';') {
		++r;
	}

	auto fn = SharedModule::acquireTypedSymbol<
			decltype(&data::readUrlencoded<memory::StandartInterface>)>(
			buildconfig::MODULE_STAPPLER_DATA_NAME, "readUrlencoded");
	if (!fn) {
		log::source().error("UrlView",
				"Module MODULE_STAPPLER_DATA declared, but not available in runtime");
	}
	return fn(str, maxVarSize);
}

#endif


} // namespace STAPPLER_VERSIONIZED stappler
