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

#include <sprt/runtime/stream.h>

namespace sprt {

void performUnicodeTests() {
	StringView test1 = "Тест";
	StringView test2 = "ТЕСТ";
	StringView test3 = "ТЕСТ";

	WideStringView wtest1 = u"Тест1";
	WideStringView wtest2 = u"ТЕСТ1";
	//WideStringView wtest3 = u"тест1";

	unicode::toupper([](StringView str) {
		sprt::cout << str << "\n"; //
	}, test1);
	unicode::tolower([](StringView str) {
		sprt::cout << str << "\n"; //
	}, test1);
	unicode::totitle([](StringView str) {
		sprt::cout << str << "\n"; //
	}, test1);

	unicode::toupper([](StringView str) {
		sprt::cout << str << "\n"; //
	}, test2);
	unicode::tolower([](StringView str) {
		sprt::cout << str << "\n"; //
	}, test2);
	unicode::totitle([](StringView str) {
		sprt::cout << str << "\n"; //
	}, test2);

	sprt::cout << "StringUnicodeCaseComparator: "
			   << test1.equals<sprt::StringUnicodeCaseComparator>(test2) << "\n";
	sprt::cout << "StringCaseComparator: " << test1.equals<sprt::StringCaseComparator>(test2)
			   << "\n";

	sprt::cout << "StringUnicodeCaseComparator: "
			   << wtest1.equals<sprt::StringUnicodeCaseComparator>(wtest2) << "\n";
	sprt::cout << "StringCaseComparator: " << wtest1.equals<sprt::StringCaseComparator>(wtest2)
			   << "\n";

	sprt::cout << (test3 < test1) << " " << (test3 > test1) << '\n';
}

} // namespace sprt
