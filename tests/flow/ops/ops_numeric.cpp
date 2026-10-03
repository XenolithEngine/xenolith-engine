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

// The numeric library over every scalar type: each operation run on a one-node graph at
// the values where it is easiest to get wrong - wrap-around, MIN / -1, a zero divisor, a range edge,
// a zero vector - and the conversions that refuse. Plus the 32-bit scene pair on a real scene.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../interp/interp_fixture.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::flow;
using flow::value::Var;
using flow::value::VarType;

enum class Outcome {
	Value, // completed, and output 0 holds exactly `expect`
	Refused, // the operation refused: the run ends in op-error
};

struct Case {
	StringView op;
	StringView params;
	Outcome outcome;
	Var expect;
};

// A pure node with every input a parameter is an entry node, so a one-node graph runs it.
Outcome runCase(const Case &c, Var &out, mem_std::String &why) {
	stappler::test::interpfx::Fixture fx;
	mem_std::Value node(mem_std::Value::Type::DICTIONARY);
	node.setInteger(1, "id");
	node.setString(c.op, "op");
	node.setValue(data::read<mem_std::Interface>(c.params), "params");

	mem_std::Value graph(mem_std::Value::Type::DICTIONARY);
	graph.setInteger(1, "formatVersion");
	graph.newArray("nodes").addValue(sprt::move(node));
	graph.newArray("edges");

	if (!fx.prepare(data::toString<mem_std::Interface>(graph))) {
		why = "the graph does not build";
		return Outcome::Refused;
	}
	fx.run();
	if (fx.report.outcome == RunOutcome::OpError) {
		return Outcome::Refused;
	}
	if (fx.report.outcome != RunOutcome::Completed || !fx.output(1, 0, out)) {
		why = mem_std::toString("outcome ", getRunOutcomeName(fx.report.outcome));
		return Outcome::Refused;
	}
	return Outcome::Value;
}

Var vec2(float x, float y) { return flow::value::makeVec2(x, y); }
Var vec3(float x, float y, float z) { return flow::value::makeVec3(x, y, z); }
Var vec4(float x, float y, float z, float w) { return flow::value::makeVec4(x, y, z, w); }

const Case s_cases[] = {
	// Int and Float, the operations they lacked.
	{"math.subInt", R"({"lhs": 5, "rhs": 7})", Outcome::Value, flow::value::makeInt(-2)},
	{"math.subInt", R"({"lhs": -9223372036854775808, "rhs": 1})", Outcome::Value,
		flow::value::makeInt(9'223'372'036'854'775'807LL)},
	{"math.divInt", R"({"lhs": 7, "rhs": -2})", Outcome::Value, flow::value::makeInt(-3)},
	{"math.divInt", R"({"lhs": 7, "rhs": 0})", Outcome::Refused, Var()},
	{"math.divInt", R"({"lhs": -9223372036854775808, "rhs": -1})", Outcome::Value,
		flow::value::makeInt(-9'223'372'036'854'775'807LL - 1)},
	{"math.minInt", R"({"lhs": 3, "rhs": -4})", Outcome::Value, flow::value::makeInt(-4)},
	{"math.maxInt", R"({"lhs": 3, "rhs": -4})", Outcome::Value, flow::value::makeInt(3)},
	{"math.absInt", R"({"value": -9})", Outcome::Value, flow::value::makeInt(9)},
	{"math.absInt", R"({"value": -9223372036854775808})", Outcome::Value,
		flow::value::makeInt(-9'223'372'036'854'775'807LL - 1)},
	{"math.divFloat", R"({"lhs": 1.0, "rhs": 4.0})", Outcome::Value, flow::value::makeFloat(0.25)},
	{"math.minFloat", R"({"lhs": 0.5, "rhs": -0.5})", Outcome::Value, flow::value::makeFloat(-0.5)},
	{"math.maxFloat", R"({"lhs": 0.5, "rhs": -0.5})", Outcome::Value, flow::value::makeFloat(0.5)},
	{"math.absFloat", R"({"value": -2.5})", Outcome::Value, flow::value::makeFloat(2.5)},
	{"compare.equalFloat", R"({"lhs": 0.5, "rhs": 0.5})", Outcome::Value, flow::value::makeBool(true)},

	// Int32: wraps modulo 2^32.
	{"math.addInt32", R"({"lhs": 2147483647, "rhs": 1})", Outcome::Value,
		flow::value::makeInt32(-2'147'483'647 - 1)},
	{"math.subInt32", R"({"lhs": -2147483648, "rhs": 1})", Outcome::Value,
		flow::value::makeInt32(2'147'483'647)},
	{"math.mulInt32", R"({"lhs": 65536, "rhs": 65536})", Outcome::Value, flow::value::makeInt32(0)},
	{"math.divInt32", R"({"lhs": -7, "rhs": 2})", Outcome::Value, flow::value::makeInt32(-3)},
	{"math.divInt32", R"({"lhs": 1, "rhs": 0})", Outcome::Refused, Var()},
	{"math.divInt32", R"({"lhs": -2147483648, "rhs": -1})", Outcome::Value,
		flow::value::makeInt32(-2'147'483'647 - 1)},
	{"math.minInt32", R"({"lhs": -1, "rhs": 1})", Outcome::Value, flow::value::makeInt32(-1)},
	{"math.maxInt32", R"({"lhs": -1, "rhs": 1})", Outcome::Value, flow::value::makeInt32(1)},
	{"math.absInt32", R"({"value": -2147483648})", Outcome::Value,
		flow::value::makeInt32(-2'147'483'647 - 1)},
	{"compare.lessInt32", R"({"lhs": -1, "rhs": 0})", Outcome::Value, flow::value::makeBool(true)},
	{"compare.equalInt32", R"({"lhs": 4, "rhs": 5})", Outcome::Value, flow::value::makeBool(false)},

	// UInt32.
	{"math.addUInt32", R"({"lhs": 4294967295, "rhs": 1})", Outcome::Value, flow::value::makeUInt32(0)},
	{"math.subUInt32", R"({"lhs": 0, "rhs": 1})", Outcome::Value,
		flow::value::makeUInt32(4'294'967'295u)},
	{"math.mulUInt32", R"({"lhs": 65536, "rhs": 65536})", Outcome::Value, flow::value::makeUInt32(0)},
	{"math.divUInt32", R"({"lhs": 7, "rhs": 2})", Outcome::Value, flow::value::makeUInt32(3)},
	{"math.divUInt32", R"({"lhs": 7, "rhs": 0})", Outcome::Refused, Var()},
	{"math.minUInt32", R"({"lhs": 1, "rhs": 4294967295})", Outcome::Value, flow::value::makeUInt32(1)},
	{"math.maxUInt32", R"({"lhs": 1, "rhs": 4294967295})", Outcome::Value,
		flow::value::makeUInt32(4'294'967'295u)},
	{"compare.lessUInt32", R"({"lhs": 1, "rhs": 4294967295})", Outcome::Value,
		flow::value::makeBool(true)},
	{"compare.equalUInt32", R"({"lhs": 7, "rhs": 7})", Outcome::Value, flow::value::makeBool(true)},

	// Float32.
	{"math.addFloat32", R"({"lhs": 0.5, "rhs": 0.25})", Outcome::Value, flow::value::makeFloat32(0.75f)},
	{"math.subFloat32", R"({"lhs": 0.5, "rhs": 0.25})", Outcome::Value, flow::value::makeFloat32(0.25f)},
	{"math.mulFloat32", R"({"lhs": 1.5, "rhs": -2.0})", Outcome::Value, flow::value::makeFloat32(-3.0f)},
	{"math.divFloat32", R"({"lhs": 1.0, "rhs": 8.0})", Outcome::Value, flow::value::makeFloat32(0.125f)},
	{"math.minFloat32", R"({"lhs": 1.0, "rhs": 2.0})", Outcome::Value, flow::value::makeFloat32(1.0f)},
	{"math.maxFloat32", R"({"lhs": 1.0, "rhs": 2.0})", Outcome::Value, flow::value::makeFloat32(2.0f)},
	{"math.absFloat32", R"({"value": -0.5})", Outcome::Value, flow::value::makeFloat32(0.5f)},
	{"compare.lessFloat32", R"({"lhs": 0.1, "rhs": 0.2})", Outcome::Value, flow::value::makeBool(true)},
	{"compare.equalFloat32", R"({"lhs": 0.1, "rhs": 0.1})", Outcome::Value, flow::value::makeBool(true)},

	// The real functions, at values every implementation agrees on.
	{"math.floorFloat", R"({"value": -1.5})", Outcome::Value, flow::value::makeFloat(-2.0)},
	{"math.ceilFloat", R"({"value": -1.5})", Outcome::Value, flow::value::makeFloat(-1.0)},
	{"math.sqrtFloat", R"({"value": 9.0})", Outcome::Value, flow::value::makeFloat(3.0)},
	{"math.sinFloat", R"({"value": 0.0})", Outcome::Value, flow::value::makeFloat(0.0)},
	{"math.cosFloat", R"({"value": 0.0})", Outcome::Value, flow::value::makeFloat(1.0)},
	{"math.tanFloat", R"({"value": 0.0})", Outcome::Value, flow::value::makeFloat(0.0)},
	{"math.powFloat", R"({"lhs": 2.0, "rhs": 10.0})", Outcome::Value, flow::value::makeFloat(1024.0)},
	{"math.floorFloat32", R"({"value": 2.5})", Outcome::Value, flow::value::makeFloat32(2.0f)},
	{"math.ceilFloat32", R"({"value": 2.5})", Outcome::Value, flow::value::makeFloat32(3.0f)},
	{"math.sqrtFloat32", R"({"value": 16.0})", Outcome::Value, flow::value::makeFloat32(4.0f)},
	{"math.sinFloat32", R"({"value": 0.0})", Outcome::Value, flow::value::makeFloat32(0.0f)},
	{"math.cosFloat32", R"({"value": 0.0})", Outcome::Value, flow::value::makeFloat32(1.0f)},
	{"math.tanFloat32", R"({"value": 0.0})", Outcome::Value, flow::value::makeFloat32(0.0f)},
	{"math.powFloat32", R"({"lhs": 3.0, "rhs": 2.0})", Outcome::Value, flow::value::makeFloat32(9.0f)},

	// Vectors.
	{"math.addVec3", R"({"lhs": [1, 2, 3], "rhs": [4, 5, 6]})", Outcome::Value, vec3(5, 7, 9)},
	{"math.subVec2", R"({"lhs": [1, 2], "rhs": [4, 8]})", Outcome::Value, vec2(-3, -6)},
	{"math.scaleVec4", R"({"lhs": [1, 2, 3, 4], "rhs": 0.5})", Outcome::Value,
		vec4(0.5f, 1.0f, 1.5f, 2.0f)},
	{"math.dotVec3", R"({"lhs": [1, 2, 3], "rhs": [4, 5, 6]})", Outcome::Value,
		flow::value::makeFloat32(32.0f)},
	{"math.lengthVec2", R"({"value": [3, 4]})", Outcome::Value, flow::value::makeFloat32(5.0f)},
	{"math.normalizeVec2", R"({"value": [0, 2]})", Outcome::Value, vec2(0.0f, 1.0f)},
	{"math.normalizeVec3", R"({"value": [0, 0, 0]})", Outcome::Value, vec3(0, 0, 0)},

	// Conversions with a range guard.
	{"convert.floatToInt", R"({"value": -2.9})", Outcome::Value, flow::value::makeInt(-2)},
	{"convert.floatToInt", R"({"value": 1.0e30})", Outcome::Refused, Var()},
	{"convert.intToInt32", R"({"value": 2147483647})", Outcome::Value,
		flow::value::makeInt32(2'147'483'647)},
	{"convert.intToInt32", R"({"value": 2147483648})", Outcome::Refused, Var()},
	{"convert.intToUInt32", R"({"value": 4294967295})", Outcome::Value,
		flow::value::makeUInt32(4'294'967'295u)},
	{"convert.intToUInt32", R"({"value": -1})", Outcome::Refused, Var()},
	{"convert.int32ToUInt32", R"({"value": -1})", Outcome::Refused, Var()},
	{"convert.uInt32ToInt32", R"({"value": 4294967295})", Outcome::Refused, Var()},
	{"convert.uInt32ToInt32", R"({"value": 5})", Outcome::Value, flow::value::makeInt32(5)},
	{"convert.floatToFloat32", R"({"value": 0.1})", Outcome::Value, flow::value::makeFloat32(0.1f)},
	{"convert.floatToFloat32", R"({"value": 1.0e39})", Outcome::Refused, Var()},
	{"convert.float32ToInt32", R"({"value": -2.5})", Outcome::Value, flow::value::makeInt32(-2)},
	{"convert.float32ToInt32", R"({"value": 3.0e9})", Outcome::Refused, Var()},
	{"convert.int32ToFloat32", R"({"value": 16777217})", Outcome::Value,
		flow::value::makeFloat32(16'777'216.0f)},

	// Enums.
	{"enum.toInt32", R"({"value": {"value": 7, "type": 0}})", Outcome::Value, flow::value::makeInt32(7)},
	{"enum.toInt32", R"({"value": {"value": 1099511627776, "type": 0}})", Outcome::Refused, Var()},
	{"enum.fromInt32", R"({"value": 3, "family": "Dir"})", Outcome::Value, flow::value::makeEnum(3)},

	// Literals of the types that had none.
	{"value.int32", R"({"value": -5})", Outcome::Value, flow::value::makeInt32(-5)},
	{"value.uint32", R"({"value": 5})", Outcome::Value, flow::value::makeUInt32(5)},
	{"value.float32", R"({"value": 0.5})", Outcome::Value, flow::value::makeFloat32(0.5f)},
	{"value.vec2", R"({"value": [1, 2]})", Outcome::Value, vec2(1, 2)},
	{"value.vec3", R"({"value": [1, 2, 3]})", Outcome::Value, vec3(1, 2, 3)},
	{"value.vec4", R"({"value": [1, 2, 3, 4]})", Outcome::Value, vec4(1, 2, 3, 4)},
	{"value.color", R"({"value": [0.25, 0.5, 0.75, 1]})", Outcome::Value,
		flow::value::makeColor(0.25f, 0.5f, 0.75f, 1.0f)},
};

} // namespace

void performOpsNumericTests() {
	sprt::cout << "\n== flow ops: the numeric library ==\n";

	uint32_t failed = 0;
	for (auto &c : s_cases) {
		Var out;
		mem_std::String why;
		auto got = runCase(c, out, why);
		bool ok = got == c.outcome && (got == Outcome::Refused || flow::value::varBytesEqual(out, c.expect));
		if (!ok) {
			++failed;
			mem_std::Value shown;
			flow::value::encodeVar(out, shown);
			sprt::cout << "       " << c.op << " " << c.params << ": "
					   << (got == Outcome::Refused ? StringView("refused") : StringView("value"))
					   << " " << data::toString<mem_std::Interface>(shown) << " " << why << "\n";
		}
	}
	check(failed == 0,
			mem_std::toString("ops-numeric: every case gives its value or its refusal (",
					sizeof(s_cases) / sizeof(s_cases[0]), " cases)"));

}

} // namespace STAPPLER_VERSIONIZED stappler