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

// The parallel class of every shipped operation: the table, the target pin and scene
// access it relies on, the hash rule, and the registrations the class refuses.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowOps.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::hasDiag;

namespace {

using namespace stappler::flow;
namespace ops = stappler::flow::ops;
using flow::value::VarType;

struct ClassCase {
	StringView name;
	OpParallel parallel;
};

const ClassCase s_classes[] = {
	{"flow.event", OpParallel::Flow},
	{"flow.sequence", OpParallel::Flow},
	{"flow.branch", OpParallel::Flow},
	{"flow.forEach", OpParallel::Flow},
	{"math.addFloat", OpParallel::Pure},
	{"math.subFloat", OpParallel::Pure},
	{"math.mulFloat", OpParallel::Pure},
	{"math.addInt", OpParallel::Pure},
	{"math.mulInt", OpParallel::Pure},
	{"logic.and", OpParallel::Pure},
	{"logic.or", OpParallel::Pure},
	{"logic.not", OpParallel::Pure},
	{"compare.lessFloat", OpParallel::Pure},
	{"compare.equalInt", OpParallel::Pure},
	{"compare.lessInt", OpParallel::Pure},
	{"convert.intToFloat", OpParallel::Pure},
	{"array.getInt", OpParallel::Pure},
	{"value.float", OpParallel::Pure},
	{"value.int", OpParallel::Pure},
	{"value.bool", OpParallel::Pure},
	{"string.concat", OpParallel::Pure},
	{"value.string", OpParallel::Pure},
	{"debug.trace", OpParallel::Flow},
	{"wait.timer", OpParallel::Serial},
	{"await.component", OpParallel::Serial},
	{"scene.global", OpParallel::SceneRead},
	{"scene.named", OpParallel::SceneRead},
	{"scene.has", OpParallel::SceneRead},
	{"scene.getInt", OpParallel::SceneRead},
	{"scene.setInt", OpParallel::SceneWrite},
	{"scene.findByField", OpParallel::SceneRead},
	{"scene.addComponent", OpParallel::Serial},
	{"scene.removeComponent", OpParallel::Serial},
	{"scene.getInt32", OpParallel::SceneRead},
	{"scene.setInt32", OpParallel::SceneWrite},
	{"scene.getUInt32", OpParallel::SceneRead},
	{"scene.setUInt32", OpParallel::SceneWrite},
	{"scene.getFloat32", OpParallel::SceneRead},
	{"scene.setFloat32", OpParallel::SceneWrite},
	{"scene.getEnum", OpParallel::SceneRead},
	{"scene.setEnum", OpParallel::SceneWrite},
	{"math.subInt", OpParallel::Pure},
	{"math.divInt", OpParallel::Pure},
	{"math.minInt", OpParallel::Pure},
	{"math.maxInt", OpParallel::Pure},
	{"math.absInt", OpParallel::Pure},
	{"math.divFloat", OpParallel::Pure},
	{"math.minFloat", OpParallel::Pure},
	{"math.maxFloat", OpParallel::Pure},
	{"math.absFloat", OpParallel::Pure},
	{"compare.equalFloat", OpParallel::Pure},
	{"math.addInt32", OpParallel::Pure},
	{"math.subInt32", OpParallel::Pure},
	{"math.mulInt32", OpParallel::Pure},
	{"math.divInt32", OpParallel::Pure},
	{"math.minInt32", OpParallel::Pure},
	{"math.maxInt32", OpParallel::Pure},
	{"math.absInt32", OpParallel::Pure},
	{"compare.lessInt32", OpParallel::Pure},
	{"compare.equalInt32", OpParallel::Pure},
	{"math.addUInt32", OpParallel::Pure},
	{"math.subUInt32", OpParallel::Pure},
	{"math.mulUInt32", OpParallel::Pure},
	{"math.divUInt32", OpParallel::Pure},
	{"math.minUInt32", OpParallel::Pure},
	{"math.maxUInt32", OpParallel::Pure},
	{"compare.lessUInt32", OpParallel::Pure},
	{"compare.equalUInt32", OpParallel::Pure},
	{"math.addFloat32", OpParallel::Pure},
	{"math.subFloat32", OpParallel::Pure},
	{"math.mulFloat32", OpParallel::Pure},
	{"math.divFloat32", OpParallel::Pure},
	{"math.minFloat32", OpParallel::Pure},
	{"math.maxFloat32", OpParallel::Pure},
	{"math.absFloat32", OpParallel::Pure},
	{"compare.lessFloat32", OpParallel::Pure},
	{"compare.equalFloat32", OpParallel::Pure},
	{"math.floorFloat", OpParallel::Pure},
	{"math.ceilFloat", OpParallel::Pure},
	{"math.sqrtFloat", OpParallel::Pure},
	{"math.sinFloat", OpParallel::Pure},
	{"math.cosFloat", OpParallel::Pure},
	{"math.tanFloat", OpParallel::Pure},
	{"math.powFloat", OpParallel::Pure},
	{"math.floorFloat32", OpParallel::Pure},
	{"math.ceilFloat32", OpParallel::Pure},
	{"math.sqrtFloat32", OpParallel::Pure},
	{"math.sinFloat32", OpParallel::Pure},
	{"math.cosFloat32", OpParallel::Pure},
	{"math.tanFloat32", OpParallel::Pure},
	{"math.powFloat32", OpParallel::Pure},
	{"math.addVec2", OpParallel::Pure},
	{"math.subVec2", OpParallel::Pure},
	{"math.scaleVec2", OpParallel::Pure},
	{"math.dotVec2", OpParallel::Pure},
	{"math.lengthVec2", OpParallel::Pure},
	{"math.normalizeVec2", OpParallel::Pure},
	{"math.addVec3", OpParallel::Pure},
	{"math.subVec3", OpParallel::Pure},
	{"math.scaleVec3", OpParallel::Pure},
	{"math.dotVec3", OpParallel::Pure},
	{"math.lengthVec3", OpParallel::Pure},
	{"math.normalizeVec3", OpParallel::Pure},
	{"math.addVec4", OpParallel::Pure},
	{"math.subVec4", OpParallel::Pure},
	{"math.scaleVec4", OpParallel::Pure},
	{"math.dotVec4", OpParallel::Pure},
	{"math.lengthVec4", OpParallel::Pure},
	{"math.normalizeVec4", OpParallel::Pure},
	{"convert.floatToInt", OpParallel::Pure},
	{"convert.intToInt32", OpParallel::Pure},
	{"convert.intToUInt32", OpParallel::Pure},
	{"convert.int32ToUInt32", OpParallel::Pure},
	{"convert.uInt32ToInt32", OpParallel::Pure},
	{"convert.floatToFloat32", OpParallel::Pure},
	{"convert.float32ToInt32", OpParallel::Pure},
	{"convert.int32ToFloat32", OpParallel::Pure},
	{"enum.toInt32", OpParallel::Pure},
	{"enum.fromInt32", OpParallel::Pure},
	{"value.int32", OpParallel::Pure},
	{"value.uint32", OpParallel::Pure},
	{"value.float32", OpParallel::Pure},
	{"value.vec2", OpParallel::Pure},
	{"value.vec3", OpParallel::Pure},
	{"value.vec4", OpParallel::Pure},
	{"value.color", OpParallel::Pure},
	{"par.forEach", OpParallel::Flow},
	{"par.forEachWith", OpParallel::Flow},
	{"par.barrier", OpParallel::Flow},
	{"par.gatherBool", OpParallel::Pure},
	{"par.gatherInt", OpParallel::Pure},
	{"par.gatherFloat", OpParallel::Pure},
	{"par.gatherInt32", OpParallel::Pure},
	{"par.gatherUInt32", OpParallel::Pure},
	{"par.gatherFloat32", OpParallel::Pure},
	{"par.gatherVec2", OpParallel::Pure},
	{"par.gatherVec3", OpParallel::Pure},
	{"par.gatherVec4", OpParallel::Pure},
	{"par.gatherColor", OpParallel::Pure},
	{"par.gatherEntityRef", OpParallel::Pure},
	{"par.sumInt", OpParallel::Pure},
	{"par.sumFloat", OpParallel::Pure},
	{"par.sumInt32", OpParallel::Pure},
	{"par.sumUInt32", OpParallel::Pure},
	{"par.sumFloat32", OpParallel::Pure},
	{"par.minInt", OpParallel::Pure},
	{"par.minFloat", OpParallel::Pure},
	{"par.minInt32", OpParallel::Pure},
	{"par.minUInt32", OpParallel::Pure},
	{"par.minFloat32", OpParallel::Pure},
	{"par.maxInt", OpParallel::Pure},
	{"par.maxFloat", OpParallel::Pure},
	{"par.maxInt32", OpParallel::Pure},
	{"par.maxUInt32", OpParallel::Pure},
	{"par.maxFloat32", OpParallel::Pure},
	{"par.sumVec2", OpParallel::Pure},
	{"par.sumVec3", OpParallel::Pure},
	{"par.sumVec4", OpParallel::Pure},
	{"par.count", OpParallel::Pure},
	{"par.any", OpParallel::Pure},
	{"par.all", OpParallel::Pure},
};

Status noopInvoke(OpContext &) { return Status::Ok; }

PinDesc pin(StringView name, VarType type, PinRole role = PinRole::None) {
	PinDesc p;
	p.name = name;
	p.type = type;
	p.role = role;
	return p;
}

const StringView s_then[] = {StringView("then")};

// A fresh registry per attempt, so a refusal leaves nothing behind for the next case.
bool refused(const OpDef &def, StringView code = StringView("op-parallel-invalid")) {
	OpRegistry reg;
	reg.init();
	mem_std::Value diag;
	auto op = reg.createNative(def, &diag);
	return !op && hasDiag(diag, code);
}

bool accepted(const OpDef &def) {
	OpRegistry reg;
	reg.init();
	return reg.createNative(def) != nullptr;
}

} // namespace

void performParallelClassifyTests() {
	sprt::cout << "\n== flow ops: parallel classes ==\n";

	OpRegistry reg;
	check(reg.init() && ops::registerCoreOps(reg) == Status::Ok,
			"parallel-classify: the library registers");

	// ---- the table -----------------------------------------------------------------------------------

	{
		bool listed = true;
		for (uint32_t i = 0; i < reg.getCount(); ++i) {
			auto name = reg.getAt(i)->getName();
			bool found = false;
			for (auto &c : s_classes) { found = found || c.name == name; }
			if (!found) {
				listed = false;
				sprt::cout << "       " << name << ": not in the table\n";
			}
		}
		check(listed, "parallel-classify: every registered operation is in the table");

		bool match = true;
		for (auto &c : s_classes) {
			auto op = reg.get(c.name);
			if (!op || op->getParallel() != c.parallel) {
				match = false;
				sprt::cout << "       " << c.name << ": expected "
						   << getOpParallelName(c.parallel) << ", got "
						   << (op ? getOpParallelName(op->getParallel()) : StringView("<missing>"))
						   << "\n";
			}
		}
		check(match, "parallel-classify: every operation has the class the table names");
	}

	// ---- names ---------------------------------------------------------------------------------------

	{
		bool roundTrip = true;
		for (uint32_t i = 0; i < OpParallelCount; ++i) {
			OpParallel back = OpParallel::Serial;
			roundTrip = roundTrip && readOpParallel(getOpParallelName(OpParallel(i)), back)
					&& back == OpParallel(i);
		}
		OpParallel unused;
		check(roundTrip && !readOpParallel(StringView("parallel"), unused),
				"parallel-classify: class names read back, and an unknown one does not");
		check(getPinRoleName(PinRole::EntityTarget) == StringView("target"),
				"parallel-classify: the target role is named");
	}

	// ---- the target and the access -------------------------------------------------------------------

	{
		auto getInt = reg.get(StringView("scene.getInt"));
		auto setInt = reg.get(StringView("scene.setInt"));
		auto find = reg.get(StringView("scene.findByField"));
		auto has = reg.get(StringView("scene.has"));
		auto global = reg.get(StringView("scene.global"));
		auto timer = reg.get(StringView("wait.timer"));

		check(getInt && getInt->getTargetPin() == 0 && setInt && setInt->getTargetPin() == 0
						&& has && has->getTargetPin() == 0,
				"parallel-classify: scene.getInt, setInt and has address their `target` pin");
		check(find && find->getTargetPin() == NullPin && global
						&& global->getTargetPin() == NullPin,
				"parallel-classify: findByField and global have no target");

		check(getInt && getInt->getSceneGroups().size() == 1
						&& getInt->getSceneGroups()[0].access == SceneAccess::Read,
				"parallel-classify: a value on an output is a read");
		check(setInt && setInt->getSceneGroups().size() == 1
						&& setInt->getSceneGroups()[0].access == SceneAccess::Write,
				"parallel-classify: a value on an input of a writing operation is a write");
		check(find && find->getSceneGroups().size() == 1
						&& find->getSceneGroups()[0].access == SceneAccess::Read,
				"parallel-classify: the key of findByField is a read");
		check(has && has->getSceneGroups().size() == 1
						&& has->getSceneGroups()[0].access == SceneAccess::Read,
				"parallel-classify: a group with no value takes the operation's flags");

		auto groups = timer ? timer->getSceneGroups() : SpanView<SceneGroup>();
		auto refs = timer ? timer->getSceneRefs() : SpanView<SceneRef>();
		check(groups.size() == 3 && refs.size() == 3 && groups[0].access == SceneAccess::Read
						&& !refs[0].targeted && groups[1].access == SceneAccess::Write
						&& refs[1].targeted && groups[2].access == SceneAccess::Write
						&& refs[2].targeted,
				"parallel-classify: wait.timer reads the frame and writes its target's timer");
	}

	// ---- the hash ------------------------------------------------------------------------------------

	{
		PinDesc in[] = {pin("lhs", VarType::Float)};
		PinDesc out[] = {pin("result", VarType::Float)};
		OpDef def;
		def.name = StringView("probe.class");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.invoke = &noopInvoke;
		OpDef pure = def;
		pure.parallel = OpParallel::Pure;
		OpDef local = def;
		local.parallel = OpParallel::Local;

		OpRegistry a, b, c;
		a.init();
		b.init();
		c.init();
		auto serialOp = a.createNative(def);
		auto pureOp = b.createNative(pure);
		auto localOp = c.createNative(local);
		check(serialOp && pureOp && localOp
						&& serialOp->getSignatureHash() != pureOp->getSignatureHash()
						&& pureOp->getSignatureHash() != localOp->getSignatureHash(),
				"parallel-classify: a class other than serial is part of the signature hash");

		mem_std::Value described;
		if (pureOp) {
			pureOp->describe(described);
		}
		mem_std::Value serialDescribed;
		if (serialOp) {
			serialOp->describe(serialDescribed);
		}
		check(described.getString("parallel") == StringView("pure")
						&& !serialDescribed.hasValue("parallel"),
				"parallel-classify: describe names a class, and says nothing for serial");
	}

	// ---- what registration refuses -------------------------------------------------------------------

	{
		// Pure: no exec pins, no locals, no scene.
		OpDef exec;
		exec.name = StringView("probe.pureExec");
		exec.hasExecIn = true;
		exec.execOut = SpanView<StringView>(s_then, 1);
		exec.parallel = OpParallel::Pure;
		exec.invoke = &noopInvoke;
		check(refused(exec), "parallel-classify: a pure operation with exec pins is refused");

		flow::value::FieldDef locals[] = {flow::value::FieldDef{.name = StringView("memo"), .type = VarType::Int}};
		OpDef withLocals;
		withLocals.name = StringView("probe.pureLocals");
		withLocals.locals = SpanView<flow::value::FieldDef>(locals, 1);
		withLocals.parallel = OpParallel::Pure;
		withLocals.invoke = &noopInvoke;
		check(refused(withLocals), "parallel-classify: a pure operation with locals is refused");
		withLocals.parallel = OpParallel::Local;
		check(accepted(withLocals), "parallel-classify: the same locals are fine for a local one");

		OpDef reads;
		reads.name = StringView("probe.flowReads");
		reads.hasExecIn = true;
		reads.execOut = SpanView<StringView>(s_then, 1);
		reads.flags = OpFlags::ReadsScene;
		reads.parallel = OpParallel::Flow;
		reads.invoke = &noopInvoke;
		check(refused(reads), "parallel-classify: a flow operation reaching the scene is refused");

		OpDef host;
		host.name = StringView("probe.host");
		host.flags = OpFlags::HostCall;
		host.parallel = OpParallel::Local;
		host.invoke = &noopInvoke;
		check(refused(host), "parallel-classify: a host call is refused outside serial");

		OpDef scope;
		scope.name = StringView("probe.scope");
		scope.hasExecIn = true;
		scope.execOut = SpanView<StringView>(s_then, 1);
		scope.scopeExecOut = 1;
		scope.parallel = OpParallel::Local;
		scope.invoke = &noopInvoke;
		check(refused(scope), "parallel-classify: only a flow operation may open a scope");
		scope.parallel = OpParallel::Flow;
		check(accepted(scope), "parallel-classify: and a flow one may");

		// SceneRead: reads, never writes.
		PinDesc readIn[] = {pin("target", VarType::EntityRef, PinRole::EntityTarget),
			pin("component", VarType::String, PinRole::ComponentName),
			pin("field", VarType::String, PinRole::FieldName)};
		PinDesc readOut[] = {pin("value", VarType::Int, PinRole::FieldValue)};
		OpDef sceneRead;
		sceneRead.name = StringView("probe.read");
		sceneRead.dataIn = SpanView<PinDesc>(readIn, 3);
		sceneRead.dataOut = SpanView<PinDesc>(readOut, 1);
		sceneRead.flags = OpFlags::ReadsScene | OpFlags::WritesScene;
		sceneRead.parallel = OpParallel::SceneRead;
		sceneRead.invoke = &noopInvoke;
		check(refused(sceneRead), "parallel-classify: a scene read that may write is refused");
		sceneRead.flags = OpFlags::ReadsScene;
		check(accepted(sceneRead), "parallel-classify: a scene read that only reads is accepted");

		// SceneWrite: a fixed-size field of the target.
		PinDesc writeIn[] = {pin("target", VarType::EntityRef, PinRole::EntityTarget),
			pin("component", VarType::String, PinRole::ComponentName),
			pin("field", VarType::String, PinRole::FieldName),
			pin("value", VarType::Int, PinRole::FieldValue)};
		OpDef sceneWrite;
		sceneWrite.name = StringView("probe.write");
		sceneWrite.dataIn = SpanView<PinDesc>(writeIn, 4);
		sceneWrite.flags = OpFlags::WritesScene;
		sceneWrite.parallel = OpParallel::SceneWrite;
		sceneWrite.invoke = &noopInvoke;
		check(accepted(sceneWrite), "parallel-classify: a targeted fixed-size write is accepted");

		PinDesc untargetedIn[] = {pin("target", VarType::EntityRef),
			pin("component", VarType::String, PinRole::ComponentName),
			pin("field", VarType::String, PinRole::FieldName),
			pin("value", VarType::Int, PinRole::FieldValue)};
		OpDef untargeted = sceneWrite;
		untargeted.dataIn = SpanView<PinDesc>(untargetedIn, 4);
		check(refused(untargeted), "parallel-classify: a write with no target pin is refused");

		PinDesc containerIn[] = {pin("target", VarType::EntityRef, PinRole::EntityTarget),
			pin("component", VarType::String, PinRole::ComponentName),
			pin("field", VarType::String, PinRole::FieldName),
			pin("value", VarType::String, PinRole::FieldValue)};
		OpDef container = sceneWrite;
		container.dataIn = SpanView<PinDesc>(containerIn, 4);
		check(refused(container), "parallel-classify: a write of a container value is refused");

		PinDesc structuralIn[] = {pin("target", VarType::EntityRef, PinRole::EntityTarget),
			pin("component", VarType::String, PinRole::ComponentName)};
		OpDef structural = sceneWrite;
		structural.dataIn = SpanView<PinDesc>(structuralIn, 2);
		check(refused(structural), "parallel-classify: adding a component is refused outside serial");
		structural.parallel = OpParallel::Serial;
		check(accepted(structural), "parallel-classify: and accepted as serial");

		SceneRef untargetedRef[] = {SceneRef{.component = StringView("probe.Clock"),
			.field = StringView("ticks"), .type = VarType::Int, .access = SceneAccess::Write}};
		OpDef refWrite;
		refWrite.name = StringView("probe.refWrite");
		refWrite.dataIn = SpanView<PinDesc>(writeIn, 1);
		refWrite.sceneRefs = SpanView<SceneRef>(untargetedRef, 1);
		refWrite.flags = OpFlags::WritesScene;
		refWrite.parallel = OpParallel::SceneWrite;
		refWrite.invoke = &noopInvoke;
		check(refused(refWrite), "parallel-classify: a reference write that is not targeted is refused");

		// The target role itself.
		PinDesc twoTargets[] = {pin("a", VarType::EntityRef, PinRole::EntityTarget),
			pin("b", VarType::EntityRef, PinRole::EntityTarget)};
		OpDef twice;
		twice.name = StringView("probe.twoTargets");
		twice.dataIn = SpanView<PinDesc>(twoTargets, 2);
		twice.invoke = &noopInvoke;
		check(refused(twice, StringView("op-pin-role-invalid")),
				"parallel-classify: a second target pin is refused");

		PinDesc intTarget[] = {pin("a", VarType::Int, PinRole::EntityTarget)};
		OpDef wrongType;
		wrongType.name = StringView("probe.intTarget");
		wrongType.dataIn = SpanView<PinDesc>(intTarget, 1);
		wrongType.invoke = &noopInvoke;
		check(refused(wrongType, StringView("op-pin-role-invalid")),
				"parallel-classify: a target that is not an EntityRef is refused");

		SceneRef targetedRef[] = {SceneRef{.component = StringView("probe.Clock"), .targeted = true}};
		OpDef noTarget;
		noTarget.name = StringView("probe.noTarget");
		noTarget.sceneRefs = SpanView<SceneRef>(targetedRef, 1);
		noTarget.invoke = &noopInvoke;
		check(refused(noTarget, StringView("op-pin-role-invalid")),
				"parallel-classify: a targeted reference without a target pin is refused");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
