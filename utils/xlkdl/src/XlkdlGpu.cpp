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
#include "SPFilesystem.h"
#include "SPCommandLineParser.h"

#include <sprt/runtime/stream.h>

#include "XlkdlGpu.h"
#include "XlkdlGpuLoad.cc"
#include "XlkdlGpuLayout.cc"
#include "XlkdlGpuEmit.cc"

namespace STAPPLER_VERSIONIZED stappler::xlkdl {

namespace {

struct GpuArgs {
	String out;
	bool check = false;
	bool help = false;
	Vector<String> schemas;
};

using GpuOption = CommandLineOption<GpuArgs>;

void printGpuUsage(const CommandLineParser<GpuArgs> &parser) {
	sprt::cerr << "usage: xlkdl gpu <schema.kdl>... [--out <file>] [--check]\n"
			   << "  writes the header of each schema to its output and the preambles of its\n"
			   << "  shaders to its preambles, from the schema's directory\n";
	parser.describe([](StringView str) { sprt::cerr << str; });
}

} // namespace

// The exit code is 1 when a schema does not load; otherwise the number of files - the header and
// the preambles - that could not be written, or under --check the number that are stale.
int runGpu(int argc, const char *argv[]) {
	CommandLineParser<GpuArgs> parser({
		GpuOption{.patterns = {"--out <file>"},
			.description = StringView("Write the header here instead of the schema's output"),
			.callback = [](GpuArgs &target, StringView, SpanView<StringView> args) -> bool {
		target.out = args[0].str<memory::StandardInterface>();
		return true;
	}},
		GpuOption{.patterns = {"--check"},
			.description = StringView("Write nothing; answer whether the files are up to date"),
			.callback = [](GpuArgs &target, StringView, SpanView<StringView>) -> bool {
		target.check = true;
		return true;
	}},
		GpuOption{.patterns = {"-h", "--help"},
			.description = StringView("This text"),
			.callback = [](GpuArgs &target, StringView, SpanView<StringView>) -> bool {
		target.help = true;
		return true;
	}},
	});

	GpuArgs args;
	if (argc > 0
			&& !parser.parse(args, argc, argv,
					Callback<void(GpuArgs &, StringView)>([](GpuArgs &target, StringView arg) {
		target.schemas.emplace_back(arg.str<memory::StandardInterface>());
	}))) {
		printGpuUsage(parser);
		return 1;
	}
	if (args.help || args.schemas.empty() || (!args.out.empty() && args.schemas.size() > 1)) {
		printGpuUsage(parser);
		return args.help ? 0 : 1;
	}

	int failed = 0;
	for (auto &file : args.schemas) {
		gpu::Schema schema;
		Diagnostics diag;
		if (!gpu::loadSchema(file, schema, diag)) {
			for (auto &it : diag.messages) { sprt::cerr << it << "\n"; }
			return 1;
		}
		auto dir = filepath::root(StringView(file));
		auto from = filepath::lastComponent(file);
		String text;
		if (schema.hasHeader()) {
			gpu::emitHeader(schema, text);
			auto path = args.out.empty()
					? filepath::merge<memory::StandardInterface>(dir, schema.output)
					: args.out;
			if (!writeOrCheck(path, text, from, args.check)) {
				++failed;
			}
		}
		for (auto &shader : schema.shaders) {
			if (shader.imported) {
				continue;
			}
			gpu::emitPreamble(schema, shader, text);
			auto path = filepath::merge<memory::StandardInterface>(dir, schema.preambles,
					mem_std::toString(shader.name, ".glsl"));
			if (!writeOrCheck(path, text, from, args.check)) {
				++failed;
			}
		}
	}
	return failed;
}

} // namespace stappler::xlkdl
