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

// Subtask E5: the local-variable schema derived from an operation's signature.
//
// The derivation writes no layout of its own. It builds a FieldDef list and hands it to the same
// ComponentType::build the native and data-driven paths use, so the offsets below are the storage
// layer's arithmetic, not this layer's - and they are identical on linux, win32 and wasm32 for the
// same reason every other schema's are. The golden hash is the check that says so.
//
// What the derivation decides, and what this section pins down:
//
//   * one field per data OUTPUT, then whatever the operation declared. Inputs are not fields: a node
//     reads an input out of the record of whoever produced it.
//   * the schema belongs to the SIGNATURE, so every node of one operation shares one descriptor.
//   * an operation with neither outputs nor locals gets no schema at all, because a zero-field
//     component would become a pool with a stride of zero.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowOp.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow;
using flow::value::FieldDef;
using flow::value::VarType;

// The one reader of a report entry's code, for every section (check/studio_check.h).
using stappler::test::hasDiag;

Status noopInvoke(OpContext &) { return Status::Ok; }

PinDesc pin(StringView name, VarType type, ElementChain element = 0) {
	PinDesc p;
	p.name = name;
	p.type = type;
	p.element = element;
	return p;
}

FieldDef local(StringView name, VarType type) {
	FieldDef f;
	f.name = name;
	f.type = type;
	return f;
}

} // namespace

void performGraphLocalsTests() {
	sprt::cout << "\n== flow graph: derived local schemas ==\n";

	// ---- the fixture: every storage class an output can have --------------------------------------

	{
		OpRegistry reg;
		check(reg.init(), "graph-locals: the registry initialises");

		PinDesc in[] = {pin("seed", VarType::Int)};
		PinDesc out[] = {pin("flag", VarType::Bool), pin("count", VarType::Int),
			pin("where", VarType::Vec3), pin("label", VarType::String),
			pin("items", VarType::Array, flow::value::makeChain(VarType::Int))};
		FieldDef locals[] = {local("cursor", VarType::Int)};

		OpDef def;
		def.name = StringView("test.mixed");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 5);
		def.locals = SpanView<FieldDef>(locals, 1);
		def.invoke = &noopInvoke;

		auto op = reg.createNative(def);
		check(op != nullptr, "graph-locals: the operation registers");

		auto schema = op->getLocalSchema();
		check(schema != nullptr, "graph-locals: an operation with outputs gets a local schema");
		check(schema->getName() == "op.test.mixed.locals",
				"graph-locals: the schema is named after the operation");

		// One field per output, then the declared locals - and NOT the input.
		check(schema->getFields().size() == 6,
				"graph-locals: five outputs and one declared local make six fields");
		check(schema->getField(StringView("seed")) == nullptr,
				"graph-locals: an input is not a field of the local record");
		check(schema->getField(StringView("cursor")) != nullptr,
				"graph-locals: a declared local is a field");

		auto fields = schema->getFields();
		check(fields[0].name == "flag" && fields[4].name == "items" && fields[5].name == "cursor",
				"graph-locals: outputs keep their order and the locals follow them");

		mem_std::Value dump;
		schema->describe(dump);

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json({
			"name": "op.test.mixed.locals",
			"id": )json"
				+ mem_std::toString(
						int64_t(flow::value::makeTypeId(StringView("op.test.mixed.locals"))))
				+ R"json(,
			"hash": )json"
				+ mem_std::toString(int64_t(0x2bda'da70'5c79'7878ull)) + R"json(,
			"size": 64,
			"align": 8,
			"fields": [
				{"name": "flag", "type": "bool", "offset": 0, "size": 1, "align": 1},
				{"name": "<padding>", "offset": 1, "size": 7},
				{"name": "count", "type": "int", "offset": 8, "size": 8, "align": 8},
				{"name": "where", "type": "vec3", "offset": 16, "size": 12, "align": 4},
				{"name": "label", "type": "string", "offset": 28, "size": 12, "align": 4},
				{"name": "items", "type": "array", "element": ["int"], "offset": 40, "size": 12,
					"align": 4},
				{"name": "<padding>", "offset": 52, "size": 4},
				{"name": "cursor", "type": "int", "offset": 56, "size": 8, "align": 8}
			]
		})json"));

		check(test::compareValues(dump, expect, StringView("derived schema")),
				"graph-locals: the derived layout matches the golden one");

		// The schema is registered in the operation registry's OWN type registry, not in a scene's.
		check(reg.getLocalTypes().getCount() == 1,
				"graph-locals: the derived type lives in the registry's own type registry");
		check(reg.getLocalTypes().get(StringView("op.test.mixed.locals")) == schema,
				"graph-locals: the registered type is the one the operation points at");
	}

	// ---- one schema per signature, not per node ---------------------------------------------------

	{
		OpRegistry reg;
		reg.init();

		PinDesc out[] = {pin("value", VarType::Int)};
		OpDef a;
		a.name = StringView("test.a");
		a.dataOut = SpanView<PinDesc>(out, 1);
		a.invoke = &noopInvoke;
		OpDef b = a;
		b.name = StringView("test.b");

		auto opA = reg.createNative(a);
		auto opB = reg.createNative(b);
		check(opA != nullptr && opB != nullptr, "graph-locals: two operations register");

		check(opA->getLocalSchema() == reg.get(StringView("test.a"))->getLocalSchema(),
				"graph-locals: every lookup of one operation yields the same descriptor");
		check(opA->getLocalSchema() != opB->getLocalSchema(),
				"graph-locals: two operations do not share a schema, even with identical fields");

		// Identical field lists, different names - so different hashes, exactly as for any two
		// component types.
		check(opA->getLocalSchema()->getSchemaHash() != opB->getLocalSchema()->getSchemaHash(),
				"graph-locals: the schema hash carries the operation's name");
		check(reg.getLocalTypes().getCount() == 2, "graph-locals: one derived type per operation");
	}

	// ---- an operation that needs no record gets none -----------------------------------------------

	{
		OpRegistry reg;
		reg.init();

		PinDesc in[] = {pin("condition", VarType::Bool)};
		StringView execOut[] = {StringView("true"), StringView("false")};
		OpDef def;
		def.name = StringView("flow.branch");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(execOut, 2);
		def.invoke = &noopInvoke;

		auto op = reg.createNative(def);
		check(op != nullptr && op->getLocalSchema() == nullptr,
				"graph-locals: no outputs and no locals means no schema");
		check(reg.getLocalTypes().getCount() == 0,
				"graph-locals: and no type is registered for it either");

		// ... while an operation with ONLY declared locals does get one: the record is needed, it is
		// simply nobody's output.
		FieldDef locals[] = {local("remaining", VarType::Float)};
		OpDef timer;
		timer.name = StringView("flow.wait");
		timer.hasExecIn = true;
		timer.execOut = SpanView<StringView>(execOut, 1);
		timer.locals = SpanView<FieldDef>(locals, 1);
		timer.invoke = &noopInvoke;

		auto wait = reg.createNative(timer);
		check(wait != nullptr && wait->getLocalSchema() != nullptr
						&& wait->getLocalSchema()->getFields().size() == 1,
				"graph-locals: an operation with only declared locals gets a one-field schema");
	}

	// ---- a collision is refused, and leaves nothing behind ------------------------------------------

	{
		OpRegistry reg;
		reg.init();

		PinDesc out[] = {pin("result", VarType::Int)};
		FieldDef clash[] = {local("result", VarType::Float)};

		OpDef def;
		def.name = StringView("test.clash");
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.locals = SpanView<FieldDef>(clash, 1);
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr
						&& hasDiag(diag, StringView("op-local-invalid")),
				"graph-locals: a local that shadows an output is refused");
		check(reg.getCount() == 0 && reg.getLocalTypes().getCount() == 0,
				"graph-locals: the refused registration left neither operation nor type");

		// The point of leaving nothing behind: the corrected signature can take the same name.
		FieldDef fixed[] = {local("carry", VarType::Float)};
		def.locals = SpanView<FieldDef>(fixed, 1);
		auto op = reg.createNative(def);
		check(op != nullptr && op->getLocalSchema() != nullptr
						&& op->getLocalSchema()->getName() == "op.test.clash.locals",
				"graph-locals: the corrected signature registers under the same name");
	}

	// ---- a local the schema layer will not accept ---------------------------------------------------

	{
		OpRegistry reg;
		reg.init();

		// An array field has to declare its element; the derivation does not get to decide otherwise.
		FieldDef bad[] = {local("items", VarType::Array)};
		OpDef def;
		def.name = StringView("test.badlocal");
		def.locals = SpanView<FieldDef>(bad, 1);
		def.invoke = &noopInvoke;

		mem_std::Value diag;
		check(reg.createNative(def, &diag) == nullptr
						&& hasDiag(diag, StringView("op-local-invalid")),
				"graph-locals: a local the schema layer refuses refuses the operation");
		check(reg.getCount() == 0 && reg.getLocalTypes().getCount() == 0,
				"graph-locals: and nothing is left behind");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
