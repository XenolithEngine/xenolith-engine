/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// xlkdl: KDL documents from the command line, and C headers generated from ABI descriptions.

#include "SPCommon.h"
#include "XlkdlAbi.h"

#include <sprt/runtime/stream.h>

int main(int argc, const char *argv[]) {
	using namespace stappler;
	return perform_main(argc, argv, [&]() -> int {
		StringView command = argc > 1 ? StringView(argv[1]) : StringView();
		if (command == "dump") {
			return xlkdl::runDump(argc - 2, argv + 2);
		} else if (command == "fmt") {
			return xlkdl::runFormat(argc - 2, argv + 2);
		} else if (command == "abi") {
			return xlkdl::runAbi(argc - 2, argv + 2);
		}
		sprt::cerr << "usage: xlkdl dump <file.kdl>\n"
				   << "       xlkdl fmt <file.kdl>\n"
				   << "       xlkdl abi --map <map.kdl> --out <dir> [--check] [--inline] [<header>...]\n";
		return 1;
	});
}
