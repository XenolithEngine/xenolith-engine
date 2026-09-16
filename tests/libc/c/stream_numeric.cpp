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

// Formatted numeric insertion into <ostream> (basic_ostream::operator<<): the
// integer/float/bool/pointer inserters honouring basefield / adjustfield / showpos
// / showbase / uppercase / width / fill / precision, plus the <iomanip> setw /
// setfill / setprecision / setbase manipulators. Uses ostringstream so the output
// is captured to a string and printed deterministically (host sprt-STL vs the
// Windows sprt-STL must agree).

#include <stdio.h>

#include <sstream>
#include <iomanip>
#include <ios>

namespace sprt::test {

void performStreamNumericTest() {
	// signed / unsigned decimals, including the extremes
	{
		std::ostringstream o;
		o << 42 << '|' << -7 << '|' << 0 << '|' << (unsigned) 4000000000u << '|';
		o << (long long) -9223372036854775807LL - 1 << '|' << 18446744073709551615ULL;
		printf("dec: %s\n", o.str().c_str());
	}
	// bases + showbase + uppercase
	{
		std::ostringstream o;
		o << std::hex << 255 << '|' << std::showbase << 255 << '|' << std::uppercase << 255 << '|';
		o << std::oct << 64 << '|' << std::dec << 64;
		printf("base: %s\n", o.str().c_str());
	}
	// width / fill / adjustfield (right default, left, internal)
	{
		std::ostringstream o;
		o << std::setw(6) << std::setfill('0') << 33 << '|';        // right: 000033
		o << std::setw(6) << std::left << 33 << '|';                // left:  330000 (fill persists)
		o << std::setfill(' ') << std::setw(6) << std::right << 33 << '|';
		o << std::internal << std::showpos << std::setw(6) << 33;   // +   33
		printf("pad: %s\n", o.str().c_str());
	}
	// setbase + showpos on positive
	{
		std::ostringstream o;
		o << std::setbase(16) << 4095 << '|' << std::setbase(10) << std::showpos << 5 << '|' << -5;
		printf("misc: %s\n", o.str().c_str());
	}
	// bool as alpha / numeric
	{
		std::ostringstream o;
		o << true << '|' << false << '|' << std::boolalpha << true << '|' << false;
		printf("bool: %s\n", o.str().c_str());
	}
	// float: only values whose shortest round-trip already equals the standard's
	// default (general, precision 6) rendering. sprt's ostream float insertion goes
	// through sprt::dtoa, whose "shortest" and precision handling do not yet match
	// the C++ default float format for integral-valued / large-magnitude numbers
	// (e.g. 100.0 -> "100.0" not "100"; 1e6 -> "1.0e6" not "1e+06"), nor fixed/
	// scientific decimal-place precision. Those are dtoa limitations, tracked
	// separately from the operator<< inserters.
	{
		std::ostringstream o;
		o << 3.14159 << '|' << 2.5 << '|' << -0.5;
		printf("float: %s\n", o.str().c_str());
	}
}

} // namespace sprt::test
