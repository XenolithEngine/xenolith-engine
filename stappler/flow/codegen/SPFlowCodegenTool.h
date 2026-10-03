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

#ifndef STAPPLER_FLOW_CODEGEN_SPFLOWCODEGENTOOL_H_
#define STAPPLER_FLOW_CODEGEN_SPFLOWCODEGENTOOL_H_

#include "SPFlowCodegen.h"

// The generator as a program: graph files in, C++ units out. The argument parsing, the loop over the
// files, writing and `--check` are here, so that a program that wants the tool with its own operation
// families, its own run environment and its own words for a refusal is a dozen lines around
// runTool() rather than a copy of it:
//
//     xlgen --out <dir> [--name <id>] [--ops <family>,...] [--split <n>] [--embed-asset] [--gpu]
//           [--env <C++ name>] [--env-include <header>] [--namespace <ns>] [--check] <graph.json>...
//
// The exit code is the number of files the tool could not write, or under `--check` the number that
// are not what it would write. A graph is built unbound: a unit binds to a scene where it is loaded.
namespace STAPPLER_VERSIONIZED stappler::flow::codegen {

// A family of operations a graph may name: the word `--ops` takes, what registers it, and the header
// a unit includes to call its bodies by name.
struct ToolFamily {
	StringView name;
	Status (*registerOps)(OpRegistry &) = nullptr;
	StringView bodyInclude;
};

struct ToolOptions {
	// The program's name, in messages.
	StringView tool = StringView("xlgen");

	// The families the program knows, and which of them a run uses when `--ops` is not given.
	SpanView<ToolFamily> families;
	StringView defaultOps = StringView("core");

	// What `--env`, `--env-include` and `--namespace` default to (EmitOptions).
	StringView env = StringView("flow::NoEnv");
	StringView envInclude = StringView("SPFlowEnv.h");
	StringView unitNamespace = StringView("stappler::flow::gen");

	// How a refusal is written before it is printed: numbers by default (flow::writeDiagNumbers);
	// a program with words of its own passes its writer.
	void (*writeDiag)(mem_std::Value *, const Diag &) = nullptr;
};

SP_PUBLIC int runTool(int argc, const char *argv[], const ToolOptions &);

} // namespace stappler::flow::codegen

#endif /* STAPPLER_FLOW_CODEGEN_SPFLOWCODEGENTOOL_H_ */
