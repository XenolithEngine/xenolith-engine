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

// The identity block, and what a unit is refused on.
//
// A generated unit carries no pointer, only names and hashes, and loading it is where the names are
// resolved and the hashes compared. Each way a process can have drifted from what the unit was
// written against is its own code - the node library changed (an operation missing or reshaped), the
// store's bookkeeping type or a record schema changed, the frame layout is not what the loader
// computes - because each is a different file to open.
//
// Every refusal here names its locus: a unit refused without one sends its owner reading the whole
// of its tables.

#include "codegen_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::codegenfx;
using stappler::test::hasDiag;

// The locus of the first entry with `code`; empty when there is none. `op` is the operation an
// entry names (DiagLocus::Op), `node` the node it stands at (DiagLocus::Node).
mem_std::String locusOf(const mem_std::Value &report, StringView code, StringView key) {
	if (!report.isArray()) {
		return mem_std::String();
	}
	for (auto &it : report.asArray()) {
		if (test::getDiagCodeName(it) == code) {
			if (key == StringView("node")) {
				return mem_std::toString(it.getValue("at").getInteger(0));
			}
			return mem_std::String(it.getValue("names").getString(0));
		}
	}
	return mem_std::String();
}

// A registry with the core library and the interpreter's types, as every host builds one.
bool prepareRegistry(OpRegistry &ops) {
	return ops.init() && stappler::flow::ops::registerCoreOps(ops) == Status::Ok
			&& Interpreter::registerCoreTypes(ops.getLocalTypes()) == Status::Ok;
}

Status opNothing(OpContext &) { return Status::Ok; }

} // namespace

void performCodegenIdentityTests() {
	sprt::cout << "\n== flow codegen: the identity block, and what a unit is refused on ==\n";

	auto walk = findUnit(StringView("walk"));
	auto loopUnit = findUnit(StringView("loop"));
	check(walk && loopUnit,
			"codegen-identity: the corpus units this section needs are committed (walk, loop)");
	if (!walk || !loopUnit) {
		return;
	}

	// ---- the good load, for reference --------------------------------------------------------------

	{
		OpRegistry ops;
		check(prepareRegistry(ops), "codegen-identity: the registry prepares");
		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(walk->tables(), ops, &diag) == Status::Ok && unit.isValid(),
				"codegen-identity: a unit loads against the shapes it was written against");
		check(!diag.isArray() || diag.size() == 0,
				"codegen-identity: with nothing to report");
		check(unit.getNodeCount() == 5 && unit.getScopeCount() == 1,
				"codegen-identity: and is the graph - the five nodes of the walk, one scope");
	}

	// ---- an operation that is missing, and one whose signature moved --------------------------------

	{
		OpRegistry ops;
		check(ops.init() && Interpreter::registerCoreTypes(ops.getLocalTypes()) == Status::Ok,
				"codegen-identity: a registry with no operations at all prepares");
		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(walk->tables(), ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a unit naming operations the process does not have is refused");
		check(hasDiag(diag, StringView("codegen-op-drift")),
				"codegen-identity: as codegen-op-drift");
		checkEq(StringView(locusOf(diag, StringView("codegen-op-drift"), StringView("op"))),
				StringView("debug.trace"),
				"codegen-identity: naming the first operation it could not find");
	}

	{
		// The same name, another shape: registered before the library, so the library's registration
		// - idempotent by name - leaves this one standing.
		OpRegistry ops;
		check(ops.init(), "codegen-identity: the drifted registry initialises");
		{
			PinDesc in[] = {
				PinDesc{.name = StringView("lhs"), .type = flow::value::VarType::Float},
				PinDesc{.name = StringView("rhs"), .type = flow::value::VarType::Float},
				PinDesc{.name = StringView("bias"), .type = flow::value::VarType::Float},
			};
			PinDesc out[] = {PinDesc{.name = StringView("result"), .type = flow::value::VarType::Float}};
			OpDef def;
			def.name = StringView("math.mulFloat");
			def.dataIn = SpanView<PinDesc>(in, 3);
			def.dataOut = SpanView<PinDesc>(out, 1);
			def.invoke = &opNothing;
			check(ops.createNative(def) != nullptr,
					"codegen-identity: math.mulFloat is registered with a third input");
		}
		check(stappler::flow::ops::registerCoreOps(ops) == Status::Ok
						&& Interpreter::registerCoreTypes(ops.getLocalTypes()) == Status::Ok,
				"codegen-identity: and the library registers around it");

		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(walk->tables(), ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a unit written against another signature is refused");
		check(hasDiag(diag, StringView("codegen-op-drift")),
				"codegen-identity: as codegen-op-drift");
		checkEq(StringView(locusOf(diag, StringView("codegen-op-drift"), StringView("op"))),
				StringView("math.mulFloat"), "codegen-identity: naming the operation that moved");
		check(!hasDiag(diag, StringView("codegen-schema-drift"))
						&& !hasDiag(diag, StringView("codegen-layout-drift")),
				"codegen-identity: and nothing else - a moved signature is one finding, not three");
	}

	{
		// The same pins, another parallel class: the class is part of the signature.
		OpRegistry ops;
		check(ops.init(), "codegen-identity: the reclassified registry initialises");
		{
			PinDesc in[] = {
				PinDesc{.name = StringView("lhs"), .type = flow::value::VarType::Float},
				PinDesc{.name = StringView("rhs"), .type = flow::value::VarType::Float},
			};
			PinDesc out[] = {PinDesc{.name = StringView("result"), .type = flow::value::VarType::Float}};
			OpDef def;
			def.name = StringView("math.mulFloat");
			def.dataIn = SpanView<PinDesc>(in, 2);
			def.dataOut = SpanView<PinDesc>(out, 1);
			def.flags = flow::OpFlags::Pure | flow::OpFlags::Infallible;
			def.invoke = &opNothing;
			check(ops.createNative(def) != nullptr,
					"codegen-identity: math.mulFloat is registered as serial");
		}
		check(stappler::flow::ops::registerCoreOps(ops) == Status::Ok
						&& Interpreter::registerCoreTypes(ops.getLocalTypes()) == Status::Ok,
				"codegen-identity: and the library registers around it");

		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(walk->tables(), ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a unit written against another parallel class is refused");
		checkEq(StringView(locusOf(diag, StringView("codegen-op-drift"), StringView("op"))),
				StringView("math.mulFloat"), "codegen-identity: as codegen-op-drift, naming it");
	}

	// ---- the store's bookkeeping type with another field -----------------------------------------------

	{
		OpRegistry ops;
		check(ops.init() && stappler::flow::ops::registerCoreOps(ops) == Status::Ok,
				"codegen-identity: the registry for the schema case prepares");
		{
			// interp.NodeState with a sixth field, registered before the interpreter's own - which is
			// idempotent by name and therefore keeps this one.
			flow::value::FieldDef fields[] = {
				flow::value::FieldDef{.name = StringView("inputs"), .type = flow::value::VarType::Int},
				flow::value::FieldDef{.name = StringView("produced"), .type = flow::value::VarType::Int},
				flow::value::FieldDef{.name = StringView("flags"), .type = flow::value::VarType::Int},
				flow::value::FieldDef{.name = StringView("stallPin"), .type = flow::value::VarType::Int},
				flow::value::FieldDef{.name = StringView("turns"), .type = flow::value::VarType::Int},
				flow::value::FieldDef{.name = StringView("extra"), .type = flow::value::VarType::Int},
			};
			check(ops.getLocalTypes().createNative(StringView("interp.NodeState"),
						  SpanView<flow::value::FieldDef>(fields, 6))
							!= nullptr,
					"codegen-identity: interp.NodeState is registered with a sixth field");
		}
		check(Interpreter::registerCoreTypes(ops.getLocalTypes()) == Status::Ok,
				"codegen-identity: and the interpreter's registration leaves it standing");

		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(walk->tables(), ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a unit laid out over another interp.NodeState is refused");
		check(hasDiag(diag, StringView("codegen-schema-drift")),
				"codegen-identity: as codegen-schema-drift");
		checkEq(StringView(locusOf(diag, StringView("codegen-schema-drift"), StringView("op"))),
				StringView("interp.NodeState"), "codegen-identity: naming the type");
		check(!hasDiag(diag, StringView("codegen-op-drift")),
				"codegen-identity: while the operations themselves are fine");
	}

	// ---- the same fields, moved ---------------------------------------------------------------------
	//
	// Two halves, because the claim rests on two separate facts and an argument that joins them is
	// not a test.
	//
	// A generated unit folds the offset of a record field into itself as a constant. That is only
	// safe while a process whose records keep those fields somewhere else cannot load the unit at
	// all. So: a schema with the same fields at different offsets must hash differently, and a unit
	// whose committed hash is not the live one must be refused, naming the operation.

	{
		OpRegistry ops;
		check(ops.init(), "codegen-identity: the registry for the reorder case initialises");

		// `flow.forEach`'s record is `item`, `index` and the `cursor` local. The same three fields
		// declared in another order is the same set at other offsets - and nothing else about the
		// operation would have changed: same names, same types, same signature.
		flow::value::FieldDef asDeclared[] = {
			flow::value::FieldDef{.name = StringView("item"), .type = flow::value::VarType::Int},
			flow::value::FieldDef{.name = StringView("index"), .type = flow::value::VarType::Int},
			flow::value::FieldDef{.name = StringView("cursor"), .type = flow::value::VarType::Int},
		};
		flow::value::FieldDef reordered[] = {
			flow::value::FieldDef{.name = StringView("index"), .type = flow::value::VarType::Int},
			flow::value::FieldDef{.name = StringView("item"), .type = flow::value::VarType::Int},
			flow::value::FieldDef{.name = StringView("cursor"), .type = flow::value::VarType::Int},
		};
		auto a = ops.getLocalTypes().createNative(StringView("probe.asDeclared"),
				SpanView<flow::value::FieldDef>(asDeclared, 3));
		auto b = ops.getLocalTypes().createNative(StringView("probe.reordered"),
				SpanView<flow::value::FieldDef>(reordered, 3));
		check(a != nullptr && b != nullptr, "codegen-identity: both spellings of the record register");
		if (a && b) {
			auto itemA = a->getField(StringView("item"));
			auto itemB = b->getField(StringView("item"));
			check(itemA && itemB && itemA->offset != itemB->offset,
					"codegen-identity: the same field sits at a different offset in the two");
			check(a->getSchemaHash() != b->getSchemaHash(),
					"codegen-identity: and the schema hash says so - offsets are inside it");
		}
	}

	{
		OpRegistry ops;
		check(prepareRegistry(ops), "codegen-identity: the registry for the record case prepares");

		// The committed unit, with the record hash of one operation replaced. The tables are views,
		// so a copy of the identity's op array over a patched row is a unit claiming a record layout
		// this process does not have - which is what a reordered schema would look like from here.
		auto tables = loopUnit->tables();
		mem_std::Vector<flow::CompiledOpIdentity> idOps(tables.identity.ops.begin(),
				tables.identity.ops.end());
		StringView moved;
		for (auto &it : idOps) {
			if (it.localSchemaHash != 0) {
				it.localSchemaHash ^= 0x5eedull;
				moved = it.name;
				break;
			}
		}
		check(!moved.empty(), "codegen-identity: the unit has an operation with a record");
		tables.identity.ops =
				SpanView<flow::CompiledOpIdentity>(idOps.data(), idOps.size());

		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(tables, ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a unit whose record layout is not this process's is refused");
		check(hasDiag(diag, StringView("codegen-schema-drift")),
				"codegen-identity: as codegen-schema-drift");
		checkEq(StringView(locusOf(diag, StringView("codegen-schema-drift"), StringView("op"))),
				moved, "codegen-identity: naming the operation whose record moved");
		check(!hasDiag(diag, StringView("codegen-op-drift")),
				"codegen-identity: while the operation's own signature is untouched");
	}

	// ---- a layout the loader does not compute ---------------------------------------------------------

	{
		OpRegistry ops;
		check(prepareRegistry(ops), "codegen-identity: the registry for the layout case prepares");

		// The unit's own tables, with one slot moved: the tables are views, so a copy of the view
		// over a patched slot array is a unit that says something the store does not.
		auto tables = walk->tables();
		mem_std::Vector<flow::FrameSlot> slots(tables.identity.slots.begin(),
				tables.identity.slots.end());
		uint32_t patched = flow::InvalidIndex;
		for (uint32_t n = 0; n < uint32_t(slots.size()); ++n) {
			if (slots[n].recordOffset != flow::InvalidIndex) {
				slots[n].recordOffset += 16;
				patched = n;
				break;
			}
		}
		check(patched != flow::InvalidIndex, "codegen-identity: the walk has a node with a record");
		tables.identity.slots = SpanView<flow::FrameSlot>(slots.data(), slots.size());

		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(tables, ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a unit whose frame layout is not the store's is refused");
		check(hasDiag(diag, StringView("codegen-layout-drift")),
				"codegen-identity: as codegen-layout-drift");
		checkEq(StringView(locusOf(diag, StringView("codegen-layout-drift"), StringView("node"))),
				StringView(mem_std::toString(tables.nodes[patched].id)),
				"codegen-identity: naming the node whose record moved");
	}

	// ---- a table that does not describe a graph ---------------------------------------------------------

	{
		OpRegistry ops;
		check(prepareRegistry(ops), "codegen-identity: the registry for the malformed case prepares");
		auto tables = walk->tables();
		mem_std::Vector<flow::RuntimeNode> nodes(tables.nodes.begin(), tables.nodes.end());
		nodes[0].execOutCount = 1000;
		tables.nodes = SpanView<flow::RuntimeNode>(nodes.data(), nodes.size());

		flow::CompiledGraph unit;
		mem_std::Value diag;
		check(unit.load(tables, ops, &diag) != Status::Ok && !unit.isValid(),
				"codegen-identity: a slice past the end of a table is refused before anything "
				"follows it");
		check(hasDiag(diag, StringView("codegen-malformed")),
				"codegen-identity: as codegen-malformed");
	}

	// ---- the documents a graph took function bodies from ------------------------------------------

	{
		struct Library : FunctionHost {
			GraphAsset inc;
			const GraphAsset *findFunction(StringView name) const override {
				return name == StringView("inc") ? &inc : nullptr;
			}
		} lib;
		lib.inc.init();
		check(lib.inc.load(data::read<mem_std::Interface>(StringView(R"json({"__meta": {"kind": "graph",
			"version": 2}, "name": "inc", "interface": {"inputs": [{"name": "x", "type": "int"}],
			"outputs": [{"name": "y", "type": "int"}]},
			"nodes": [{"id": 1, "op": "fn.entry"}, {"id": 2, "op": "math.addInt", "params": {"rhs": 1}},
				{"id": 3, "op": "fn.return"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "x", "to": 2, "toPin": "lhs"},
				{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "y"}]})json")))
						== Status::Ok,
				"codegen-identity: the library function loads");

		Fixture fx;
		fx.functions = &lib;
		check(fx.prepare(StringView(R"json({"formatVersion": 1, "name": "caller",
			"nodes": [{"id": 1, "op": "fn.inc", "params": {"x": 41}}], "edges": []})json")),
				"codegen-identity: a graph calling a library function builds");

		codegen::EmitOptions options;
		options.name = StringView("callee_probe");
		codegen::Emitted out;
		check(codegen::emit(fx.graph, fx.asset, fx.ops, options, out) == Status::Ok,
				"codegen-identity: and is written as a unit");
		check(out.header.find("CompiledCalleeIdentity") != mem_std::String::npos
						&& sourcesHave(out, StringView("SPFlowFunctionInline.h")),
				"codegen-identity: the unit names its callee and includes the function bodies");

		CompiledCalleeIdentity callees[] = {
			{.name = StringView("inc"), .contentHash = lib.inc.getContentHash()}};
		CompiledIdentity identity;
		identity.callees = SpanView<CompiledCalleeIdentity>(callees, 1);
		check(checkUnitCallees(identity, fx.graph.getLink()) == Status::Ok,
				"codegen-identity: the callee a host links is the one the unit was written from");

		mem_std::Value diag;
		callees[0].contentHash ^= 1;
		check(checkUnitCallees(identity, fx.graph.getLink(), &diag) != Status::Ok
						&& locusOf(diag, StringView("codegen-callee-drift"), StringView("op"))
								== "inc",
				"codegen-identity: a callee that changed since is codegen-callee-drift, by name");

		diag = mem_std::Value();
		CompiledIdentity none;
		check(checkUnitCallees(none, fx.graph.getLink(), &diag) != Status::Ok
						&& hasDiag(diag, StringView("codegen-callee-drift")),
				"codegen-identity: and so is a callee the unit never had");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
