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

// Waiting, and the point of this file is how little of it there is. Waiting is data, not a state of
// the interpreter: a run lives exactly once, and "wait a tenth of a second" is a component
// in the scene that survives between runs, plus a node that reads the frame's dt and subtracts it.
// The exec output does not fire until the remainder is gone, and an exec output that did not fire
// is a branch that ended. So a graph that waits is a graph run once per frame with the
// waiting in the store, and it rewinds for free: the remainder is scene state, the journal already
// rolls scene state back, and replaying the same frame subtracts the same dt because dt is inside
// the snapshot. The same shape without the arithmetic is waiting for the environment - the answer
// is a component that appears, and the branch continues the frame it appears on. The bodies are in
// SPFlowOpsInline.h, and the two component names with them: `wait.timer` names them in C++ rather than
// through a pin, so they belong beside the body that reads them.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;

using inl::FrameTypeName;
using inl::TimerTypeName;

Status registerCoreComponents(value::TypeRegistry &reg) {
	if (!reg.get(TimerTypeName)) {
		value::FieldDef fields[] = {
			value::FieldDef{.name = StringView("remaining"), .type = value::VarType::Float},
		};
		if (!reg.createNative(TimerTypeName, SpanView<value::FieldDef>(fields, 1))) {
			return Status::ErrorInvalidArguemnt;
		}
	}
	return Status::Ok;
}

Status registerTimeOps(OpRegistry &reg) {
	Registrar r{reg};

	const StringView then[] = {StringView("then")};

	{
		// What this operation names in the scene, and no pin names any of it: the frame's dt and
		// the timer it arms are its own business, not the author's. The order is the contract with
		// inl::waitTimer, which reads them back by these indices.
		SceneRef refs[] = {
			SceneRef{.component = FrameTypeName, .field = StringView("dt"), .type = VarType::Float},
			SceneRef{.component = TimerTypeName, .optional = true, .access = SceneAccess::Write,
				.targeted = true},
			SceneRef{.component = TimerTypeName, .field = StringView("remaining"),
				.type = VarType::Float, .access = SceneAccess::Write, .targeted = true},
		};
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRequired("timeout", VarType::Float)};
		OpDef def;
		def.name = StringView("wait.timer");
		static constexpr StringView syn[] = {StringView("delay"), StringView("sleep"), StringView("pause")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.sceneRefs = SpanView<SceneRef>(refs, 3);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene | OpFlags::WritesScene;
		auto body = SP_FLOW_OPS_BODY(waitTimer);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		// Optional, and that is the whole operation: a component nobody has registered yet is an
		// answer that has not arrived, not a graph that names something wrong.
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentNameOptional)};
		OpDef def;
		def.name = StringView("await.component");
		static constexpr StringView syn[] = {StringView("wait"), StringView("until")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene | OpFlags::WritesScene;
		auto body = SP_FLOW_OPS_BODY(awaitComponent);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	return r.status;
}

} // namespace stappler::flow::ops
