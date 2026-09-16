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

// CommandLineParser operand handling — in particular that an operand taken from its own argv
// element survives whitespace inside it (a path with a space, as the shell delivered it).

#include "SPCommon.h"
#include "SPCommandLineParser.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

struct CmdlineOutput {
	memory::StandardInterface::StringType engine;
	memory::StandardInterface::StringType first;
	memory::StandardInterface::StringType second;
	memory::StandardInterface::StringType positional;
	int64_t jobs = 0;
};

using Option = CommandLineOption<CmdlineOutput>;

CommandLineParser<CmdlineOutput> makeParser() {
	return CommandLineParser<CmdlineOutput>({
		Option{.patterns = {"--engine <path>"},
			.description = StringView("single trailing operand"),
			.callback = [](CmdlineOutput &out, StringView, SpanView<StringView> args) -> bool {
		out.engine = args[0].str<memory::StandardInterface>();
		return true;
	}},
		Option{.patterns = {"--pair <a> <b>"},
			.description = StringView("two whitespace-separated operands"),
			.callback = [](CmdlineOutput &out, StringView, SpanView<StringView> args) -> bool {
		out.first = args[0].str<memory::StandardInterface>();
		out.second = args[1].str<memory::StandardInterface>();
		return true;
	}},
		Option{.patterns = {"-j<#>", "--jobs <#>"},
			.description = StringView("numeric operand"),
			.callback = [](CmdlineOutput &out, StringView, SpanView<StringView> args) -> bool {
		out.jobs = StringView(args[0]).readInteger(10).get(0);
		return true;
	}},
	});
}

} // namespace

void performCommandLineTests() {
	sprt::cout << "\n== stappler core command line tests ==\n";

	auto parser = makeParser();
	auto positionalCb =
			Callback<void(CmdlineOutput &, StringView)>([](CmdlineOutput &out, StringView arg) {
		out.positional = arg.str<memory::StandardInterface>();
	});

	{
		// The shell hands a quoted path over as ONE argv element; the parser used to cut it at the
		// first space, so `--engine "/my dir/engine"` arrived as "/my".
		const char *argv[] = {"--engine", "/tmp/my dir/engine"};
		CmdlineOutput out;
		check(parser.parse(out, 2, argv, positionalCb), "cmdline: --engine <path> parsed");
		checkEq(StringView(out.engine), StringView("/tmp/my dir/engine"),
				"cmdline: operand keeps its spaces");
	}

	{
		const char *argv[] = {"--engine", "/tmp/plain/engine"};
		CmdlineOutput out;
		check(parser.parse(out, 2, argv, positionalCb), "cmdline: plain operand parsed");
		checkEq(StringView(out.engine), StringView("/tmp/plain/engine"),
				"cmdline: operand without a space is unchanged");
	}

	{
		// A pattern with two operands still splits: only the LAST placeholder swallows the rest.
		const char *argv[] = {"--pair", "alpha", "beta"};
		CmdlineOutput out;
		check(parser.parse(out, 3, argv, positionalCb), "cmdline: --pair <a> <b> parsed");
		checkEq(StringView(out.first), StringView("alpha"), "cmdline: first of two operands");
		checkEq(StringView(out.second), StringView("beta"), "cmdline: second of two operands");
	}

	{
		// The inline form splits on whitespace itself, so an unquoted space still separates.
		const char *argv[] = {"--pair=alpha beta"};
		CmdlineOutput out;
		check(parser.parse(out, 1, argv, positionalCb), "cmdline: --pair=a b parsed");
		checkEq(StringView(out.second), StringView("beta"), "cmdline: inline form still splits");
	}

	{
		const char *argv[] = {"--jobs", "12"};
		CmdlineOutput out;
		check(parser.parse(out, 2, argv, positionalCb), "cmdline: --jobs <#> parsed");
		check(out.jobs == 12, "cmdline: numeric operand");
	}

	{
		// A positional argument with a space is one argv element and must stay whole too.
		const char *argv[] = {"--engine", "/tmp/e", "/tmp/with space/project"};
		CmdlineOutput out;
		check(parser.parse(out, 3, argv, positionalCb), "cmdline: positional after option parsed");
		checkEq(StringView(out.positional), StringView("/tmp/with space/project"),
				"cmdline: positional keeps its spaces");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
