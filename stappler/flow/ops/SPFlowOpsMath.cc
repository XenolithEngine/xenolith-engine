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

// Arithmetic, logic, comparison, conversion and literals. `convert.intToFloat` is not a
// convenience: the conversion matrix calls Int -> Float a narrow cell (int64 does not fit in a
// double without loss), and only Same and Widen cross an edge implicitly, so without this node an
// integer cannot reach a float input. The bodies are in SPFlowOpsInline.h; this file is the signatures
// and the registration.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;

Status registerMathOps(OpRegistry &reg) {
	Registrar r{reg};

	auto binary = [&](StringView name, SpanView<StringView> syn, VarType type, Body body) {
		PinDesc in[] = {pinDefault("lhs", type, mem_std::Value()),
			pinDefault("rhs", type, mem_std::Value())};
		PinDesc out[] = {pin("result", type)};
		OpDef def;
		def.name = name;
		def.synonyms = syn;
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure | OpFlags::Infallible;
		def.parallel = OpParallel::Pure;
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	auto compare = [&](StringView name, SpanView<StringView> syn, VarType type, Body body) {
		PinDesc in[] = {pin("lhs", type), pin("rhs", type)};
		PinDesc out[] = {pin("result", VarType::Bool)};
		OpDef def;
		def.name = name;
		def.synonyms = syn;
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure | OpFlags::Infallible;
		def.parallel = OpParallel::Pure;
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	auto unary = [&](StringView name, SpanView<StringView> syn, VarType in, VarType out,
					  Body body) {
		PinDesc inPins[] = {pin("value", in)};
		PinDesc outPins[] = {pin("result", out)};
		OpDef def;
		def.name = name;
		def.synonyms = syn;
		def.dataIn = SpanView<PinDesc>(inPins, 1);
		def.dataOut = SpanView<PinDesc>(outPins, 1);
		def.flags = OpFlags::Pure | OpFlags::Infallible;
		def.parallel = OpParallel::Pure;
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	static constexpr StringView syn0[] = {StringView("plus"), StringView("sum"), StringView("add")};
	static constexpr StringView syn1[] = {StringView("minus"), StringView("difference"), StringView("subtract")};
	static constexpr StringView syn2[] = {StringView("times"), StringView("product"), StringView("multiply")};
	static constexpr StringView syn3[] = {StringView("plus"), StringView("sum"), StringView("add")};
	static constexpr StringView syn4[] = {StringView("times"), StringView("product"), StringView("multiply")};
	static constexpr StringView syn5[] = {StringView("both"), StringView("conjunction")};
	static constexpr StringView syn6[] = {StringView("either"), StringView("disjunction")};
	static constexpr StringView syn7[] = {StringView("invert"), StringView("negate")};
	static constexpr StringView syn8[] = {StringView("less"), StringView("smaller"), StringView("below")};
	static constexpr StringView syn9[] = {StringView("equal"), StringView("same")};
	static constexpr StringView syn10[] = {StringView("less"), StringView("smaller"), StringView("below")};
	static constexpr StringView syn11[] = {StringView("cast"), StringView("widen")};
	static constexpr StringView syn12[] = {StringView("constant"), StringView("literal")};
	static constexpr StringView syn13[] = {StringView("constant"), StringView("literal")};
	static constexpr StringView syn14[] = {StringView("constant"), StringView("literal")};

	binary(StringView("math.addFloat"), SpanView<StringView>(syn0, 3), VarType::Float, SP_FLOW_OPS_BODY(mathAddFloat));
	binary(StringView("math.subFloat"), SpanView<StringView>(syn1, 3), VarType::Float, SP_FLOW_OPS_BODY(mathSubFloat));
	binary(StringView("math.mulFloat"), SpanView<StringView>(syn2, 3), VarType::Float, SP_FLOW_OPS_BODY(mathMulFloat));
	binary(StringView("math.addInt"), SpanView<StringView>(syn3, 3), VarType::Int, SP_FLOW_OPS_BODY(mathAddInt));
	binary(StringView("math.mulInt"), SpanView<StringView>(syn4, 3), VarType::Int, SP_FLOW_OPS_BODY(mathMulInt));

	binary(StringView("logic.and"), SpanView<StringView>(syn5, 2), VarType::Bool, SP_FLOW_OPS_BODY(logicAnd));
	binary(StringView("logic.or"), SpanView<StringView>(syn6, 2), VarType::Bool, SP_FLOW_OPS_BODY(logicOr));
	unary(StringView("logic.not"), SpanView<StringView>(syn7, 2), VarType::Bool, VarType::Bool, SP_FLOW_OPS_BODY(logicNot));

	compare(StringView("compare.lessFloat"), SpanView<StringView>(syn8, 3), VarType::Float, SP_FLOW_OPS_BODY(compareLessFloat));
	compare(StringView("compare.equalInt"), SpanView<StringView>(syn9, 2), VarType::Int, SP_FLOW_OPS_BODY(compareEqualInt));
	compare(StringView("compare.lessInt"), SpanView<StringView>(syn10, 3), VarType::Int, SP_FLOW_OPS_BODY(compareLessInt));

	unary(StringView("convert.intToFloat"), SpanView<StringView>(syn11, 2), VarType::Int, VarType::Float, SP_FLOW_OPS_BODY(convertIntToFloat));

	{
		PinDesc in[] = {pin("items", VarType::Array, value::makeChain(VarType::Int)),
			pin("index", VarType::Int)};
		PinDesc out[] = {pin("value", VarType::Int)};
		OpDef def;
		def.name = StringView("array.getInt");
		static constexpr StringView syn[] = {StringView("index"), StringView("element"), StringView("at")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		// Pure but not Infallible, and the one operation in this file where the difference shows:
		// an index outside the array is a refusal this operation makes on its own account, so a run
		// that wants to undo exactly the node that refused has to have a boundary in front of it.
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		def.invoke = &inl::arrayGetInt<OpContext>;
		def.inlineName = StringView("flow::ops::inl::arrayGetInt");
		r.add(def);
	}

	// The literals. Same shape for each: `value` in, `value` out.
	auto literal = [&](StringView name, SpanView<StringView> syn, VarType type, Body body) {
		PinDesc in[] = {pinDefault("value", type, mem_std::Value())};
		PinDesc out[] = {pin("value", type)};
		OpDef def;
		def.name = name;
		def.synonyms = syn;
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure | OpFlags::Infallible;
		def.parallel = OpParallel::Pure;
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	literal(StringView("value.float"), SpanView<StringView>(syn12, 2), VarType::Float, SP_FLOW_OPS_BODY(valuePassthrough));
	literal(StringView("value.int"), SpanView<StringView>(syn13, 2), VarType::Int, SP_FLOW_OPS_BODY(valuePassthrough));
	literal(StringView("value.bool"), SpanView<StringView>(syn14, 2), VarType::Bool, SP_FLOW_OPS_BODY(valuePassthrough));

	return r.status;
}

} // namespace stappler::flow::ops
