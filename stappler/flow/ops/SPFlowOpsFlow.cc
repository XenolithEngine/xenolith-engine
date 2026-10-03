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

// Control flow: where a run starts, how it goes in order, and how it chooses. The bodies are in
// SPFlowOpsInline.h - one template each, instantiated here over the abstract door and by a generated
// unit over its own. What is left in this file is the signatures, which is what an asset is written
// against and what `ops-core` holds to a golden table.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;

Status registerFlowOps(OpRegistry &reg) {
	Registrar r{reg};

	const StringView then[] = {StringView("then")};
	const StringView order[] = {StringView("first"), StringView("second")};
	const StringView branches[] = {StringView("true"), StringView("false")};
	const StringView loop[] = {StringView("body"), StringView("completed")};

	// Every operation in this file carries OpFlags::Infallible. None of them rejects anything: they
	// read a condition or a cursor, fire an exec output, and that is the whole of it. A run whose
	// quantum is Failable therefore takes no boundary in front of any of them, which matters
	// because flow nodes are the most numerous kind in a graph of any size.

	{
		// The entry point: no exec input, so the interpreter starts here.
		OpDef def;
		def.name = StringView("flow.event");
		static constexpr StringView syn[] = {StringView("start"), StringView("begin"), StringView("entry")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::Infallible;
		def.parallel = OpParallel::Flow;
		def.invoke = &inl::flowEvent<OpContext>;
		def.inlineName = StringView("flow::ops::inl::flowEvent");
		r.add(def);
	}

	{
		OpDef def;
		def.name = StringView("flow.sequence");
		static constexpr StringView syn[] = {StringView("then"), StringView("order"), StringView("both")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(order, 2);
		def.flags = OpFlags::Infallible;
		def.parallel = OpParallel::Flow;
		def.invoke = &inl::flowSequence<OpContext>;
		def.inlineName = StringView("flow::ops::inl::flowSequence");
		r.add(def);
	}

	{
		PinDesc in[] = {pinRequired("condition", VarType::Bool)};
		OpDef def;
		def.name = StringView("flow.branch");
		static constexpr StringView syn[] = {StringView("if"), StringView("condition"), StringView("else")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(branches, 2);
		def.execExclusive = true;
		def.flags = OpFlags::Infallible;
		def.parallel = OpParallel::Flow;
		def.invoke = &inl::flowBranch<OpContext>;
		def.inlineName = StringView("flow::ops::inl::flowBranch");
		r.add(def);
	}

	{
		PinDesc in[] = {pin("items", VarType::Array, value::makeChain(VarType::Int))};
		PinDesc out[] = {pin("item", VarType::Int), pin("index", VarType::Int)};
		value::FieldDef locals[] = {
			value::FieldDef{.name = StringView("cursor"), .type = VarType::Int},
		};

		OpDef def;
		def.name = StringView("flow.forEach");
		static constexpr StringView syn[] = {StringView("loop"), StringView("iterate"), StringView("each")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(loop, 2);
		def.scopeExecOut = 1u << 0; // `body`, and only `body`
		def.execExclusive = true;
		def.locals = SpanView<value::FieldDef>(locals, 1);
		def.flags = OpFlags::Infallible;
		def.parallel = OpParallel::Flow;
		def.invoke = &inl::flowForEach<OpContext>;
		def.inlineName = StringView("flow::ops::inl::flowForEach");
		r.add(def);
	}

	return r.status;
}

} // namespace stappler::flow::ops
