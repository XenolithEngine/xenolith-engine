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

// The parallel block: the two fan-outs, the barrier and the collectors. The build checks a block,
// the machine opens its branches and delivers it.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;
using flow::ScopeKind;
using flow::SettingDesc;
using flow::SettingRole;

#ifndef SP_FLOW_OPS_TBODY
#define SP_FLOW_OPS_TBODY(fn, arg) \
	Body{&inl::fn<arg, flow::OpContext>, StringView("flow::ops::inl::" #fn "<" #arg ">")}
#endif

Status registerParallelOps(OpRegistry &reg) {
	Registrar r{reg};

	static constexpr StringView executorChoices[] = {StringView("threads"), StringView("gpu")};
	static constexpr StringView failureChoices[] = {StringView("cancelFrame"),
		StringView("partial"), StringView("nothing")};
	const auto names = value::makeChain(VarType::String);

	auto blockSettings = [&](mem_std::Vector<SettingDesc> &out) {
		SettingDesc executors;
		executors.name = StringView("executors");
		executors.type = VarType::Array;
		executors.element = names;
		executors.role = SettingRole::ChoiceSet;
		executors.choices = SpanView<StringView>(executorChoices, 2);
		out.emplace_back(sprt::move(executors));

		SettingDesc onFailure;
		onFailure.name = StringView("onFailure");
		onFailure.type = VarType::String;
		onFailure.role = SettingRole::Choice;
		onFailure.choices = SpanView<StringView>(failureChoices, 3);
		out.emplace_back(sprt::move(onFailure));

		// Milliseconds; 0 is the executor's own default.
		SettingDesc timeout;
		timeout.name = StringView("timeout");
		timeout.type = VarType::Int32;
		out.emplace_back(sprt::move(timeout));
	};

	// What a fan-out keeps: the set, the first branch activation, the branches closed, the phase
	// (FanOutLocal). The machine reads and writes them; a debugger sees where the block is.
	value::FieldDef locals[] = {
		value::FieldDef{.name = StringView("entities"),
			.type = VarType::Array,
			.element = value::makeChain(VarType::EntityRef)},
		value::FieldDef{.name = StringView("first"), .type = VarType::Int},
		value::FieldDef{.name = StringView("closed"), .type = VarType::Int},
		value::FieldDef{.name = StringView("phase"), .type = VarType::Int},
	};

	auto fanOut = [&](StringView name, bool query) {
		static constexpr StringView bodyOut[] = {StringView("body")};
		PinDesc in[] = {pinRequired("entities", VarType::Array)};
		in[0].element = value::makeChain(VarType::EntityRef);
		PinDesc out[] = {pin("entity", VarType::EntityRef), pin("index", VarType::Int32)};

		mem_std::Vector<SettingDesc> settings;
		blockSettings(settings);
		if (query) {
			for (auto key : {StringView("with"), StringView("without")}) {
				SettingDesc list;
				list.name = key;
				list.type = VarType::Array;
				list.element = names;
				list.role = SettingRole::ComponentNames;
				settings.emplace_back(sprt::move(list));
			}
		}

		OpDef def;
		def.name = name;
		if (!query) {
			def.dataIn = SpanView<PinDesc>(in, 1);
		}
		def.dataOut = SpanView<PinDesc>(out, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(bodyOut, 1);
		def.scopeExecOut = 1u << 0;
		def.scopeKind = ScopeKind::Parallel;
		def.settings = SpanView<SettingDesc>(settings.data(), settings.size());
		def.parallel = OpParallel::Flow;
		def.locals = SpanView<value::FieldDef>(locals, 4);
		static constexpr StringView syn[] = {StringView("parallel"), StringView("batch")};
		def.synonyms = SpanView<StringView>(syn, 2);
		// The query reads the scene at the fan-out, on the machine's thread, before any branch
		// runs.
		def.flags = query ? OpFlags::ReadsScene : OpFlags::None;
		auto body = query ? SP_FLOW_OPS_BODY(parForEachWith) : SP_FLOW_OPS_BODY(parForEach);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	fanOut(StringView("par.forEach"), false);
	fanOut(StringView("par.forEachWith"), true);

	{
		static constexpr StringView completed[] = {StringView("completed")};
		const auto masks = value::makeChain(VarType::Bool);
		PinDesc out[] = {pin("present", VarType::Array, masks), pin("failed", VarType::Array, masks)};
		OpDef def;
		def.name = StringView("par.barrier");
		def.dataOut = SpanView<PinDesc>(out, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(completed, 1);
		def.joinsScope = true;
		def.parallel = OpParallel::Flow;
		static constexpr StringView syn[] = {StringView("join"), StringView("parallel")};
		def.synonyms = SpanView<StringView>(syn, 2);
		auto body = SP_FLOW_OPS_BODY(parBarrier);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	auto collector = [&](StringView name, VarType value, VarType result, Body body,
							 value::ElementChain element = 0) {
		PinDesc in[] = {pinRole(pinRequired("value", value), flow::PinRole::BranchValue)};
		PinDesc out[] = {pin("result", result, element)};
		OpDef def;
		def.name = name;
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		static constexpr StringView syn[] = {StringView("join")};
		def.synonyms = SpanView<StringView>(syn, 1);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	};

	const auto chain = [](VarType t) { return value::makeChain(t); };

	collector("par.gatherBool", VarType::Bool, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Bool), chain(VarType::Bool));
	collector("par.gatherInt", VarType::Int, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Int), chain(VarType::Int));
	collector("par.gatherFloat", VarType::Float, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Float), chain(VarType::Float));
	collector("par.gatherInt32", VarType::Int32, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Int32), chain(VarType::Int32));
	collector("par.gatherUInt32", VarType::UInt32, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::UInt32), chain(VarType::UInt32));
	collector("par.gatherFloat32", VarType::Float32, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Float32), chain(VarType::Float32));
	collector("par.gatherVec2", VarType::Vec2, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Vec2), chain(VarType::Vec2));
	collector("par.gatherVec3", VarType::Vec3, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Vec3), chain(VarType::Vec3));
	collector("par.gatherVec4", VarType::Vec4, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Vec4), chain(VarType::Vec4));
	collector("par.gatherColor", VarType::Color, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::Color), chain(VarType::Color));
	collector("par.gatherEntityRef", VarType::EntityRef, VarType::Array,
			SP_FLOW_OPS_TBODY(parGather, flow::value::VarType::EntityRef), chain(VarType::EntityRef));

	collector("par.sumInt", VarType::Int, VarType::Int, SP_FLOW_OPS_TBODY(parSum, flow::value::VarType::Int));
	collector("par.sumFloat", VarType::Float, VarType::Float,
			SP_FLOW_OPS_TBODY(parSum, flow::value::VarType::Float));
	collector("par.sumInt32", VarType::Int32, VarType::Int32,
			SP_FLOW_OPS_TBODY(parSum, flow::value::VarType::Int32));
	collector("par.sumUInt32", VarType::UInt32, VarType::UInt32,
			SP_FLOW_OPS_TBODY(parSum, flow::value::VarType::UInt32));
	collector("par.sumFloat32", VarType::Float32, VarType::Float32,
			SP_FLOW_OPS_TBODY(parSum, flow::value::VarType::Float32));
	collector("par.minInt", VarType::Int, VarType::Int, SP_FLOW_OPS_TBODY(parMin, flow::value::VarType::Int));
	collector("par.minFloat", VarType::Float, VarType::Float,
			SP_FLOW_OPS_TBODY(parMin, flow::value::VarType::Float));
	collector("par.minInt32", VarType::Int32, VarType::Int32,
			SP_FLOW_OPS_TBODY(parMin, flow::value::VarType::Int32));
	collector("par.minUInt32", VarType::UInt32, VarType::UInt32,
			SP_FLOW_OPS_TBODY(parMin, flow::value::VarType::UInt32));
	collector("par.minFloat32", VarType::Float32, VarType::Float32,
			SP_FLOW_OPS_TBODY(parMin, flow::value::VarType::Float32));
	collector("par.maxInt", VarType::Int, VarType::Int, SP_FLOW_OPS_TBODY(parMax, flow::value::VarType::Int));
	collector("par.maxFloat", VarType::Float, VarType::Float,
			SP_FLOW_OPS_TBODY(parMax, flow::value::VarType::Float));
	collector("par.maxInt32", VarType::Int32, VarType::Int32,
			SP_FLOW_OPS_TBODY(parMax, flow::value::VarType::Int32));
	collector("par.maxUInt32", VarType::UInt32, VarType::UInt32,
			SP_FLOW_OPS_TBODY(parMax, flow::value::VarType::UInt32));
	collector("par.maxFloat32", VarType::Float32, VarType::Float32,
			SP_FLOW_OPS_TBODY(parMax, flow::value::VarType::Float32));

	collector("par.sumVec2", VarType::Vec2, VarType::Vec2, SP_FLOW_OPS_TBODY(parSumVec, 2));
	collector("par.sumVec3", VarType::Vec3, VarType::Vec3, SP_FLOW_OPS_TBODY(parSumVec, 3));
	collector("par.sumVec4", VarType::Vec4, VarType::Vec4, SP_FLOW_OPS_TBODY(parSumVec, 4));

	collector("par.count", VarType::Bool, VarType::Int32, SP_FLOW_OPS_BODY(parCount));
	collector("par.any", VarType::Bool, VarType::Bool, SP_FLOW_OPS_BODY(parAny));
	collector("par.all", VarType::Bool, VarType::Bool, SP_FLOW_OPS_BODY(parAll));

	return r.status;
}

} // namespace stappler::flow::ops
