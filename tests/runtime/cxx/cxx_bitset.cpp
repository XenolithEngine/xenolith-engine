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

#include <sprt/cxx/bitset>
#include <sprt/runtime/stream.h>

namespace sprt {

void performBitsetTests() {
	{
		sprt::bitset<67> bs;
		bs.set(65);

		sprt::cout << "bitset: find_first_set: "
				   << ((bs.find_first_set() == 65) ? "Success" : "Failure") << "\n";

		bs.set();

		sprt::cout << "bitset: find_first_set (all set): "
				   << ((bs.find_first_set() == 0) ? "Success" : "Failure") << "\n";

		bs.reset();

		sprt::cout << "bitset: find_first_set (empty): "
				   << ((bs.find_first_set() == 67) ? "Success" : "Failure") << "\n";

		bs.set();
		bs.reset(65);

		sprt::cout << "bitset: find_first_not_set: "
				   << ((bs.find_first_not_set() == 65) ? "Success" : "Failure") << "\n";

		bs.reset();

		sprt::cout << "bitset: find_first_not_set (empty): "
				   << ((bs.find_first_not_set() == 0) ? "Success" : "Failure") << "\n";

		bs.set();

		sprt::cout << "bitset: find_first_not_set (all set): "
				   << ((bs.find_first_not_set() == 67) ? "Success" : "Failure") << "\n";
	}

	{
		sprt::bitset<27> bs;
		bs.set(20);

		sprt::cout << "bitset: find_first_set: "
				   << ((bs.find_first_set() == 20) ? "Success" : "Failure") << "\n";

		bs.set();

		sprt::cout << "bitset: find_first_set (all set): "
				   << ((bs.find_first_set() == 0) ? "Success" : "Failure") << "\n";

		bs.reset();

		sprt::cout << "bitset: find_first_set (empty): "
				   << ((bs.find_first_set() == bs.size()) ? "Success" : "Failure") << "\n";

		bs.set();
		bs.reset(20);

		sprt::cout << "bitset: find_first_not_set: "
				   << ((bs.find_first_not_set() == 20) ? "Success" : "Failure") << "\n";

		bs.reset();

		sprt::cout << "bitset: find_first_not_set (empty): "
				   << ((bs.find_first_not_set() == 0) ? "Success" : "Failure") << "\n";

		bs.set();

		sprt::cout << "bitset: find_first_not_set (all set): "
				   << ((bs.find_first_not_set() == bs.size()) ? "Success" : "Failure") << "\n";
	}
}

} // namespace sprt
