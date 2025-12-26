/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_DOCUMENT_SPDOCPARSER_H_
#define CORE_DOCUMENT_SPDOCPARSER_H_

#include "SPDocStyle.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::document::parser {

using namespace mem_pool;

using StringReader = StringViewUtf8;

SP_PUBLIC String readHtmlTagName(StringReader &);
SP_PUBLIC String readHtmlTagParamName(StringReader &);
SP_PUBLIC String readHtmlTagParamValue(StringReader &);

SP_PUBLIC bool readStyleMargin(const StringView &, Metric &top, Metric &right, Metric &bottom,
		Metric &left);
SP_PUBLIC bool readStyleMargin(StringReader &, Metric &top, Metric &right, Metric &bottom,
		Metric &left);
SP_PUBLIC bool readStyleMetric(const StringView &, Metric &value, bool resolutionMetric = false,
		bool allowEmptyMetric = false);
SP_PUBLIC bool readStyleMetric(StringReader &, Metric &value, bool resolutionMetric = false,
		bool allowEmptyMetric = false);

struct SP_PUBLIC StyleReaderInfo {
	Callback<void(StringId, const StringView &)> addString;
	Callback<MediaQueryId(MediaQuery &&)> addQuery;
	Callback<void(StringView, const StyleList::StyleVec &, MediaQueryId)> addStyle;
};

SP_PUBLIC void readStyle(StyleReaderInfo &, StringReader &);

} // namespace stappler::document::parser

#endif /* CORE_DOCUMENT_SPDOCPARSER_H_ */
