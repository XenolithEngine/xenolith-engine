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

// The numeric library over every scalar type, the vector operations, the conversions with a range
// guard, the enum conversions and the literals of the types that had none. Registered after every
// other family so that the existing operations keep their order.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;

#define SP_FLOW_OPS_TBODY(fn, arg) \
	Body{&inl::fn<arg, flow::OpContext>, StringView("flow::ops::inl::" #fn "<" #arg ">")}

Status registerNumericOps(OpRegistry &reg) {
	Registrar r{reg};

	auto add = [&](StringView name, SpanView<PinDesc> in, SpanView<PinDesc> out, OpFlags flags,
					   Body body) {
		OpDef def;
		def.name = name;
		def.dataIn = in;
		def.dataOut = out;
		def.flags = flags;
		def.parallel = OpParallel::Pure;
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	const auto pure = OpFlags::Pure | OpFlags::Infallible;

	auto binary = [&](StringView name, VarType lhs, VarType rhs, VarType result, OpFlags flags,
						  Body body) {
		PinDesc in[] = {pinDefault("lhs", lhs, mem_std::Value()),
			pinDefault("rhs", rhs, mem_std::Value())};
		PinDesc out[] = {pin("result", result)};
		add(name, SpanView<PinDesc>(in, 2), SpanView<PinDesc>(out, 1), flags, body);
	};

	auto unary = [&](StringView name, VarType value, VarType result, OpFlags flags, Body body) {
		PinDesc in[] = {pin("value", value)};
		PinDesc out[] = {pin("result", result)};
		add(name, SpanView<PinDesc>(in, 1), SpanView<PinDesc>(out, 1), flags, body);
	};

	binary("math.subInt", VarType::Int, VarType::Int, VarType::Int, pure,
			SP_FLOW_OPS_TBODY(mathSub, flow::value::VarType::Int));
	binary("math.divInt", VarType::Int, VarType::Int, VarType::Int, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(mathDiv, flow::value::VarType::Int));
	binary("math.minInt", VarType::Int, VarType::Int, VarType::Int, pure,
			SP_FLOW_OPS_TBODY(mathMin, flow::value::VarType::Int));
	binary("math.maxInt", VarType::Int, VarType::Int, VarType::Int, pure,
			SP_FLOW_OPS_TBODY(mathMax, flow::value::VarType::Int));
	unary("math.absInt", VarType::Int, VarType::Int, pure,
			SP_FLOW_OPS_TBODY(mathAbs, flow::value::VarType::Int));

	binary("math.divFloat", VarType::Float, VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathDiv, flow::value::VarType::Float));
	binary("math.minFloat", VarType::Float, VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathMin, flow::value::VarType::Float));
	binary("math.maxFloat", VarType::Float, VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathMax, flow::value::VarType::Float));
	unary("math.absFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathAbs, flow::value::VarType::Float));
	binary("compare.equalFloat", VarType::Float, VarType::Float, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareEqual, flow::value::VarType::Float));

	binary("math.addInt32", VarType::Int32, VarType::Int32, VarType::Int32, pure,
			SP_FLOW_OPS_TBODY(mathAdd, flow::value::VarType::Int32));
	binary("math.subInt32", VarType::Int32, VarType::Int32, VarType::Int32, pure,
			SP_FLOW_OPS_TBODY(mathSub, flow::value::VarType::Int32));
	binary("math.mulInt32", VarType::Int32, VarType::Int32, VarType::Int32, pure,
			SP_FLOW_OPS_TBODY(mathMul, flow::value::VarType::Int32));
	binary("math.divInt32", VarType::Int32, VarType::Int32, VarType::Int32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(mathDiv, flow::value::VarType::Int32));
	binary("math.minInt32", VarType::Int32, VarType::Int32, VarType::Int32, pure,
			SP_FLOW_OPS_TBODY(mathMin, flow::value::VarType::Int32));
	binary("math.maxInt32", VarType::Int32, VarType::Int32, VarType::Int32, pure,
			SP_FLOW_OPS_TBODY(mathMax, flow::value::VarType::Int32));
	unary("math.absInt32", VarType::Int32, VarType::Int32, pure,
			SP_FLOW_OPS_TBODY(mathAbs, flow::value::VarType::Int32));
	binary("compare.lessInt32", VarType::Int32, VarType::Int32, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareLess, flow::value::VarType::Int32));
	binary("compare.equalInt32", VarType::Int32, VarType::Int32, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareEqual, flow::value::VarType::Int32));

	binary("math.addUInt32", VarType::UInt32, VarType::UInt32, VarType::UInt32, pure,
			SP_FLOW_OPS_TBODY(mathAdd, flow::value::VarType::UInt32));
	binary("math.subUInt32", VarType::UInt32, VarType::UInt32, VarType::UInt32, pure,
			SP_FLOW_OPS_TBODY(mathSub, flow::value::VarType::UInt32));
	binary("math.mulUInt32", VarType::UInt32, VarType::UInt32, VarType::UInt32, pure,
			SP_FLOW_OPS_TBODY(mathMul, flow::value::VarType::UInt32));
	binary("math.divUInt32", VarType::UInt32, VarType::UInt32, VarType::UInt32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(mathDiv, flow::value::VarType::UInt32));
	binary("math.minUInt32", VarType::UInt32, VarType::UInt32, VarType::UInt32, pure,
			SP_FLOW_OPS_TBODY(mathMin, flow::value::VarType::UInt32));
	binary("math.maxUInt32", VarType::UInt32, VarType::UInt32, VarType::UInt32, pure,
			SP_FLOW_OPS_TBODY(mathMax, flow::value::VarType::UInt32));
	binary("compare.lessUInt32", VarType::UInt32, VarType::UInt32, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareLess, flow::value::VarType::UInt32));
	binary("compare.equalUInt32", VarType::UInt32, VarType::UInt32, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareEqual, flow::value::VarType::UInt32));

	binary("math.addFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathAdd, flow::value::VarType::Float32));
	binary("math.subFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathSub, flow::value::VarType::Float32));
	binary("math.mulFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathMul, flow::value::VarType::Float32));
	binary("math.divFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathDiv, flow::value::VarType::Float32));
	binary("math.minFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathMin, flow::value::VarType::Float32));
	binary("math.maxFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathMax, flow::value::VarType::Float32));
	unary("math.absFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathAbs, flow::value::VarType::Float32));
	binary("compare.lessFloat32", VarType::Float32, VarType::Float32, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareLess, flow::value::VarType::Float32));
	binary("compare.equalFloat32", VarType::Float32, VarType::Float32, VarType::Bool, pure,
			SP_FLOW_OPS_TBODY(compareEqual, flow::value::VarType::Float32));

	unary("math.floorFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathFloor, flow::value::VarType::Float));
	unary("math.ceilFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathCeil, flow::value::VarType::Float));
	unary("math.sqrtFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathSqrt, flow::value::VarType::Float));
	unary("math.sinFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathSin, flow::value::VarType::Float));
	unary("math.cosFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathCos, flow::value::VarType::Float));
	unary("math.tanFloat", VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathTan, flow::value::VarType::Float));
	binary("math.powFloat", VarType::Float, VarType::Float, VarType::Float, pure,
			SP_FLOW_OPS_TBODY(mathPow, flow::value::VarType::Float));

	unary("math.floorFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathFloor, flow::value::VarType::Float32));
	unary("math.ceilFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathCeil, flow::value::VarType::Float32));
	unary("math.sqrtFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathSqrt, flow::value::VarType::Float32));
	unary("math.sinFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathSin, flow::value::VarType::Float32));
	unary("math.cosFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathCos, flow::value::VarType::Float32));
	unary("math.tanFloat32", VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathTan, flow::value::VarType::Float32));
	binary("math.powFloat32", VarType::Float32, VarType::Float32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(mathPow, flow::value::VarType::Float32));

	struct VecKind {
		const char *suffix;
		VarType type;
		uint32_t arity;
	};
	const VecKind vecs[] = {{"Vec2", VarType::Vec2, 2}, {"Vec3", VarType::Vec3, 3},
		{"Vec4", VarType::Vec4, 4}};
	for (auto &v : vecs) {
		auto reg3 = [&](const char *op, const char *tmpl, flow::OpFn fn2, flow::OpFn fn3,
							flow::OpFn fn4, int shape) {
			auto opName = mem_std::toString("math.", op, v.suffix);
			auto inlineName = mem_std::toString("flow::ops::inl::", tmpl, "<", v.arity, ">");
			Body body{v.arity == 2 ? fn2 : (v.arity == 3 ? fn3 : fn4), StringView(inlineName)};
			switch (shape) {
			case 0: binary(StringView(opName), v.type, v.type, v.type, pure, body); break;
			case 1: binary(StringView(opName), v.type, VarType::Float32, v.type, pure, body); break;
			case 2: binary(StringView(opName), v.type, v.type, VarType::Float32, pure, body); break;
			case 3: unary(StringView(opName), v.type, VarType::Float32, pure, body); break;
			default: unary(StringView(opName), v.type, v.type, pure, body); break;
			}
		};
		reg3("add", "mathAddVec", &inl::mathAddVec<2, OpContext>, &inl::mathAddVec<3, OpContext>,
				&inl::mathAddVec<4, OpContext>, 0);
		reg3("sub", "mathSubVec", &inl::mathSubVec<2, OpContext>, &inl::mathSubVec<3, OpContext>,
				&inl::mathSubVec<4, OpContext>, 0);
		reg3("scale", "mathScaleVec", &inl::mathScaleVec<2, OpContext>,
				&inl::mathScaleVec<3, OpContext>, &inl::mathScaleVec<4, OpContext>, 1);
		reg3("dot", "mathDotVec", &inl::mathDotVec<2, OpContext>, &inl::mathDotVec<3, OpContext>,
				&inl::mathDotVec<4, OpContext>, 2);
		reg3("length", "mathLengthVec", &inl::mathLengthVec<2, OpContext>,
				&inl::mathLengthVec<3, OpContext>, &inl::mathLengthVec<4, OpContext>, 3);
		reg3("normalize", "mathNormalizeVec", &inl::mathNormalizeVec<2, OpContext>,
				&inl::mathNormalizeVec<3, OpContext>, &inl::mathNormalizeVec<4, OpContext>, 4);
	}

	unary("convert.floatToInt", VarType::Float, VarType::Int, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Int));
	unary("convert.intToInt32", VarType::Int, VarType::Int32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Int32));
	unary("convert.intToUInt32", VarType::Int, VarType::UInt32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::UInt32));
	unary("convert.int32ToUInt32", VarType::Int32, VarType::UInt32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::UInt32));
	unary("convert.uInt32ToInt32", VarType::UInt32, VarType::Int32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Int32));
	unary("convert.floatToFloat32", VarType::Float, VarType::Float32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Float32));
	unary("convert.float32ToInt32", VarType::Float32, VarType::Int32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Int32));
	unary("convert.int32ToFloat32", VarType::Int32, VarType::Float32, pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Float32));

	unary("enum.toInt32", VarType::Enum, VarType::Int32, OpFlags::Pure,
			SP_FLOW_OPS_TBODY(convertScalar, flow::value::VarType::Int32));
	{
		PinDesc in[] = {pin("value", VarType::Int32),
			pinRole(pin("family", VarType::String), flow::PinRole::EnumFamily)};
		PinDesc out[] = {pinRole(pin("result", VarType::Enum), flow::PinRole::EnumFamily)};
		add("enum.fromInt32", SpanView<PinDesc>(in, 2), SpanView<PinDesc>(out, 1), pure,
				SP_FLOW_OPS_BODY(enumFromInt32));
	}

	auto literal = [&](StringView name, VarType type) {
		PinDesc in[] = {pinDefault("value", type, mem_std::Value())};
		PinDesc out[] = {pin("value", type)};
		add(name, SpanView<PinDesc>(in, 1), SpanView<PinDesc>(out, 1), pure,
				SP_FLOW_OPS_BODY(valuePassthrough));
	};
	literal("value.int32", VarType::Int32);
	literal("value.uint32", VarType::UInt32);
	literal("value.float32", VarType::Float32);
	literal("value.vec2", VarType::Vec2);
	literal("value.vec3", VarType::Vec3);
	literal("value.vec4", VarType::Vec4);
	literal("value.color", VarType::Color);

	return r.status;
}

#undef SP_FLOW_OPS_TBODY

} // namespace stappler::flow::ops
