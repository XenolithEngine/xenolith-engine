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

// Shared spelling for the node library: one place that says what a pin declaration looks like, so
// that the operations below read as signatures rather than as boilerplate. The bodies are not here
// either - they are templates over the door in SPFlowOpsInline.h, and every file beside this one is a
// list of signatures and one line per operation saying which body it is. That line names the body
// twice, and SP_FLOW_OPS_BODY below is what keeps the two spellings from drifting apart.

#include "SPFlowOps.h"
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::ops {

using flow::OpDef;
using flow::OpDesc;
using flow::OpFlags;
using flow::OpParallel;
using flow::OpRegistry;
using flow::PinDesc;
using flow::PinFlags;
using flow::PinRole;
using flow::SceneAccess;
using flow::SceneRef;
using value::VarType;

// An operation's two spellings, side by side: the function pointer the interpreter calls through a
// vtable, and the name a generated unit writes as a direct call (OpDef::inlineName). The macro
// derives both from one identifier, so a pair that fell out of step - an operation meaning two
// things - is not possible.
struct Body {
	flow::OpFn invoke;
	StringView inlineName;
};

#define SP_FLOW_OPS_BODY(fn) Body{&inl::fn<flow::OpContext>, StringView("flow::ops::inl::" #fn)}

static PinDesc pin(StringView name, VarType type, value::ElementChain element = 0) {
	PinDesc p;
	p.name = name;
	p.type = type;
	p.element = element;
	return p;
}

static PinDesc pinRequired(StringView name, VarType type) {
	auto p = pin(name, type);
	p.flags = PinFlags::Required;
	return p;
}

static PinDesc pinDefault(StringView name, VarType type, mem_std::Value &&def) {
	auto p = pin(name, type);
	p.def = sprt::move(def);
	return p;
}

// What this pin names in the scene. Written as a wrapper rather than a fourth constructor so that a
// signature still reads as its pins, with the role as an annotation on one of them.
static PinDesc pinRole(PinDesc &&p, flow::PinRole role) {
	p.role = role;
	return p;
}

// Whether an operation has a shader form (OpFlags::ShaderForm): a pure, flow or scene operation
// whose values are all of the GPU set, or a conversion with exactly one side in it - the narrowing
// or widening boundary of a GPU segment. Pins that name something (a component, a field, a target,
// a family) are literals of the node and carry no value to a shader.
static bool isGpuType(VarType t) {
	switch (t) {
	case VarType::Bool:
	case VarType::Int32:
	case VarType::UInt32:
	case VarType::Float32:
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: return true;
	default: return false;
	}
}

static bool hasShaderForm(const OpDef &def) {
	switch (def.parallel) {
	case OpParallel::Pure:
	case OpParallel::Flow:
	case OpParallel::SceneRead:
	case OpParallel::SceneWrite: break;
	default: return false;
	}
	if (def.scopeExecOut != 0 || def.joinsScope || !def.locals.empty() || !def.settings.empty()) {
		return false;
	}
	uint32_t values = 0, gpu = 0, ins = 0, outs = 0;
	auto count = [&](SpanView<PinDesc> pins, uint32_t &side) {
		bool ok = true;
		for (auto &p : pins) {
			switch (p.role) {
			case PinRole::ComponentName:
			case PinRole::ComponentNameOptional:
			case PinRole::FieldName:
			case PinRole::EntityTarget:
			case PinRole::EnumFamily: continue;
			case PinRole::BranchValue:
			case PinRole::ExtensionName:
			case PinRole::EntityName: ok = false; continue;
			default: break;
			}
			++values;
			++side;
			if (isGpuType(p.type)) {
				++gpu;
			}
		}
		return ok;
	};
	if (!count(def.dataIn, ins) || !count(def.dataOut, outs)) {
		return false;
	}
	return gpu == values || (ins == 1 && outs == 1 && gpu == 1);
}

// SPFlowOpsShader.cc: the shader form an operation's template form implies, or nothing.
void attachShaderForm(const OpDef &def, mem_std::String &name, mem_std::String &source, bool &inexact);

// Registration that leaves an existing operation alone. That is what makes registerCoreOps
// idempotent, and idempotence is what lets a host call it without knowing who called it first.
struct Registrar {
	OpRegistry &reg;
	Status status = Status::Ok;

	bool has(StringView name) const { return reg.get(name) != nullptr; }

	void add(const OpDef &def) {
		if (has(def.name)) {
			return;
		}
		auto copy = def;
		// The shader form is derived from the template form (SPFlowOpsShader.cc) unless the operation
		// names its own; ShaderForm is only set on an operation that has one.
		mem_std::String shaderName, shaderSource;
		bool inexact = false;
		if (copy.shaderName.empty()) {
			attachShaderForm(copy, shaderName, shaderSource, inexact);
			copy.shaderName = shaderName;
			copy.shaderSource = shaderSource;
		}
		if (hasShaderForm(def) && !copy.shaderName.empty()) {
			copy.flags |= OpFlags::ShaderForm;
		}
		if (inexact) {
			copy.flags |= OpFlags::ShaderInexact;
		}
		if (!reg.createNative(copy)) {
			status = Status::ErrorInvalidArguemnt;
		}
	}
};

Status registerCoreOps(OpRegistry &reg) {
	auto st = registerFlowOps(reg);
	if (st != Status::Ok) {
		return st;
	}
	st = registerMathOps(reg);
	if (st != Status::Ok) {
		return st;
	}
	st = registerStringOps(reg);
	if (st != Status::Ok) {
		return st;
	}
	st = registerTimeOps(reg);
	if (st != Status::Ok) {
		return st;
	}
	st = registerSceneOps(reg);
	if (st != Status::Ok) {
		return st;
	}
	st = registerNumericOps(reg);
	if (st != Status::Ok) {
		return st;
	}
	return registerParallelOps(reg);
}

} // namespace stappler::flow::ops
