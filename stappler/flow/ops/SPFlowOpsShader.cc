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

// The shader form of an operation, derived from the template form it already has. An operation's
// C++ body is named by `inlineName`, and its GLSL body is decided here from the same name and the
// pin types, so the two spellings of one body are one entry rather than two tables that could fall
// out of step. Every GLSL body has one calling convention,
// `int name(in inputs..., out outputs...)`, returning 0 or the Status the C++ body refuses with.
// The arithmetic follows SPFlowOpsInline.h exactly: integers wrap through `uint`, MIN / -1 and
// abs(MIN) wrap to MIN, a zero integer divisor refuses, min and max are written as the comparisons
// the C++ makes.

#include "SPFlowOps.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

namespace {

StringView glslType(VarType t) {
	switch (t) {
	case VarType::Bool: return StringView("bool");
	case VarType::Int32: return StringView("int");
	case VarType::UInt32: return StringView("uint");
	case VarType::Float32: return StringView("float");
	case VarType::Vec2: return StringView("vec2");
	case VarType::Vec3: return StringView("vec3");
	case VarType::Vec4:
	case VarType::Color: return StringView("vec4");
	default: return StringView();
	}
}

StringView typeTag(VarType t) {
	switch (t) {
	case VarType::Bool: return StringView("b");
	case VarType::Int32: return StringView("i32");
	case VarType::UInt32: return StringView("u32");
	case VarType::Float32: return StringView("f32");
	case VarType::Vec2: return StringView("v2");
	case VarType::Vec3: return StringView("v3");
	case VarType::Vec4: return StringView("v4");
	case VarType::Color: return StringView("c");
	default: return StringView();
	}
}

bool isGpuValue(VarType t) { return !glslType(t).empty(); }

// The template's own name, without the namespace and the template arguments.
StringView templateOf(StringView inlineName) {
	auto name = inlineName;
	if (name.starts_with(StringView("flow::ops::inl::"))) {
		name = StringView(name.data() + 16, name.size() - 16);
	}
	for (size_t i = 0; i < name.size(); ++i) {
		if (name[i] == '<') {
			return StringView(name.data(), i);
		}
	}
	return name;
}

mem_std::String statusText(Status st) { return mem_std::toString(int64_t(toInt(st))); }

struct Form {
	mem_std::String name;
	mem_std::String source;
	bool inexact = false;
};

// `int xs_<op>_<tags>(in T a, in T b, out R r) { ... return 0; }`
Form binaryBody(StringView op, VarType lhs, VarType rhs, VarType result, StringView expr,
		StringView guard = StringView()) {
	Form f;
	f.name = mem_std::toString("xs_", op, "_", typeTag(lhs));
	f.source = mem_std::toString("int ", f.name, "(in ", glslType(lhs), " a, in ", glslType(rhs),
			" b, out ", glslType(result), " r) {\n", guard, "\tr = ", expr, ";\n\treturn 0;\n}\n");
	return f;
}

Form unaryBody(StringView op, VarType value, VarType result, StringView body) {
	Form f;
	f.name = mem_std::toString("xs_", op, "_", typeTag(value));
	f.source = mem_std::toString("int ", f.name, "(in ", glslType(value), " a, out ",
			glslType(result), " r) {\n", body, "\treturn 0;\n}\n");
	return f;
}

// A dot product in the C++ body's order, so that a driver cannot fuse it into another sum.
mem_std::String dotText(uint32_t n, StringView a, StringView b) {
	mem_std::String out("0.0");
	static constexpr const char *comp[] = {"x", "y", "z", "w"};
	for (uint32_t k = 0; k < n; ++k) {
		out = mem_std::toString("(", out, " + ", a, ".", comp[k], " * ", b, ".", comp[k], ")");
	}
	return out;
}

bool deriveBody(const OpDef &def, Form &f) {
	auto tmpl = templateOf(def.inlineName);
	auto ins = def.dataIn;
	auto outs = def.dataOut;
	const auto invalid = statusText(Status::ErrorInvalidArguemnt);

	auto allGpu = [&] {
		for (auto &p : ins) {
			if (!isGpuValue(p.type)) {
				return false;
			}
		}
		for (auto &p : outs) {
			if (!isGpuValue(p.type)) {
				return false;
			}
		}
		return true;
	};

	if (ins.size() == 2 && outs.size() == 1 && allGpu()) {
		auto l = ins[0].type;
		auto r = outs[0].type;
		const bool isInt = l == VarType::Int32;
		const bool isUInt = l == VarType::UInt32;
		if (tmpl == StringView("mathAdd")) {
			f = binaryBody("add", l, ins[1].type, r,
					isInt ? StringView("int(uint(a) + uint(b))") : StringView("a + b"));
		} else if (tmpl == StringView("mathSub")) {
			f = binaryBody("sub", l, ins[1].type, r,
					isInt ? StringView("int(uint(a) - uint(b))") : StringView("a - b"));
		} else if (tmpl == StringView("mathMul")) {
			f = binaryBody("mul", l, ins[1].type, r,
					isInt ? StringView("int(uint(a) * uint(b))") : StringView("a * b"));
		} else if (tmpl == StringView("mathDiv")) {
			if (isInt) {
				f = binaryBody("div", l, ins[1].type, r,
						"(b == -1) ? int(0u - uint(a)) : (a / b)",
						mem_std::toString("\tif (b == 0) { r = 0; return ", invalid, "; }\n"));
			} else if (isUInt) {
				f = binaryBody("div", l, ins[1].type, r, "a / b",
						mem_std::toString("\tif (b == 0u) { r = 0u; return ", invalid, "; }\n"));
			} else {
				f = binaryBody("div", l, ins[1].type, r, "a / b");
			}
		} else if (tmpl == StringView("mathMin")) {
			f = binaryBody("min", l, ins[1].type, r, "(b < a) ? b : a");
		} else if (tmpl == StringView("mathMax")) {
			f = binaryBody("max", l, ins[1].type, r, "(a < b) ? b : a");
		} else if (tmpl == StringView("compareLess")) {
			f = binaryBody("less", l, ins[1].type, r, "a < b");
		} else if (tmpl == StringView("compareEqual")) {
			f = binaryBody("equal", l, ins[1].type, r, "a == b");
		} else if (tmpl == StringView("mathPow")) {
			f = binaryBody("pow", l, ins[1].type, r, "pow(a, b)");
			f.inexact = true;
		} else if (tmpl == StringView("logicAnd")) {
			f = binaryBody("and", l, ins[1].type, r, "a && b");
		} else if (tmpl == StringView("logicOr")) {
			f = binaryBody("or", l, ins[1].type, r, "a || b");
		} else if (tmpl == StringView("mathAddVec")) {
			f = binaryBody("addVec", l, ins[1].type, r, "a + b");
		} else if (tmpl == StringView("mathSubVec")) {
			f = binaryBody("subVec", l, ins[1].type, r, "a - b");
		} else if (tmpl == StringView("mathScaleVec")) {
			f = binaryBody("scaleVec", l, ins[1].type, r, "a * b");
		} else if (tmpl == StringView("mathDotVec")) {
			auto n = l == VarType::Vec2 ? 2 : (l == VarType::Vec3 ? 3 : 4);
			f = binaryBody("dotVec", l, ins[1].type, r, dotText(n, "a", "b"));
		} else {
			return false;
		}
		return true;
	}

	if (ins.size() == 1 && outs.size() == 1 && allGpu()) {
		auto v = ins[0].type;
		auto r = outs[0].type;
		auto n = v == VarType::Vec2 ? 2 : (v == VarType::Vec3 ? 3 : 4);
		if (tmpl == StringView("mathAbs")) {
			f = unaryBody("abs", v, r,
					v == VarType::Int32 ? StringView("\tr = (a < 0) ? int(0u - uint(a)) : a;\n")
										: StringView("\tr = (a < 0.0) ? -a : a;\n"));
		} else if (tmpl == StringView("mathFloor")) {
			f = unaryBody("floor", v, r, "\tr = floor(a);\n");
		} else if (tmpl == StringView("mathCeil")) {
			f = unaryBody("ceil", v, r, "\tr = ceil(a);\n");
		} else if (tmpl == StringView("mathSqrt")) {
			f = unaryBody("sqrt", v, r, "\tr = sqrt(a);\n");
			f.inexact = true;
		} else if (tmpl == StringView("mathSin")) {
			f = unaryBody("sin", v, r, "\tr = sin(a);\n");
			f.inexact = true;
		} else if (tmpl == StringView("mathCos")) {
			f = unaryBody("cos", v, r, "\tr = cos(a);\n");
			f.inexact = true;
		} else if (tmpl == StringView("mathTan")) {
			f = unaryBody("tan", v, r, "\tr = tan(a);\n");
			f.inexact = true;
		} else if (tmpl == StringView("logicNot")) {
			f = unaryBody("not", v, r, "\tr = !a;\n");
		} else if (tmpl == StringView("mathLengthVec")) {
			f = unaryBody("lengthVec", v, r, mem_std::toString("\tr = sqrt(", dotText(n, "a", "a"), ");\n"));
			f.inexact = true;
		} else if (tmpl == StringView("mathNormalizeVec")) {
			f = unaryBody("normalizeVec", v, r,
					mem_std::toString("\tfloat l = sqrt(", dotText(n, "a", "a"), ");\n\tr = (l > 0.0) ? (a / l) : ",
							glslType(v), "(0.0);\n"));
			f.inexact = true;
		} else if (tmpl == StringView("convertScalar")) {
			// Between two 32-bit types, with the value layer's range refusals (SPFlowValueVar.cc).
			if (v == VarType::Int32 && r == VarType::UInt32) {
				f = unaryBody("toUInt32", v, r,
						mem_std::toString("\tif (a < 0) { r = 0u; return ", invalid, "; }\n\tr = uint(a);\n"));
			} else if (v == VarType::UInt32 && r == VarType::Int32) {
				f = unaryBody("toInt32", v, r,
						mem_std::toString("\tif (a > 2147483647u) { r = 0; return ", invalid,
								"; }\n\tr = int(a);\n"));
			} else if (v == VarType::Float32 && r == VarType::Int32) {
				f = unaryBody("toInt32", v, r,
						mem_std::toString("\tif (isnan(a) || isinf(a) || !(a >= -2147483648.0 && a < 2147483648.0)) { r = 0; return ",
								invalid, "; }\n\tr = int(a);\n"));
			} else if (v == VarType::Int32 && r == VarType::Float32) {
				f = unaryBody("toFloat32", v, r, "\tr = float(a);\n");
			} else {
				return false;
			}
		} else {
			return false;
		}
		return true;
	}

	return false;
}

} // namespace

void attachShaderForm(const OpDef &def, mem_std::String &name, mem_std::String &source, bool &inexact) {
	name.clear();
	source.clear();
	inexact = false;

	auto tmpl = templateOf(def.inlineName);
	if (tmpl == StringView("flowSequence") || tmpl == StringView("flowEvent")) {
		name = "@fireAll";
		return;
	}
	if (tmpl == StringView("flowBranch")) {
		name = "@branch";
		return;
	}
	if (tmpl == StringView("sceneGetInt") || tmpl == StringView("sceneGetScalar")) {
		name = "@sceneGet";
		return;
	}
	if (tmpl == StringView("sceneSetInt")) {
		name = "@sceneSet";
		return;
	}
	if (tmpl == StringView("sceneHas")) {
		name = "@sceneHas";
		return;
	}
	if (tmpl == StringView("valuePassthrough") && def.dataIn.size() == 1 && isGpuValue(def.dataIn[0].type)) {
		name = "@passthrough";
		return;
	}
	if (tmpl == StringView("enumFromInt32")) {
		name = "@widen";
		return;
	}
	if (tmpl == StringView("convertScalar") && def.dataIn.size() == 1 && def.dataOut.size() == 1) {
		auto in = def.dataIn[0].type;
		auto out = def.dataOut[0].type;
		if (!isGpuValue(in) && isGpuValue(out)) {
			name = "@narrow";
			return;
		}
		if (isGpuValue(in) && !isGpuValue(out)) {
			name = "@widen";
			return;
		}
	}

	Form f;
	if (deriveBody(def, f)) {
		name = sprt::move(f.name);
		source = sprt::move(f.source);
		inexact = f.inexact;
	}
}

} // namespace stappler::flow::ops
