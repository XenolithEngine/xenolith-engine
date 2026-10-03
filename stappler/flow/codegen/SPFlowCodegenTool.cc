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

// The generator tool's loop: arguments, the registry, and for each file load, build, emit, and either
// write or compare.

#include "SPFlowCodegenTool.h"
#include "SPFlowInterp.h"

#include "SPData.h"
#include "SPFilesystem.h"

#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::flow::codegen {

namespace {

// A sink over a report, in the words the program chose.
class ToolSink final : public DiagSink {
public:
	ToolSink(mem_std::Value *out, void (*write)(mem_std::Value *, const Diag &))
	: _out(out), _write(write ? write : &writeDiagNumbers) { }

	void add(const Diag &d) override { _write(_out, d); }

private:
	mem_std::Value *_out = nullptr;
	void (*_write)(mem_std::Value *, const Diag &) = nullptr;
};

// The file's stem, with anything that is not an identifier character turned into an underscore.
mem_std::String stemOf(StringView path) {
	auto slash = path.rfind('/');
	auto name = slash == sprt::Max<size_t> ? path
										   : StringView(path.data() + slash + 1, path.size() - slash - 1);
	auto dot = name.find('.');
	if (dot != sprt::Max<size_t>) {
		name = StringView(name.data(), dot);
	}
	mem_std::String out;
	for (size_t i = 0; i < name.size(); ++i) {
		auto c = name[i];
		bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		bool digit = c >= '0' && c <= '9';
		out.push_back(alpha || (digit && i > 0) ? c : '_');
	}
	return out.empty() ? mem_std::String("graph") : out;
}

bool writeText(StringView dir, StringView file, StringView text) {
	auto path = mem_std::toString(dir, "/", file);
	return filesystem::write(FileInfo{path}, reinterpret_cast<const uint8_t *>(text.data()),
			text.size());
}

// The file one part of a unit's code goes in: `<name>.gen.cpp` for the first and
// `<name>.gen.<k>.cpp` for the rest (EmitOptions::split).
mem_std::String sourceName(StringView stem, uint32_t part) {
	return part == 0 ? mem_std::toString(stem, ".gen.cpp")
					 : mem_std::toString(stem, ".gen.", part, ".cpp");
}

// Whether the file on disk is already what was generated: byte for byte, since the generator is
// deterministic. A missing file is stale - the directory does not hold what the tool writes.
bool sameOnDisk(StringView dir, StringView file, StringView text) {
	auto path = mem_std::toString(dir, "/", file);
	auto have = filesystem::readTextFile<mem_std::Interface>(FileInfo{path});
	return StringView(have) == text;
}

void printDiag(StringView path, const mem_std::Value &diag) {
	if (diag.isArray() && diag.size() > 0) {
		sprt::cerr << path << ": " << data::toString<mem_std::Interface>(diag, true) << "\n";
	}
}

void printUsage(const ToolOptions &opts, bool full) {
	mem_std::String families;
	for (auto &f : opts.families) {
		if (!families.empty()) {
			families.append(",");
		}
		families.append(f.name.data(), f.name.size());
	}
	sprt::cerr << "usage: " << opts.tool << " --out <dir> [--name <id>] [--ops " << families
			   << "] [--split <n>] [--embed-asset] [--gpu] [--env <name>] [--env-include <header>] "
				  "[--namespace <ns>] [--check] <graph.json>...\n";
	if (!full) {
		return;
	}
	sprt::cerr << "  --name applies to a single file; several files are named after their stems\n"
			   << "  --ops names the operation families a graph may use (default " << opts.defaultOps
			   << ")\n"
			   << "  --split writes the code across <n> sources, for a graph too big for one\n"
			   << "  --embed-asset puts the asset's canonical CBOR in the unit, for tools that want a "
				  "RuntimeGraph beside it\n"
			   << "  --gpu writes the compute shader of every block the graph gives the GPU\n"
			   << "  --env, --env-include name the run environment the unit is compiled for (default "
			   << opts.env << ", " << opts.envInclude << ")\n"
			   << "  --namespace is the namespace the unit is written into (default "
			   << opts.unitNamespace << ")\n"
			   << "  --check writes nothing and answers whether <dir> already holds what would be "
				  "written\n";
}

} // namespace

int runTool(int argc, const char *argv[], const ToolOptions &opts) {
	mem_std::String out;
	mem_std::String name;
	mem_std::String opsList(opts.defaultOps.data(), opts.defaultOps.size());
	mem_std::String env(opts.env.data(), opts.env.size());
	mem_std::String envInclude(opts.envInclude.data(), opts.envInclude.size());
	mem_std::String unitNamespace(opts.unitNamespace.data(), opts.unitNamespace.size());
	bool embedAsset = false;
	bool check = false;
	bool gpu = false;
	uint32_t split = 1;
	mem_std::Vector<mem_std::String> files;

	for (int i = 1; i < argc; ++i) {
		StringView arg(argv[i]);
		auto value = [&]() { return mem_std::toString(StringView(argv[++i])); };
		if (arg == "--out" && i + 1 < argc) {
			out = value();
		} else if (arg == "--name" && i + 1 < argc) {
			name = value();
		} else if (arg == "--ops" && i + 1 < argc) {
			opsList = value();
		} else if (arg == "--env" && i + 1 < argc) {
			env = value();
		} else if (arg == "--env-include" && i + 1 < argc) {
			envInclude = value();
		} else if (arg == "--namespace" && i + 1 < argc) {
			unitNamespace = value();
		} else if (arg == "--embed-asset") {
			embedAsset = true;
		} else if (arg == "--gpu") {
			gpu = true;
		} else if (arg == "--check") {
			check = true;
		} else if (arg == "--split" && i + 1 < argc) {
			auto n = StringView(argv[++i]).readInteger(10).get(0);
			if (n < 1 || n > 1'024) {
				sprt::cerr << opts.tool << ": --split takes a count between 1 and 1024\n";
				return 1;
			}
			split = uint32_t(n);
		} else if (arg.starts_with("--")) {
			printUsage(opts, false);
			return 1;
		} else {
			files.emplace_back(mem_std::toString(arg));
		}
	}
	if (out.empty() || files.empty() || (files.size() > 1 && !name.empty())) {
		printUsage(opts, true);
		return 1;
	}

	// The families a graph may name, and the body headers a unit includes for them - the same set,
	// so a unit written without a family does not make its project link that family's bodies.
	mem_std::Vector<const ToolFamily *> used;
	StringView list(opsList);
	while (!list.empty()) {
		auto word = list.readUntil<StringView::Chars<','>>();
		list.skipChars<StringView::Chars<','>>();
		const ToolFamily *found = nullptr;
		for (auto &f : opts.families) {
			if (f.name == word) {
				found = &f;
			}
		}
		if (!found) {
			sprt::cerr << opts.tool << ": --ops does not know the family \"" << word << "\"\n";
			return 1;
		}
		used.emplace_back(found);
	}

	// An existing directory is where the caller wants the files; only one that is not there yet is
	// made, because mkdir_recursive answers "no" about a directory that already exists.
	filesystem::Stat outStat;
	auto present = filesystem::stat(FileInfo{out}, outStat) && outStat.type == FileType::Dir;
	if (check && !present) {
		sprt::cerr << opts.tool << ": " << out << " does not exist, so it holds nothing to check\n";
		return 1;
	}
	if (!present && !filesystem::mkdir_recursive(FileInfo{out})) {
		sprt::cerr << opts.tool << ": cannot create " << out << "\n";
		return 1;
	}

	// The registry every graph is built against: the families asked for, and the interpreter's own
	// types, which the frame layout is a function of. A graph naming an operation of a family left
	// out is refused by name.
	OpRegistry ops;
	auto ready = ops.init();
	for (auto f : used) {
		ready = ready && f->registerOps && f->registerOps(ops) == Status::Ok;
	}
	ready = ready
			&& InterpreterT<value::PlainArena, NoEnv>::registerCoreTypes(ops.getLocalTypes())
					== Status::Ok;
	if (!ready) {
		sprt::cerr << opts.tool << ": the operation registry could not be prepared\n";
		return 1;
	}

	mem_std::Vector<StringView> bodies;
	for (auto f : used) {
		if (!f->bodyInclude.empty()) {
			bodies.emplace_back(f->bodyInclude);
		}
	}

	int failed = 0;
	for (auto &path : files) {
		auto value = data::readFile<mem_std::Interface>(FileInfo{path});
		if (value.isNull()) {
			sprt::cerr << path << ": cannot be read as a document\n";
			++failed;
			continue;
		}

		GraphAsset asset;
		asset.init();
		mem_std::Value diag;
		ToolSink assetSink(&diag, opts.writeDiag);
		if (asset.load(value, &assetSink) != Status::Ok) {
			printDiag(path, diag);
			++failed;
			continue;
		}

		RuntimeGraph graph;
		graph.init();
		mem_std::Value report;
		ToolSink buildSink(&report, opts.writeDiag);
		if (graph.build(asset, ops, &buildSink) != Status::Ok) {
			printDiag(path, report);
			++failed;
			continue;
		}
		printDiag(path, report); // the warnings and the advice of a graph that built

		EmitOptions options;
		auto unitName = name.empty() ? stemOf(path) : name;
		options.name = unitName;
		options.unitNamespace = unitNamespace;
		options.env = env;
		options.envInclude = envInclude;
		options.bodyIncludes = SpanView<StringView>(bodies.data(), bodies.size());
		options.embedAsset = embedAsset;
		options.split = split;
		options.gpu = gpu;

		Emitted emitted;
		mem_std::Value emitDiag;
		ToolSink emitSink(&emitDiag, opts.writeDiag);
		if (emit(graph, asset, ops, options, emitted, &emitSink) != Status::Ok) {
			printDiag(path, emitDiag);
			++failed;
			continue;
		}

		auto parts = uint32_t(emitted.sources.size());
		if (check) {
			auto stale = !sameOnDisk(out, mem_std::toString(unitName, ".gen.h"), emitted.header);
			for (uint32_t part = 0; part < parts; ++part) {
				if (!sameOnDisk(out, sourceName(unitName, part), emitted.sources[part])) {
					stale = true;
				}
			}
			for (auto &shader : emitted.shaders) {
				if (!sameOnDisk(out, shader.fileName, shader.text)) {
					stale = true;
				}
			}
			if (stale) {
				sprt::cerr << path << ": " << out << "/" << unitName
						   << ".gen.* is not what the generator writes today - regenerate it\n";
				++failed;
			} else {
				sprt::cout << path << " -> " << out << "/" << unitName << ".gen.* is current\n";
			}
			continue;
		}

		auto written = writeText(out, mem_std::toString(unitName, ".gen.h"), emitted.header);
		for (uint32_t part = 0; written && part < parts; ++part) {
			written = writeText(out, sourceName(unitName, part), emitted.sources[part]);
		}
		for (auto &shader : emitted.shaders) {
			written = written && writeText(out, shader.fileName, shader.text);
		}
		if (!written) {
			sprt::cerr << path << ": the unit could not be written to " << out << "\n";
			++failed;
			continue;
		}
		sprt::cout << path << " -> " << out << "/" << unitName << ".gen.{h,cpp}  ("
				   << graph.getNodeCount() << " nodes, " << graph.getScopeCount() << " scopes"
				   << (parts > 1 ? mem_std::toString(", ", parts, " parts") : mem_std::String())
				   << (emitted.shaders.empty()
							  ? mem_std::String()
							  : mem_std::toString(", ", emitted.shaders.size(), " shaders"))
				   << ")\n";
	}
	return failed;
}

} // namespace stappler::flow::codegen
