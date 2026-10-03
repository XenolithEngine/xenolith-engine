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

// Reaching the scene from a graph: seven nodes that name the component and the field by string, so
// that a graph can read and write state its author chose. They are sequenced, not pure - each has
// an exec input and a `then` output and delivers its answer as data, because a read placed after a
// write must not be free to run before it; `scene.global` is the exception, since the global
// entity's identifier cannot change during a run. The type is in the operation and the name is in
// the data: `makeTypeId` is a pure function of a string, so naming a component by string decides
// nothing at run time, while the type of the value is never decided by data - `scene.getInt`
// reads an Int field or fails. Entity lifetime, intersections and iteration over a pool wait on the
// deferred-effect buffer. The bodies are in SPFlowOpsInline.h; this file is the signatures.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpContext;

Status registerSceneOps(OpRegistry &reg) {
	Registrar r{reg};

	const StringView then[] = {StringView("then")};

	{
		PinDesc out[] = {pin("entity", VarType::EntityRef)};
		OpDef def;
		def.name = StringView("scene.global");
		static constexpr StringView syn[] = {StringView("singleton"), StringView("world")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		auto body = SP_FLOW_OPS_BODY(sceneGlobal);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		// A named entity of the project, by its name. The name is resolved when the graph is bound
		// to the project's table, so the node hands back a constant.
		PinDesc in[] = {pinRole(pinRequired("name", VarType::String), PinRole::EntityName)};
		PinDesc out[] = {pin("entity", VarType::EntityRef)};
		OpDef def;
		def.name = StringView("scene.named");
		static constexpr StringView syn[] = {StringView("entity"), StringView("named")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		auto body = SP_FLOW_OPS_BODY(sceneNamed);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		// Optional: a type nobody registered is what this operation exists to say "no" about.
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentNameOptional)};
		PinDesc out[] = {pin("result", VarType::Bool)};
		OpDef def;
		def.name = StringView("scene.has");
		static constexpr StringView syn[] = {StringView("exists"), StringView("contains")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		auto body = SP_FLOW_OPS_BODY(sceneHas);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
			pinRole(pinRequired("field", VarType::String), PinRole::FieldName)};
		PinDesc out[] = {pinRole(pin("value", VarType::Int), PinRole::FieldValue)};
		OpDef def;
		def.name = StringView("scene.getInt");
		static constexpr StringView syn[] = {StringView("read"), StringView("load")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 3);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		auto body = SP_FLOW_OPS_BODY(sceneGetInt);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
			pinRole(pinRequired("field", VarType::String), PinRole::FieldName),
			pinRole(pinDefault("value", VarType::Int, mem_std::Value()), PinRole::FieldValue)};
		OpDef def;
		def.name = StringView("scene.setInt");
		static constexpr StringView syn[] = {StringView("write"), StringView("store")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 4);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene | OpFlags::WritesScene;
		def.parallel = OpParallel::SceneWrite;
		auto body = SP_FLOW_OPS_BODY(sceneSetInt);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		PinDesc in[] = {pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
			pinRole(pinRequired("field", VarType::String), PinRole::FieldName),
			pinRole(pinDefault("value", VarType::Int, mem_std::Value()), PinRole::FieldValue)};
		PinDesc out[] = {pin("found", VarType::Bool), pin("entity", VarType::EntityRef)};
		OpDef def;
		def.name = StringView("scene.findByField");
		static constexpr StringView syn[] = {StringView("query"), StringView("search"), StringView("lookup")};
		def.synonyms = SpanView<StringView>(syn, 3);
		def.dataIn = SpanView<PinDesc>(in, 3);
		def.dataOut = SpanView<PinDesc>(out, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		auto body = SP_FLOW_OPS_BODY(sceneFindByField);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentName)};
		OpDef def;
		def.name = StringView("scene.addComponent");
		static constexpr StringView syn[] = {StringView("attach"), StringView("create")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::WritesScene;
		auto body = SP_FLOW_OPS_BODY(sceneAddComponent);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	{
		// Optional: removing what is not there is done, not refused - so a type nobody registered
		// is an answer here too.
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentNameOptional)};
		OpDef def;
		def.name = StringView("scene.removeComponent");
		static constexpr StringView syn[] = {StringView("detach"), StringView("delete")};
		def.synonyms = SpanView<StringView>(syn, 2);
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::WritesScene;
		auto body = SP_FLOW_OPS_BODY(sceneRemoveComponent);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	// The same pair for the 32-bit scalars. A field binds only to a pin of its own type.
	auto sceneScalar = [&](StringView get, StringView set, VarType type, Body getBody) {
		{
			PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
				pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
				pinRole(pinRequired("field", VarType::String), PinRole::FieldName)};
			PinDesc out[] = {pinRole(pin("value", type), PinRole::FieldValue)};
			OpDef def;
			def.name = get;
			def.dataIn = SpanView<PinDesc>(in, 3);
			def.dataOut = SpanView<PinDesc>(out, 1);
			def.hasExecIn = true;
			def.execOut = SpanView<StringView>(then, 1);
			def.flags = OpFlags::ReadsScene;
			def.parallel = OpParallel::SceneRead;
			def.invoke = getBody.invoke;
			def.inlineName = getBody.inlineName;
			r.add(def);
		}
		{
			PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
				pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
				pinRole(pinRequired("field", VarType::String), PinRole::FieldName),
				pinRole(pinDefault("value", type, mem_std::Value()), PinRole::FieldValue)};
			OpDef def;
			def.name = set;
			def.dataIn = SpanView<PinDesc>(in, 4);
			def.hasExecIn = true;
			def.execOut = SpanView<StringView>(then, 1);
			def.flags = OpFlags::ReadsScene | OpFlags::WritesScene;
			def.parallel = OpParallel::SceneWrite;
			auto body = SP_FLOW_OPS_BODY(sceneSetInt);
			def.invoke = body.invoke;
			def.inlineName = body.inlineName;
			r.add(def);
		}
	};
	sceneScalar("scene.getInt32", "scene.setInt32", VarType::Int32,
			Body{&inl::sceneGetScalar<value::VarType::Int32, OpContext>,
				StringView("flow::ops::inl::sceneGetScalar<flow::value::VarType::Int32>")});
	sceneScalar("scene.getUInt32", "scene.setUInt32", VarType::UInt32,
			Body{&inl::sceneGetScalar<value::VarType::UInt32, OpContext>,
				StringView("flow::ops::inl::sceneGetScalar<flow::value::VarType::UInt32>")});
	sceneScalar("scene.getFloat32", "scene.setFloat32", VarType::Float32,
			Body{&inl::sceneGetScalar<value::VarType::Float32, OpContext>,
				StringView("flow::ops::inl::sceneGetScalar<flow::value::VarType::Float32>")});

	// An Enum field names its family on the node: the value it reads carries that family, and the
	// bind refuses a field whose family is another one.
	{
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
			pinRole(pinRequired("field", VarType::String), PinRole::FieldName),
			pinRole(pin("family", VarType::String), PinRole::EnumFamily)};
		PinDesc out[] = {pinRole(pin("value", VarType::Enum), PinRole::FieldValue)};
		OpDef def;
		def.name = StringView("scene.getEnum");
		def.dataIn = SpanView<PinDesc>(in, 4);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		def.invoke = &inl::sceneGetScalar<value::VarType::Enum, OpContext>;
		def.inlineName = StringView("flow::ops::inl::sceneGetScalar<flow::value::VarType::Enum>");
		r.add(def);
	}
	{
		PinDesc in[] = {pinRole(pinRequired("target", VarType::EntityRef), PinRole::EntityTarget),
			pinRole(pinRequired("component", VarType::String), PinRole::ComponentName),
			pinRole(pinRequired("field", VarType::String), PinRole::FieldName),
			pinRole(pinDefault("value", VarType::Enum, mem_std::Value()), PinRole::FieldValue),
			pinRole(pin("family", VarType::String), PinRole::EnumFamily)};
		OpDef def;
		def.name = StringView("scene.setEnum");
		def.dataIn = SpanView<PinDesc>(in, 5);
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(then, 1);
		def.flags = OpFlags::ReadsScene | OpFlags::WritesScene;
		def.parallel = OpParallel::SceneWrite;
		auto body = SP_FLOW_OPS_BODY(sceneSetInt);
		def.invoke = body.invoke;
		def.inlineName = body.inlineName;
		r.add(def);
	}

	return r.status;
}

} // namespace stappler::flow::ops
