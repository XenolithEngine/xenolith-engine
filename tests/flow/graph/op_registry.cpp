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

// Subtask E2: the operation registry.
//
// The registry is the only thing in layer L2 that knows what an operation IS, and it deliberately
// knows nothing about any particular one: everything exercised here is registered by this file. A
// standard library of nodes is a separate module, and the proof that it can be is that the registry
// never mentions one.
//
// Two properties carry more weight than the rest and are checked hardest:
//
//   * The signature hash is a golden literal. It has to be the same number on linux, win32 and
//     wasm32, because an asset stores it and a drift is reported by comparing them. A hash that
//     picked its width from the pointer size would pass on the host and fail nowhere anyone looks.
//   * A pin default is CONVERTED once, at registration, and stored in the pin's own type. That is
//     the first half of "nothing decides a type at run time"; the second half is the edge,
//     which arrives with the build.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowOp.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow;
using flow::value::VarType;

// Does the report carry an entry with this code? The whole report is compared against a golden one
// in graph-validate; here the code alone is the question, because the message is prose.
// The one reader of a report entry's code, for every section (check/studio_check.h).
using stappler::test::hasDiag;

Status noopInvoke(OpContext &) { return Status::Ok; }

PinDesc pin(StringView name, VarType type) {
	PinDesc p;
	p.name = name;
	p.type = type;
	return p;
}

PinDesc pinDef(StringView name, VarType type, mem_std::Value &&def) {
	PinDesc p;
	p.name = name;
	p.type = type;
	p.def = sprt::move(def);
	return p;
}

// ---- the fixture registry ----------------------------------------------------------------------

const OpDesc *registerAdd(OpRegistry &reg, mem_std::Value *diag = nullptr) {
	PinDesc in[] = {pin("lhs", VarType::Float), pinDef("rhs", VarType::Float, mem_std::Value(1))};
	PinDesc out[] = {pin("result", VarType::Float)};
	OpDef def;
	def.name = StringView("math.add");
	def.dataIn = SpanView<PinDesc>(in, 2);
	def.dataOut = SpanView<PinDesc>(out, 1);
	def.flags = OpFlags::Pure;
	def.invoke = &noopInvoke;
	return reg.createNative(def, diag);
}

const OpDesc *registerBranch(OpRegistry &reg, mem_std::Value *diag = nullptr) {
	PinDesc in[] = {pin("condition", VarType::Bool)};
	StringView execOut[] = {StringView("true"), StringView("false")};
	OpDef def;
	def.name = StringView("flow.branch");
	def.dataIn = SpanView<PinDesc>(in, 1);
	def.hasExecIn = true;
	def.execOut = SpanView<StringView>(execOut, 2);
	def.invoke = &noopInvoke;
	return reg.createNative(def, diag);
}

} // namespace

void performOpRegistryTests() {
	sprt::cout << "\n== flow graph: the operation registry ==\n";

	{
		OpRegistry reg;
		check(reg.init(), "op-registry: the registry initialises");

		auto add = registerAdd(reg);
		auto branch = registerBranch(reg);
		check(add != nullptr && branch != nullptr, "op-registry: both operations registered");
		check(reg.getCount() == 2, "op-registry: the registry holds two operations");

		// ---- identity ----------------------------------------------------------------------------

		check(add->getId() == flow::value::makeTypeId(StringView("math.add")),
				"op-registry: the id is the hash of the name, nothing else");
		check(reg.get(StringView("math.add")) == add && reg.get(add->getId()) == add,
				"op-registry: lookup by name and by id find the same descriptor");
		check(reg.get(StringView("math.sub")) == nullptr,
				"op-registry: an unregistered name returns nullptr");
		check(reg.getAt(0) == add && reg.getAt(1) == branch,
				"op-registry: registration order is the enumeration order");

		// The number an asset stores. Identical on every ABI - that is the whole reason it exists.
		check(add->getSignatureHash() == 0x13a2'c3e6'f0a7'559cull,
				"op-registry: the signature hash of math.add is the golden literal");
		check(branch->getSignatureHash() == 0xab1d'0b74'5dba'f743ull,
				"op-registry: the signature hash of flow.branch is the golden literal");

		// ---- pins --------------------------------------------------------------------------------

		uint32_t idx = 0xffff'ffffu;
		check(add->findDataIn(StringView("rhs"), idx) && idx == 1,
				"op-registry: a data input is found by name");
		check(add->findDataOut(StringView("result"), idx) && idx == 0,
				"op-registry: a data output is found by name");
		check(!add->findDataIn(StringView("result"), idx),
				"op-registry: an output is not found among the inputs");
		check(branch->findExecOut(StringView("false"), idx) && idx == 1,
				"op-registry: an exec output is found by name, in declaration order");
		check(!branch->findExecOut(StringView("maybe"), idx),
				"op-registry: an unknown exec output is not found");
		check(branch->hasExecIn() && !add->hasExecIn(),
				"op-registry: the exec input is recorded as declared");

		// ---- a default is converted once, at registration -----------------------------------------

		auto &rhs = add->getDataIn()[1];
		check(rhs.def.isDouble() && rhs.def.getDouble() == 1.0,
				"op-registry: the integer literal 1 on a Float pin is stored as 1.0");
		check(add->getDataIn()[0].def.isNull(),
				"op-registry: a pin with no default keeps an empty value");
	}

	// ---- registration refuses what cannot be executed --------------------------------------------

	{
		OpRegistry reg;
		reg.init();
		registerAdd(reg);

		mem_std::Value diag;
		auto again = registerAdd(reg, &diag);
		check(again == nullptr && hasDiag(diag, StringView("op-duplicate")),
				"op-registry: registering a name twice is refused");
		check(reg.getCount() == 1, "op-registry: the refused registration left nothing behind");
	}

	{
		OpRegistry reg;
		reg.init();

		// Nil is not a type a pin may have: the layer's own field validator says so, and this layer
		// does not get a second opinion.
		PinDesc in[] = {pin("value", VarType::Nil)};
		OpDef def;
		def.name = StringView("bad.nil");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr
						&& hasDiag(diag, StringView("op-pin-invalid")),
				"op-registry: a Nil pin is refused");
	}

	{
		OpRegistry reg;
		reg.init();

		// An array pin has to declare its element - the same rule a component field obeys.
		PinDesc in[] = {pin("items", VarType::Array)};
		OpDef def;
		def.name = StringView("bad.array");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr
						&& hasDiag(diag, StringView("op-pin-invalid")),
				"op-registry: an Array pin with no element is refused");

		PinDesc scalar[] = {pin("count", VarType::Int)};
		scalar[0].element = flow::value::makeChain(VarType::Int);
		OpDef def2;
		def2.name = StringView("bad.chain");
		def2.dataIn = SpanView<PinDesc>(scalar, 1);
		def2.invoke = &noopInvoke;

		mem_std::Value diag2;
		check(reg.createNative(def2, &diag2) == nullptr
						&& hasDiag(diag2, StringView("op-pin-invalid")),
				"op-registry: a scalar pin that declares an element is refused");
	}

	{
		OpRegistry reg;
		reg.init();

		PinDesc in[] = {pin("a", VarType::Int), pin("a", VarType::Float)};
		OpDef def;
		def.name = StringView("bad.dup");
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr
						&& hasDiag(diag, StringView("op-pin-duplicate")),
				"op-registry: two inputs with one name are refused");

		// An input and an output MAY share a name: they are addressed separately by every edge.
		PinDesc sameIn[] = {pin("value", VarType::Int)};
		PinDesc sameOut[] = {pin("value", VarType::Int)};
		OpDef ok;
		ok.name = StringView("good.passthrough");
		ok.dataIn = SpanView<PinDesc>(sameIn, 1);
		ok.dataOut = SpanView<PinDesc>(sameOut, 1);
		ok.invoke = &noopInvoke;
		check(reg.createNative(ok) != nullptr,
				"op-registry: an input and an output may share a name");

		StringView execOut[] = {StringView("next"), StringView("next")};
		OpDef dupExec;
		dupExec.name = StringView("bad.exec");
		dupExec.hasExecIn = true;
		dupExec.execOut = SpanView<StringView>(execOut, 2);
		dupExec.invoke = &noopInvoke;

		mem_std::Value execDiag;
		check(reg.createNative(dupExec, &execDiag) == nullptr
						&& hasDiag(execDiag, StringView("op-pin-duplicate")),
				"op-registry: two exec outputs with one name are refused");
	}

	{
		OpRegistry reg;
		reg.init();

		// The limit is not decoration: layer L3 will keep one bit per input in a machine word, and
		// this is the only place that can refuse a signature that would not fit.
		mem_std::Vector<PinDesc> many;
		for (uint32_t i = 0; i <= MaxDataPins; ++i) {
			many.emplace_back(pin(mem_std::toString("p", i), VarType::Int));
		}
		OpDef def;
		def.name = StringView("bad.wide");
		def.dataIn = SpanView<PinDesc>(many.data(), many.size());
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr && hasDiag(diag, StringView("op-pin-limit")),
				"op-registry: more than MaxDataPins inputs are refused");

		mem_std::Vector<StringView> manyExec;
		mem_std::Vector<mem_std::String> execNames;
		for (uint32_t i = 0; i <= MaxExecOut; ++i) {
			execNames.emplace_back(mem_std::toString("e", i));
		}
		for (auto &it : execNames) { manyExec.emplace_back(StringView(it)); }
		OpDef wideExec;
		wideExec.name = StringView("bad.wide-exec");
		wideExec.hasExecIn = true;
		wideExec.execOut = SpanView<StringView>(manyExec.data(), manyExec.size());
		wideExec.invoke = &noopInvoke;

		mem_std::Value execDiag;
		check(reg.createNative(wideExec, &execDiag) == nullptr
						&& hasDiag(execDiag, StringView("op-pin-limit")),
				"op-registry: more than MaxExecOut exec outputs are refused");
	}

	// ---- defaults are checked by VALUE, not by type ----------------------------------------------

	{
		OpRegistry reg;
		reg.init();

		PinDesc bad[] = {pinDef("count", VarType::Int, mem_std::Value(3.5))};
		OpDef def;
		def.name = StringView("bad.default");
		def.dataIn = SpanView<PinDesc>(bad, 1);
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr
						&& hasDiag(diag, StringView("op-default-invalid")),
				"op-registry: 3.5 is refused as an Int default");

		// ... while 3.0 is not: the check is whether THIS value survives the conversion, and it does.
		PinDesc good[] = {pinDef("count", VarType::Int, mem_std::Value(3.0))};
		OpDef ok;
		ok.name = StringView("good.default");
		ok.dataIn = SpanView<PinDesc>(good, 1);
		ok.invoke = &noopInvoke;
		auto desc = reg.createNative(ok);
		check(desc != nullptr && desc->getDataIn()[0].def.isInteger()
						&& desc->getDataIn()[0].def.getInteger() == 3,
				"op-registry: 3.0 becomes the Int default 3");

		// A string on a numeric pin is Parse - it reads bytes out of an arena, which no build has.
		PinDesc parsed[] = {pinDef("count", VarType::Int, mem_std::Value("5"))};
		OpDef str;
		str.name = StringView("bad.parse");
		str.dataIn = SpanView<PinDesc>(parsed, 1);
		str.invoke = &noopInvoke;

		mem_std::Value parseDiag;
		check(reg.createNative(str, &parseDiag) == nullptr
						&& hasDiag(parseDiag, StringView("op-default-invalid")),
				"op-registry: a string literal on an Int pin is refused");

		// A container default is kept verbatim: turning it into a value needs an arena.
		PinDesc text[] = {pinDef("label", VarType::String, mem_std::Value("hello"))};
		OpDef strDef;
		strDef.name = StringView("good.string");
		strDef.dataIn = SpanView<PinDesc>(text, 1);
		strDef.invoke = &noopInvoke;
		auto strDesc = reg.createNative(strDef);
		check(strDesc != nullptr && StringView(strDesc->getDataIn()[0].def.getString()) == "hello",
				"op-registry: a String default is kept as written");

		PinDesc wrong[] = {pinDef("label", VarType::String, mem_std::Value(7))};
		OpDef wrongDef;
		wrongDef.name = StringView("bad.string");
		wrongDef.dataIn = SpanView<PinDesc>(wrong, 1);
		wrongDef.invoke = &noopInvoke;

		mem_std::Value wrongDiag;
		check(reg.createNative(wrongDef, &wrongDiag) == nullptr
						&& hasDiag(wrongDiag, StringView("op-default-invalid")),
				"op-registry: an integer is refused as a String default");

		// A vector has a structural spelling and only that one.
		mem_std::Value vec(mem_std::Value::Type::ARRAY);
		vec.addDouble(1.0);
		vec.addDouble(2.0);
		vec.addDouble(3.0);
		PinDesc vecPin[] = {pinDef("offset", VarType::Vec3, sprt::move(vec))};
		OpDef vecDef;
		vecDef.name = StringView("good.vec");
		vecDef.dataIn = SpanView<PinDesc>(vecPin, 1);
		vecDef.invoke = &noopInvoke;
		auto vecDesc = reg.createNative(vecDef);
		check(vecDesc != nullptr && vecDesc->getDataIn()[0].def.size() == 3,
				"op-registry: a Vec3 default is accepted in its structural form");

		mem_std::Value shortVec(mem_std::Value::Type::ARRAY);
		shortVec.addDouble(1.0);
		PinDesc shortPin[] = {pinDef("offset", VarType::Vec3, sprt::move(shortVec))};
		OpDef shortDef;
		shortDef.name = StringView("bad.vec");
		shortDef.dataIn = SpanView<PinDesc>(shortPin, 1);
		shortDef.invoke = &noopInvoke;

		mem_std::Value shortDiag;
		check(reg.createNative(shortDef, &shortDiag) == nullptr
						&& hasDiag(shortDiag, StringView("op-default-invalid")),
				"op-registry: a two-short Vec3 default is refused");
	}

	// ---- what the signature hash covers, and what it does not ------------------------------------

	{
		OpRegistry reg;
		reg.init();
		auto base = registerAdd(reg);

		// Flags are behaviour, not wiring: an operation that starts writing the scene did not change
		// shape, and an asset written against it is still wired correctly.
		PinDesc in[] = {pin("lhs", VarType::Float),
			pinDef("rhs", VarType::Float, mem_std::Value(2))};
		PinDesc out[] = {pin("result", VarType::Float)};
		OpDef flagged;
		flagged.name = StringView("math.add2");
		flagged.dataIn = SpanView<PinDesc>(in, 2);
		flagged.dataOut = SpanView<PinDesc>(out, 1);
		flagged.flags = OpFlags::WritesScene;
		flagged.invoke = &noopInvoke;
		auto other = reg.createNative(flagged);

		// ... but the NAME is part of it, so these two differ. Compare a same-named rebuild instead.
		OpRegistry reg2;
		reg2.init();
		OpDef renamed = flagged;
		renamed.name = StringView("math.add");
		auto rebuilt = reg2.createNative(renamed);
		check(rebuilt != nullptr && rebuilt->getSignatureHash() == base->getSignatureHash(),
				"op-registry: flags and defaults do not enter the signature hash");
		check(other != nullptr && other->getSignatureHash() != base->getSignatureHash(),
				"op-registry: the name does enter the signature hash");

		OpRegistry reg3;
		reg3.init();
		PinDesc retyped[] = {pin("lhs", VarType::Int),
			pinDef("rhs", VarType::Float, mem_std::Value(1))};
		OpDef typeChanged;
		typeChanged.name = StringView("math.add");
		typeChanged.dataIn = SpanView<PinDesc>(retyped, 2);
		typeChanged.dataOut = SpanView<PinDesc>(out, 1);
		typeChanged.invoke = &noopInvoke;
		auto changed = reg3.createNative(typeChanged);
		check(changed != nullptr && changed->getSignatureHash() != base->getSignatureHash(),
				"op-registry: retyping a pin changes the signature hash");

		OpRegistry reg4;
		reg4.init();
		PinDesc renamedPin[] = {pin("left", VarType::Float),
			pinDef("rhs", VarType::Float, mem_std::Value(1))};
		OpDef pinRenamed;
		pinRenamed.name = StringView("math.add");
		pinRenamed.dataIn = SpanView<PinDesc>(renamedPin, 2);
		pinRenamed.dataOut = SpanView<PinDesc>(out, 1);
		pinRenamed.invoke = &noopInvoke;
		auto pinChanged = reg4.createNative(pinRenamed);
		check(pinChanged != nullptr && pinChanged->getSignatureHash() != base->getSignatureHash(),
				"op-registry: renaming a pin changes the signature hash");
	}

	// ---- the dump ---------------------------------------------------------------------------------

	{
		OpRegistry reg;
		reg.init();
		registerAdd(reg);
		registerBranch(reg);

		mem_std::Value dump;
		reg.describe(dump);

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json([
			{
				"name": "math.add",
				"id": )json"
				+ mem_std::toString(int64_t(flow::value::makeTypeId(StringView("math.add"))))
				+ R"json(,
				"hash": )json"
				+ mem_std::toString(int64_t(0x13a2'c3e6'f0a7'559cull)) + R"json(,
				"flags": ["pure"],
				"dataIn": [
					{"name": "lhs", "type": "float"},
					{"name": "rhs", "type": "float", "default": 1.0}
				],
				"dataOut": [{"name": "result", "type": "float"}],
				"execIn": false,
				"execOut": [],
				"locals": "op.math.add.locals"
			},
			{
				"name": "flow.branch",
				"id": )json"
				+ mem_std::toString(int64_t(flow::value::makeTypeId(StringView("flow.branch"))))
				+ R"json(,
				"hash": )json"
				+ mem_std::toString(int64_t(0xab1d'0b74'5dba'f743ull)) + R"json(,
				"dataIn": [{"name": "condition", "type": "bool"}],
				"dataOut": [],
				"execIn": true,
				"execOut": ["true", "false"]
			}
		])json"));

		check(test::compareValues(dump, expect, StringView("op-registry dump")),
				"op-registry: the dump matches the golden one");
	}

	// ---- what a pin NAMES ------------------------------------------------------------------------
	//
	// A role says that a pin does not merely carry a string, it names something in the scene - and
	// the grouping is the pin ORDER, so registration is where the order is checked. It has to be
	// here: the build reads groups off the signature and cannot re-derive what a malformed one meant.

	{
		const StringView then[] = {StringView("then")};
		auto withPins = [&](OpRegistry &reg, SpanView<PinDesc> in, SpanView<PinDesc> out) {
			OpDef def;
			def.name = StringView("probe.roles");
			def.dataIn = in;
			def.dataOut = out;
			def.hasExecIn = true;
			def.execOut = SpanView<StringView>(then, 1);
			def.invoke = &noopInvoke;
			return reg.createNative(def);
		};
		auto roled = [](PinDesc &&p, PinRole role) {
			p.role = role;
			return p;
		};

		{
			OpRegistry reg;
			reg.init();
			PinDesc in[] = {roled(pin("component", VarType::String), PinRole::ComponentName),
				roled(pin("field", VarType::String), PinRole::FieldName)};
			PinDesc out[] = {roled(pin("value", VarType::Int), PinRole::FieldValue)};
			auto desc = withPins(reg, SpanView<PinDesc>(in, 2), SpanView<PinDesc>(out, 1));
			check(desc != nullptr, "op-registry: a well-ordered group registers");
			check(desc && desc->getSceneGroups().size() == 1,
					"op-registry: three roled pins are ONE group, not three");
			check(desc && desc->getSceneGroups()[0].componentPin == 0
							&& desc->getSceneGroups()[0].fieldPin == 1
							&& desc->getSceneGroups()[0].valuePin == 0
							&& desc->getSceneGroups()[0].valueIsOutput,
					"op-registry: and the group knows the value came back on an output");
		}

		{
			OpRegistry reg;
			reg.init();
			mem_std::Value diag;
			PinDesc in[] = {roled(pin("field", VarType::String), PinRole::FieldName)};
			OpDef def;
			def.name = StringView("probe.roles");
			def.dataIn = SpanView<PinDesc>(in, 1);
			def.invoke = &noopInvoke;
			check(reg.createNative(def, &diag) == nullptr
							&& hasDiag(diag, StringView("op-pin-role-invalid")),
					"op-registry: a field name with no component before it is refused");
		}

		{
			OpRegistry reg;
			reg.init();
			mem_std::Value diag;
			PinDesc in[] = {roled(pin("component", VarType::Int), PinRole::ComponentName)};
			OpDef def;
			def.name = StringView("probe.roles");
			def.dataIn = SpanView<PinDesc>(in, 1);
			def.invoke = &noopInvoke;
			check(reg.createNative(def, &diag) == nullptr
							&& hasDiag(diag, StringView("op-pin-role-invalid")),
					"op-registry: a component name on a pin that is not a String is refused");
		}

		{
			OpRegistry reg;
			reg.init();
			mem_std::Value diag;
			PinDesc in[] = {roled(pin("component", VarType::String), PinRole::ComponentName),
				roled(pin("value", VarType::Int), PinRole::FieldValue)};
			OpDef def;
			def.name = StringView("probe.roles");
			def.dataIn = SpanView<PinDesc>(in, 2);
			def.invoke = &noopInvoke;
			check(reg.createNative(def, &diag) == nullptr
							&& hasDiag(diag, StringView("op-pin-role-invalid")),
					"op-registry: a value in a group that names no field is refused");
		}

		// The role is WIRING, so it is in the hash: a pin that used to carry an ordinary string and
		// now names a component type is checked against the scene, and an asset written against the
		// other meaning should be told. Two signatures alike in every other respect prove it.
		{
			OpRegistry plain;
			OpRegistry named;
			plain.init();
			named.init();
			PinDesc bare[] = {pin("component", VarType::String)};
			PinDesc withRole[] = {roled(pin("component", VarType::String), PinRole::ComponentName)};
			auto a = withPins(plain, SpanView<PinDesc>(bare, 1), SpanView<PinDesc>());
			auto b = withPins(named, SpanView<PinDesc>(withRole, 1), SpanView<PinDesc>());
			check(a && b && a->getSignatureHash() != b->getSignatureHash(),
					"op-registry: giving a pin a role changes the signature hash");
			check(a && a->getSceneGroups().empty() && b && b->getSceneGroups().size() == 1,
					"op-registry: and only the roled one declares a group");
		}

		// What the operation names ITSELF is not wiring - no asset was written against it, and no
		// author can see it - so it stays out of the hash, exactly as OpFlags does.
		{
			OpRegistry plain;
			OpRegistry withRefs;
			plain.init();
			withRefs.init();
			SceneRef refs[] = {SceneRef{.component = StringView("probe.Clock"),
				.field = StringView("dt"), .type = VarType::Float}};

			OpDef bare;
			bare.name = StringView("probe.refs");
			bare.invoke = &noopInvoke;
			OpDef declared = bare;
			declared.sceneRefs = SpanView<SceneRef>(refs, 1);

			auto a = plain.createNative(bare);
			auto b = withRefs.createNative(declared);
			check(a && b && a->getSignatureHash() == b->getSignatureHash(),
					"op-registry: what an operation names in the scene itself is not in the hash");
			check(b && b->getSceneGroups().size() == 1 && b->getSceneRefs().size() == 1,
					"op-registry: but it is still a group, so a node gets a binding for it");
		}
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
