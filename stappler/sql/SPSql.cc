/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPSql.h"

namespace STAPPLER_VERSIONIZED stappler::sql {

Pair<StringView, bool> encodeComparation(Comparation cmp) {
	bool isTwoArgs = false;
	StringView ret;

	switch (cmp) {
	case Comparation::Invalid: break;
	case Comparation::LessThen: ret = StringView("lt"); break;
	case Comparation::LessOrEqual: ret = StringView("le"); break;
	case Comparation::Equal: ret = StringView("eq"); break;
	case Comparation::NotEqual: ret = StringView("neq"); break;
	case Comparation::GreatherOrEqual: ret = StringView("ge"); break;
	case Comparation::GreatherThen: ret = StringView("gt"); break;
	case Comparation::BetweenValues: ret = StringView("bw"); isTwoArgs = true; break;
	case Comparation::BetweenEquals: ret = StringView("be"); isTwoArgs = true; break;
	case Comparation::NotBetweenValues: ret = StringView("nbw"); isTwoArgs = true; break;
	case Comparation::NotBetweenEquals: ret = StringView("nbe"); isTwoArgs = true; break;
	case Comparation::Includes: ret = StringView("incl"); break;
	case Comparation::Between: ret = StringView("sbw"); isTwoArgs = true; break;
	case Comparation::In: ret = StringView("in"); break;
	case Comparation::NotIn: ret = StringView("notin"); break;
	case Comparation::IsNull: ret = StringView("isnull"); break;
	case Comparation::IsNotNull: ret = StringView("isnotnull"); break;
	case Comparation::Suffix: ret = StringView("suffix"); break;
	case Comparation::Prefix: ret = StringView("prefix"); break;
	case Comparation::WordPart: ret = StringView("wordpart"); break;
	}

	return pair(move(ret), isTwoArgs);
}

Pair<Comparation, bool> decodeComparation(StringView str) {
	bool isTwoArgs = false;
	Comparation ret = Comparation::Invalid;

	if (str == "lt") {
		ret = Comparation::LessThen;
	} else if (str == "le") {
		ret = Comparation::LessOrEqual;
	} else if (str == "eq") {
		ret = Comparation::Equal;
	} else if (str == "neq") {
		ret = Comparation::NotEqual;
	} else if (str == "ge") {
		ret = Comparation::GreatherOrEqual;
	} else if (str == "gt") {
		ret = Comparation::GreatherThen;
	} else if (str == "bw") {
		ret = Comparation::BetweenValues; isTwoArgs = true;
	} else if (str == "be") {
		ret = Comparation::BetweenEquals; isTwoArgs = true;
	} else if (str == "nbw") {
		ret = Comparation::NotBetweenValues; isTwoArgs = true;
	} else if (str == "nbe") {
		ret = Comparation::NotBetweenEquals; isTwoArgs = true;
	} else if (str == "incl") {
		ret = Comparation::Includes;
	} else if (str == "sbw") {
		ret = Comparation::Between; isTwoArgs = true;
	} else if (str == "in") {
		ret = Comparation::In;
	} else if (str == "notin") {
		ret = Comparation::NotIn;
	} else if (str == "isnull") {
		ret = Comparation::IsNull;
	} else if (str == "isnotnull") {
		ret = Comparation::IsNotNull;
	} else if (str == "prefix") {
		ret = Comparation::Prefix;
	} else if (str == "suffix") {
		ret = Comparation::Suffix;
	} else if (str == "wordpart") {
		ret = Comparation::WordPart;
	}

	return pair(ret, isTwoArgs);
}

}
