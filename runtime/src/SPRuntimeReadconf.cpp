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

#include <sprt/runtime/utils/readconf.h>

namespace sprt {

Status readconf(const Callback<bool(StringView, StringView, StringView)> &cb, StringView data,
		ConfFeatures features) {
	StringView currentSection;
	StringView currentString;
	while (!data.empty()) {
		currentString = data.readUntil<StringView::Chars<'\n', '\r'>>();
		currentString.trimChars<StringView::WhiteSpace>();

		if (currentString.is('[')) {
			if (currentString.ends_with(']')) {
				currentSection = currentString;
			} else {
				// section name is not terminated with ]
				if (!hasFlag(features, ConfFeatures::ContinueOnError)) {
					return Status::ErrorInvalidArguemnt;
				}
			}
		} else {
			auto key = currentString.readUntil<StringView::WhiteSpace, StringView::Chars<'='>>();
			if (key.empty()) {
				// key is not defined
				if (!hasFlag(features, ConfFeatures::ContinueOnError)) {
					return Status::ErrorInvalidArguemnt;
				}
			} else {
				currentString.skipChars<StringView::WhiteSpace>();
				if (currentString.is('=')) {
					++currentString;
					currentString.skipChars<StringView::WhiteSpace>();
					if (!cb(currentSection, key, currentString)) {
						return Status::Declined;
					}
				} else if (currentString.empty()) {
					if (!cb(currentSection, key, StringView())) {
						return Status::Declined;
					}
				} else {
					if (!hasFlag(features, ConfFeatures::ContinueOnError)) {
						return Status::ErrorInvalidArguemnt;
					}
				}
			}
		}

		data.skipChars<StringView::Chars<'\n', '\r'>>();
	}
	return Status::Ok;
}

} // namespace sprt
