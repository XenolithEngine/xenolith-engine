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

#include "SPCommon.h"
#include "SPData.h"
#include "SPFilesystem.h"
#include "SPCommandLineParser.h"

#include <sprt/runtime/stream.h>

#include "XlkdlAbi.h"
#include "XlkdlAbiLoad.cc"
#include "XlkdlApiLoad.cc"
#include "XlkdlAbiEmit.cc"
#include "XlkdlApiEmit.cc"
#include "XlkdlAbiFind.cc"

namespace STAPPLER_VERSIONIZED stappler::xlkdl {

namespace {

bool readDocument(StringView file, KdlDocument &doc) {
	if (!filesystem::exists(FileInfo{file})) {
		sprt::cerr << file << ": no such file\n";
		return false;
	}
	auto text = filesystem::readTextFile<memory::StandardInterface>(FileInfo{file});
	kdl::ParseError err;
	if (kdl::read(text, doc, &err) != Status::Ok) {
		sprt::cerr << file << ":" << err.location.line << ":" << err.location.column << ": "
				   << err.message << "\n";
		return false;
	}
	return true;
}

mem_std::Value toValue(const KdlValue &value) {
	mem_std::Value out;
	switch (value.type) {
	case kdl::ValueType::Null: break;
	case kdl::ValueType::Bool: out.setBool(value.boolean); break;
	case kdl::ValueType::String: out.setString(value.getString()); break;
	case kdl::ValueType::Integer:
		if (auto i = value.getInteger()) {
			out.setInteger(i.get());
		} else {
			out.setString(value.text);
		}
		break;
	case kdl::ValueType::Float:
	case kdl::ValueType::Inf:
	case kdl::ValueType::NegInf:
	case kdl::ValueType::NaN: out.setDouble(value.getDouble().get(0.0)); break;
	}
	if (value.hasAnnotation) {
		mem_std::Value typed;
		typed.setString(value.getAnnotation(), "type");
		typed.setValue(sprt::move(out), "value");
		return typed;
	}
	return out;
}

mem_std::Value toValue(const KdlNode &node) {
	mem_std::Value out;
	out.setString(node.name, "name");
	if (node.hasAnnotation) {
		out.setString(node.getAnnotation(), "type");
	}
	if (!node.arguments.empty()) {
		auto &args = out.emplace("args");
		for (auto &arg : node.arguments) { args.addValue(toValue(arg)); }
	}
	if (!node.properties.empty()) {
		auto &props = out.emplace("props");
		for (auto &prop : node.properties) { props.setValue(toValue(prop.value), prop.name); }
	}
	if (!node.children.empty()) {
		auto &children = out.emplace("children");
		for (auto &child : node.children) { children.addValue(toValue(child)); }
	}
	return out;
}

struct AbiArgs {
	String map;
	String out;
	bool check = false;
	bool inlineBlocks = false;
	bool index = false;
	bool help = false;
	Vector<String> headers;
	Vector<String> find;
};

using AbiOption = CommandLineOption<AbiArgs>;

void printAbiUsage(const CommandLineParser<AbiArgs> &parser) {
	sprt::cerr
			<< "usage: xlkdl abi --map <map.kdl> --out <dir> [--check] [--inline] [<header>...]\n"
			<< "       xlkdl abi --map <map.kdl> --find <name> [--find <name>...]\n"
			<< "       xlkdl abi --map <map.kdl> --index\n"
			<< "  writes every header of the map that has a description, or the named ones;\n"
			<< "  --find prints where a name is defined, --index every definition as JSON\n";
	parser.describe([](StringView str) { sprt::cerr << str; });
}

// Writes a generated file, or under check compares it with the file there; false when it differs or
// cannot be written.
bool writeOrCheck(StringView path, StringView text, StringView from, bool check) {
	if (check) {
		auto have = filesystem::exists(FileInfo{path})
				? filesystem::readTextFile<memory::StandardInterface>(FileInfo{path})
				: String();
		if (StringView(have) != text) {
			sprt::cerr << path << ": not what " << from << " generates\n";
			return false;
		}
		return true;
	}
	filesystem::mkdir_recursive(FileInfo{filepath::root(path)});
	if (!filesystem::write(FileInfo{path}, reinterpret_cast<const uint8_t *>(text.data()),
				text.size())) {
		sprt::cerr << path << ": cannot write\n";
		return false;
	}
	return true;
}

} // namespace

int runDump(int argc, const char *argv[]) {
	if (argc != 1) {
		sprt::cerr << "usage: xlkdl dump <file.kdl>\n";
		return 1;
	}
	KdlDocument doc;
	if (!readDocument(StringView(argv[0]), doc)) {
		return 1;
	}
	mem_std::Value out;
	for (auto &node : doc.nodes) { out.addValue(toValue(node)); }
	sprt::cout << data::toString<memory::StandardInterface>(out, true) << "\n";
	return 0;
}

int runFormat(int argc, const char *argv[]) {
	if (argc != 1) {
		sprt::cerr << "usage: xlkdl fmt <file.kdl>\n";
		return 1;
	}
	KdlDocument doc;
	if (!readDocument(StringView(argv[0]), doc)) {
		return 1;
	}
	kdl::write([](StringView str) { sprt::cout << str; }, doc);
	return 0;
}

// The exit code is 1 when a document does not load; otherwise the number of headers that could not
// be written, or under --check the number that are not what would be written.
int runAbi(int argc, const char *argv[]) {
	CommandLineParser<AbiArgs> parser({
		AbiOption{.patterns = {"--map <file>"},
			.description = StringView("The header map: the profile and every header"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.map = args[0].str<memory::StandardInterface>();
		return true;
	}},
		AbiOption{.patterns = {"--out <dir>"},
			.description = StringView("The include root the headers are written under"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.out = args[0].str<memory::StandardInterface>();
		return true;
	}},
		AbiOption{.patterns = {"--check"},
			.description =
					StringView("Write nothing; answer whether <dir> already holds the headers"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.check = true;
		return true;
	}},
		AbiOption{.patterns = {"--inline"},
			.description = StringView("Write the blocks of fragments in place of their includes"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.inlineBlocks = true;
		return true;
	}},
		AbiOption{.patterns = {"--find <name>"},
			.description = StringView("Print every definition of a name, as written or as spelled in C"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.find.emplace_back(args[0].str<memory::StandardInterface>());
		return true;
	}},
		AbiOption{.patterns = {"--index"},
			.description = StringView("Print every definition of the map as JSON"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.index = true;
		return true;
	}},
		AbiOption{.patterns = {"-h", "--help"},
			.description = StringView("This text"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.help = true;
		return true;
	}},
	});

	AbiArgs args;
	if (argc > 0
			&& !parser.parse(args, argc, argv,
					Callback<void(AbiArgs &, StringView)>([](AbiArgs &target, StringView arg) {
		target.headers.emplace_back(arg.str<memory::StandardInterface>());
	}))) {
		printAbiUsage(parser);
		return 1;
	}
	auto query = args.index || !args.find.empty();
	if (args.help || args.map.empty() || (args.out.empty() && !query)) {
		printAbiUsage(parser);
		return args.help ? 0 : 1;
	}

	Map map;
	Diagnostics diag;
	if (!loadMap(args.map, map, diag)) {
		for (auto &it : diag.messages) { sprt::cerr << it << "\n"; }
		return 1;
	}

	if (query) {
		auto print = [](StringView str) { sprt::cout << str; };
		if (args.index) {
			writeIndex(map, print);
		}
		return args.find.empty() ? 0 : findNames(map, args.find, print);
	}

	Vector<const Header *> selected;
	if (args.headers.empty()) {
		for (auto &header : map.headers) {
			if (!header.external && !header.imported) {
				selected.emplace_back(&header);
			}
		}
	} else {
		for (auto &name : args.headers) {
			auto header = map.findHeader(name);
			if (!header || header->external || header->imported) {
				sprt::cerr << name << ": not a generated header of " << args.map << "\n";
				return 1;
			}
			selected.emplace_back(header);
		}
	}

	int failed = 0;
	for (auto header : selected) {
		String text;
		EmitOptions opts;
		opts.inlineBlocks = args.inlineBlocks;
		if (!emitHeader(map, *header, opts, text, diag)) {
			for (auto &it : diag.messages) { sprt::cerr << it << "\n"; }
			return 1;
		}
		auto path = filepath::merge<memory::StandardInterface>(args.out, header->path);
		if (!writeOrCheck(path, text, header->abi, args.check)) {
			++failed;
		}
	}
	return failed;
}

// The API map: its headers under <root>/<include-root>, and the .def, the table and the wrappers of
// each DLL under <root>. The exit code is as for abi.
int runApi(int argc, const char *argv[]) {
	CommandLineParser<AbiArgs> parser({
		AbiOption{.patterns = {"--map <file>"},
			.description = StringView("The API map: the ABI map it loads, its headers and DLLs"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.map = args[0].str<memory::StandardInterface>();
		return true;
	}},
		AbiOption{.patterns = {"--root <dir>"},
			.description = StringView("The root the profile's paths are under (runtime/)"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.out = args[0].str<memory::StandardInterface>();
		return true;
	}},
		AbiOption{.patterns = {"--check"},
			.description = StringView("Write nothing; answer whether <root> already holds the files"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.check = true;
		return true;
	}},
		AbiOption{.patterns = {"--find <name>"},
			.description = StringView("Print every definition of a name, and where a function goes"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.find.emplace_back(args[0].str<memory::StandardInterface>());
		return true;
	}},
		AbiOption{.patterns = {"--index"},
			.description = StringView("Print every definition of the map as JSON"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.index = true;
		return true;
	}},
		AbiOption{.patterns = {"-h", "--help"},
			.description = StringView("This text"),
			.callback = [](AbiArgs &target, StringView, SpanView<StringView>) -> bool {
		target.help = true;
		return true;
	}},
	});

	auto usage = [&]() {
		sprt::cerr << "usage: xlkdl api --map <map.kdl> --root <dir> [--check]\n"
				   << "       xlkdl api --map <map.kdl> --find <name> [--find <name>...]\n"
				   << "       xlkdl api --map <map.kdl> --index\n"
				   << "  writes the API headers and the .def, table and wrappers of every DLL\n";
		parser.describe([](StringView str) { sprt::cerr << str; });
	};
	AbiArgs args;
	if (argc > 0
			&& !parser.parse(args, argc, argv,
					Callback<void(AbiArgs &, StringView)>([](AbiArgs &target, StringView arg) {
		target.headers.emplace_back(arg.str<memory::StandardInterface>());
	}))) {
		usage();
		return 1;
	}
	auto query = args.index || !args.find.empty();
	if (args.help || args.map.empty() || (args.out.empty() && !query) || !args.headers.empty()) {
		usage();
		return args.help ? 0 : 1;
	}

	Map map;
	Diagnostics diag;
	if (!loadMap(args.map, map, diag)) {
		for (auto &it : diag.messages) { sprt::cerr << it << "\n"; }
		return 1;
	}
	if (query) {
		auto print = [](StringView str) { sprt::cout << str; };
		if (args.index) {
			writeIndex(map, print);
		}
		return args.find.empty() ? 0 : findNames(map, args.find, print);
	}

	int failed = 0;
	auto includeRoot = filepath::merge<memory::StandardInterface>(args.out, map.includeRoot);
	for (auto &header : map.headers) {
		if (header.external || header.imported) {
			continue;
		}
		String text;
		if (!emitHeader(map, header, EmitOptions(), text, diag)) {
			for (auto &it : diag.messages) { sprt::cerr << it << "\n"; }
			return 1;
		}
		auto path = filepath::merge<memory::StandardInterface>(includeRoot, header.path);
		auto from = header.umbrellaTarget ? StringView(header.umbrellaTarget->abi) : StringView(header.abi);
		if (!writeOrCheck(path, text, from, args.check)) {
			++failed;
		}
	}
	for (auto &dll : map.dlls) {
		for (auto part : {DllPart::Def, DllPart::Table, DllPart::Stubs}) {
			// A DLL without a table has a .def only.
			if (part != DllPart::Def && dll.table.empty()) {
				continue;
			}
			String text;
			if (!emitDll(map, dll, part, text, diag)) {
				for (auto &it : diag.messages) { sprt::cerr << it << "\n"; }
				return 1;
			}
			auto path = filepath::merge<memory::StandardInterface>(args.out, dllPath(map, dll, part));
			if (!writeOrCheck(path, text, dll.abi, args.check)) {
				++failed;
			}
		}
	}
	return failed;
}

} // namespace stappler::xlkdl
