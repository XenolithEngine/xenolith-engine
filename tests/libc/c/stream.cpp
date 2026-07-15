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

// <sstream> / <fstream> / <ostream> / <istream> skeleton smoke test. On the
// freestanding x86_64-pc-windows-msvc build these are the sprt-backed STL stream
// headers; on the Linux host they are the system headers. compare.sh diffs the two,
// so this exercises only the parts of the skeleton whose behaviour is fully specified
// and therefore identical to the reference: the character-level unformatted transfer
// (put/write/get/read), str() round-tripping, the endl/ends manipulators, and binary
// file I/O. The numeric inserters/extractors are intentionally not covered — they are
// declared-only stubs at the skeleton stage. Files use a relative path and binary mode
// so the byte streams match on both targets.

#include <stdio.h>
#include <string.h>

#include <sstream>
#include <fstream>
#include <ostream>
#include <istream>
#include <string>

namespace sprt::test {

void performStreamTest() {
	// ---- basic_ostringstream: put / write / char+string inserters / str() ----
	std::ostringstream oss;
	oss.put('H');
	oss.put('i');
	oss.write("!!", 2);
	oss << "abc"; // operator<<(ostream&, const char*)
	oss << '.';   // operator<<(ostream&, char)
	std::string s = oss.str();
	printf("oss: [%s] len=%d good=%d\n", s.c_str(), (int)s.size(), (int)oss.good());

	// str(seed) then read the seed back (no writes: out-mode put position is skeleton)
	std::ostringstream seeded("seed");
	printf("seeded: [%s]\n", seeded.str().c_str());

	// ---- basic_istringstream: get / read / gcount / eof+fail on exhaustion ----
	std::istringstream iss("wxyz");
	int c0 = iss.get(); // 'w'
	char cc = 0;
	iss.get(cc); // 'x'
	char buf[3] = {0, 0, 0};
	iss.read(buf, 2);           // "yz"
	int g = (int)iss.gcount();  // 2
	int atEof = iss.get();      // eof -> eofbit | failbit
	printf("iss: c0=%c cc=%c read=[%s] gcount=%d eofget=%d eof=%d fail=%d\n", c0, cc, buf, g,
			atEof, (int)iss.eof(), (int)iss.fail());

	// ---- peek / unget round trip ----
	std::istringstream iss2("AB");
	int p0 = iss2.peek(); // 'A' (no advance)
	int a0 = iss2.get();  // 'A'
	iss2.unget();         // back to 'A'
	int a1 = iss2.get();  // 'A' again
	printf("peek: p0=%c a0=%c a1=%c\n", p0, a0, a1);

	// ---- basic_stringstream: write then read back ----
	std::stringstream ss;
	ss.put('R');
	ss.put('T');
	char r0 = 0, r1 = 0;
	ss.get(r0);
	ss.get(r1);
	printf("ss: r0=%c r1=%c\n", r0, r1);

	// ---- manipulators: endl (writes '\n' + flush), ends (writes '\0') ----
	std::ostringstream om;
	om << "x" << std::endl << "y" << std::ends;
	std::string ms = om.str();
	printf("manip: len=%d nl=%d nul=%d\n", (int)ms.size(), (int)(ms[1] == '\n'),
			(int)(ms[3] == '\0'));

	// ---- basic_ofstream / basic_ifstream over a relative binary file ----
	const char *path = "sprt_stream.tmp";
	remove(path); // clean slate
	{
		std::ofstream ofs(path, std::ios_base::out | std::ios_base::binary);
		printf("ofs: open=%d\n", (int)ofs.is_open());
		ofs.write("file-data\n", 10);
		ofs.put('Z');
		ofs.close();
	}
	{
		std::ifstream ifs(path, std::ios_base::in | std::ios_base::binary);
		printf("ifs: open=%d\n", (int)ifs.is_open());
		char fb[16];
		memset(fb, 0, sizeof(fb));
		ifs.read(fb, 11);
		printf("ifs: gcount=%d [%s]\n", (int)ifs.gcount(), fb);
		ifs.close();
	}
	remove(path);
}

} // namespace sprt::test
