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

// xlkdl data: a document between KDL and JSON by the mapping of EncodeFormat::Kdl.

#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPData.h"

#include <sprt/runtime/stream.h>

#include "XlkdlAbi.h"

namespace STAPPLER_VERSIONIZED stappler::xlkdl {

using Interface = memory::StandardInterface;

int runData(int argc, const char *argv[]) {
	bool toKdl = argc == 2 && StringView(argv[0]) == "--kdl";
	if (argc != 1 && !toKdl) {
		sprt::cerr << "usage: xlkdl data <file.kdl>         the document as JSON (JsonGit)\n"
				   << "       xlkdl data --kdl <file.json>  the document as KDL\n";
		return 1;
	}

	StringView file(argv[argc - 1]);
	if (!filesystem::exists(FileInfo{file})) {
		sprt::cerr << file << ": no such file\n";
		return 1;
	}
	auto text = filesystem::readTextFile<Interface>(FileInfo{file});

	if (toKdl) {
		auto value = data::json::read<Interface>(StringView(text));
		auto out = data::kdl::write<Interface>(value);
		if (!value.isDictionary()
				|| (out.empty() && !value.asDict().empty())) {
			sprt::cerr << file << ": not a document of KDL: the root is not an object, or a string "
								  "is not UTF-8\n";
			return 1;
		}
		sprt::cout << out;
		return 0;
	}

	data::kdl::ParseError err;
	mem_std::Value value;
	if (data::kdl::read<Interface>(StringView(text), value, &err) != Status::Ok) {
		sprt::cerr << file << ":" << err.location.line << ":" << err.location.column << ": "
				   << err.message << "\n";
		return 1;
	}
	sprt::cout << data::toString<Interface>(value, data::EncodeFormat::JsonGit);
	return 0;
}

} // namespace stappler::xlkdl
