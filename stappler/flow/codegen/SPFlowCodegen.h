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

#ifndef STAPPLER_FLOW_CODEGEN_SPFLOWCODEGEN_H_
#define STAPPLER_FLOW_CODEGEN_SPFLOWCODEGEN_H_

#include "SPFlowCompiled.h"

// The generator: a built graph, written out as a C++ unit that carries it as constants.
// What is emitted is data: the tables SPFlowCompiled.h describes, filled in from
// a RuntimeGraph, and nothing that decides anything. The unit names `flow::StaticGraph<Tables>`,
// and loading that is where the tables become a graph again - operations resolved by name, the
// identity checked, the constants decoded.
// The generator takes the built graph and not the asset: the canonical order, the resolved pins,
// the casts on the edges, the scopes and the frame layout come out of the one build the interpreter
// also runs against, so a unit cannot disagree with the interpreter about the shape of the graph -
// it was copied from the same object.
// Every byte of the output is a function of the input: same graph, same registry, same options,
// same text. `codegen-emit` asserts it by emitting twice, and a freshness check relies on it to prove a
// committed unit current by regenerating it and comparing one number.
namespace STAPPLER_VERSIONIZED stappler::flow::codegen {

struct EmitOptions {
	// The unit's name: a C++ identifier (isUnitName), used as the last part of the namespace
	// `<unitNamespace>::<name>`, as the include guard and as the file stem `<name>.gen.h`.
	StringView name;

	// The namespace the unit is written into. It has to be inside `stappler`: the unit names the
	// kernel from there (`flow::`, `flow::value::`), whatever namespace it itself lives in.
	StringView unitNamespace = StringView("stappler::flow::gen");

	// The header the unit includes for the table types. The default is what a program that links
	// stappler_flow reaches; a build that stages headers elsewhere names its own path.
	StringView include = StringView("SPFlowCompiled.h");

	// The environment the unit's steps are compiled for (SPFlowEnv.h), as the C++ name the unit
	// spells it with, and the header that declares it. The kernel's own has no scene; a host with
	// one names its own, and a unit compiled for it serves runs in that environment only - in any
	// other the machine performs the nodes through the registry.
	StringView env = StringView("flow::NoEnv");
	StringView envInclude = StringView("SPFlowEnv.h");

	// The headers the unit's source includes for the operation bodies it calls directly - the
	// templates named by OpDef::inlineName. `SPFlowOpsInline.h` for the standard library,
	// and whatever a host's own module calls its own. The
	// generator does not check them and cannot: it holds names, not symbols. A unit whose graph
	// uses an operation whose body is not included fails to compile, naming the call - which is
	// where such a mistake belongs, and is why there is no fallback to the registry here.
	SpanView<StringView> bodyIncludes;

	/* Whether the unit carries the asset it was written from. The machine never reads it: a unit
	runs off its tables, and the asset is there for tools - a narrative of the run, an
	overlay on a canvas, a debugger that wants a `RuntimeGraph` beside the static one. Without it
	a unit simply cannot answer those questions, and `describe()` says so. The canonical CBOR of
	`GraphAsset::save()` and not JSON text: those are the very bytes `getContentHash()` is taken
	over, so "the unit carries the asset its hash names" is true by construction rather than by a
	second serialisation agreeing with the first. `data::read` takes them back. Off by default: a
	shipped game should not carry a document nobody in it reads, and a program that wants the
	narrative asks for it. */
	bool embedAsset = false;

	/* How many source files the unit's code is written across. One is the whole of it for any graph
	a person drew: a node costs the compiler about 230 ms - each `stepN` instantiates the door
	six times - so fifty nodes is ten seconds and a hundred is under half a minute. A graph a
	tool produced is another matter: the editor opens forty thousand, which is two and a half
	hours in one translation unit, alone, while the other cores wait. Splitting it is the
	ordinary answer: each part performs a range of nodes and exports one entry point for it, the
	first part dispatches by range, and the parts compile in parallel (five hundred nodes: 116 s
	in one file, 14.3 s in eight on eight cores). The
	tables are never split - they are constants in the header, and a header is included whole. 1
	(or 0) emits exactly what an unsplit unit always emitted, byte for byte, so that reaching for
	this option is additive. */
	uint32_t split = 1;

	// Whether the shaders of the unit's GPU blocks are written beside it: one `<name>_b<k>.comp`
	// per block its author gave to the GPU. Their hashes are in the identity either way; the text
	// is what a build compiles to SPIR-V.
	bool gpu = false;
};

struct Emitted {
	mem_std::String header; // <name>.gen.h

	/* The code, in one part or several (EmitOptions::split). `sources[0]` is `<name>.gen.cpp` and
	`sources[k]` is `<name>.gen.<k>.cpp`, which is the naming a caller writing them out uses and the
	only thing about the split a build system has to know: they are sources beside the header, and
	`LOCAL_SRCS_DIRS` picks them up like any other. */
	mem_std::Vector<mem_std::String> sources;

	// The hash of `header` with the unit's own `textHash` literal blanked, which is also what the
	// unit carries in its identity. A committed unit is fresh when regenerating it gives this
	// number.
	uint64_t textHash = 0;

	// Under EmitOptions::gpu: the GLSL of each GPU block, as a file name beside the unit and its
	// text.
	struct Shader {
		mem_std::String fileName;
		mem_std::String text;
	};
	mem_std::Vector<Shader> shaders;
};

// Whether `name` may name a unit: a C++ identifier of ASCII letters, digits and underscores, not
// starting with a digit.
SP_PUBLIC bool isUnitName(StringView);

// Whether `ns` may hold units: `stappler::` and one or more identifiers joined by `::`. `relative`
// is the part after `stappler::`.
SP_PUBLIC bool isUnitNamespace(StringView ns, StringView &relative);

// Writes the unit. The graph must be built; the registry must be the one it was built against and
// must hold the interpreter's core types (the frame layout is a function of interp.NodeState); the
// asset is what the graph was built from, for its name, its content hash and the extension
// declarations the unit binds against. Refusals are reported in the graph's vocabulary.
SP_PUBLIC Status emit(const flow::RuntimeGraph &, const flow::GraphAsset &,
		const flow::OpRegistry &, const EmitOptions &, Emitted &out,
		flow::DiagSink *diagnostic = nullptr);

template <flow::DiagContainer Out>
Status emit(const flow::RuntimeGraph &g, const flow::GraphAsset &asset,
		const flow::OpRegistry &ops, const EmitOptions &options, Emitted &out, Out *diagnostic) {
	flow::DiagSinkFor<Out> sink(diagnostic);
	return emit(g, asset, ops, options, out, sink.get());
}

} // namespace stappler::flow::codegen

#endif /* STAPPLER_FLOW_CODEGEN_SPFLOWCODEGEN_H_ */
