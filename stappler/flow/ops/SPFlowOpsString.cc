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

// Strings, and the one node that exists to be seen in an execution log. The bodies are in
// SPFlowOpsInline.h; this file is the signatures and the registration.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;

Status registerStringOps(OpRegistry &reg) {
	Registrar r{reg};

	{
		PinDesc in[] = {pin("lhs", VarType::String), pin("rhs", VarType::String)};
		PinDesc out[] = {pin("result", VarType::String)};
		OpDef def;
		def.name = StringView("string.concat");
		static constexpr StringView syn[] = {StringView("join"), StringView("append"), StringView("text")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		auto body = SP_FLOW_OPS_BODY(stringConcat);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		PinDesc in[] = {pin("value", VarType::String)};
		PinDesc out[] = {pin("value", VarType::String)};
		OpDef def;
		def.name = StringView("value.string");
		static constexpr StringView syn[] = {StringView("constant"), StringView("literal"), StringView("text")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		// A container cannot be passed through by value: setOutput refuses a container Var, and the
		// copy the body makes is what makes the output the node's own.
		auto body = SP_FLOW_OPS_BODY(valueString);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		const StringView then[] = {StringView("then")};
		PinDesc in[] = {pin("value", VarType::Float)};
		OpDef def;
		def.name = StringView("debug.trace");
		static constexpr StringView syn[] = {StringView("log"), StringView("print"), StringView("dump")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.parallel = OpParallel::Flow;
		auto body = SP_FLOW_OPS_BODY(debugTrace);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	return r.status;
}

} // namespace stappler::flow::ops
