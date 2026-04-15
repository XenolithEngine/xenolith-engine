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

#include <sprt/cxx/variant>
#include <sprt/runtime/stream.h>

namespace sprt {

void performVariantTests() {
	sprt::cout << "\n== variant tests ==\n";

	auto fn = [](auto &&arg) {
		using T = sprt::decay_t<decltype(arg)>;
		if constexpr (sprt::is_same_v<T, int>) {
			sprt::cout << "Int: " << arg << "\n";
		} else if constexpr (sprt::is_same_v<T, float>) {
			sprt::cout << "Float: " << arg << "\n";
		} else if constexpr (sprt::is_same_v<T, const char *>) {
			sprt::cout << "String: " << arg << "\n";
		} else if constexpr (sprt::is_same_v< T, variant_empty>) {
			sprt::cout << "Empty\n";
		}
	};

	// Basic tests
	{
		variant<monostate, int, float, const char *> v1(12);
		variant<monostate, int, float, const char *> v2(23.45f);
		variant<monostate, int, float, const char *> v3("string");

		visit(fn, v1);
		visit(fn, v2);
		visit(fn, v3);
	}

	{
		variant<monostate, int, float, const char *> v1("copy test");
		auto v2 = v1;

		visit(fn, v1);
		visit(fn, v2);
	}

	{
		variant<monostate, int, float, const char *> v1("move test");
		auto v2 = sprt::move(v1);

		visit(fn, v1);
		visit(fn, v2);
	}
}

} // namespace sprt
