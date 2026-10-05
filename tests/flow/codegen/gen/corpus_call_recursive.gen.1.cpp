// Generated from the graph "recursive". Do not edit: regenerate it.
//
// Part 2 of 2: the nodes 5..10. The parts compile in parallel and are linked together; the first holds the dispatcher
// and the table of entry points.

#include "corpus_call_recursive.gen.h"

// The door's own bodies: this unit instantiates them over a site of its own, which is what makes
// a node index a constant inside the step below (StaticContext).
#include "SPFlowContext.hpp"

// The operations, as templates over the door (OpDef::inlineName).
#include "SPFlowOpsInline.h"
#include "SPFlowFunctionInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_call_recursive {

// One node each, and the index is the constant: `stepN` is the `step<N>` of the plan. Inside it
// the edge feeding a pin, the constant behind it and the offsets of the node's state and record
// are decided by the compiler, and the operation is called by name where it has an inline form.
// Everything else - the descriptor, the schema, the bindings, the decoded constants - is resolved
// when the unit is loaded, and read exactly as the interpreter reads it.

// node 5: flow.branch
template <typename Local>
static Status step5(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 5, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowBranch(ctx);
	ctx.finish(site);
	return st;
}

// node 6: fn.recursive:fact#return:0
template <typename Local>
static Status step6(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 6, Local> ctx;
	ctx.bind(site);
	auto st = flow::fn::ret(ctx);
	ctx.finish(site);
	return st;
}

// node 7: math.subInt
template <typename Local>
static Status step7(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 7, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::mathSub<flow::value::VarType::Int>(ctx);
	ctx.finish(site);
	return st;
}

// node 8: fn.recursive:fact
template <typename Local>
static Status step8(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 8, Local> ctx;
	ctx.bind(site);
	auto st = flow::fn::call(ctx);
	ctx.finish(site);
	return st;
}

// node 9: math.mulInt
template <typename Local>
static Status step9(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 9, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::mathMulInt(ctx);
	ctx.finish(site);
	return st;
}

// node 10: fn.recursive:fact#return:0
template <typename Local>
static Status step10(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 10, Local> ctx;
	ctx.bind(site);
	auto st = flow::fn::ret(ctx);
	ctx.finish(site);
	return st;
}

// The nodes this part performs. Declared in the header so that the dispatcher can call it
// from another translation unit, and instantiated below for every store a run may live in.
template <typename Local>
Status stepPart1(flow::CompiledStepSiteT<Local> &site) {
	switch (site.node) {
	case 5: return step5<Local>(site);
	case 6: return step6<Local>(site);
	case 7: return step7<Local>(site);
	case 8: return step8<Local>(site);
	case 9: return step9<Local>(site);
	case 10: return step10<Local>(site);
	default: break;
	}
	// A node index this part does not hold. The machine never asks for one; a unit that answered
	// something for it would be answering for a graph it is not.
	return Status::ErrorNotImplemented;
}

template Status stepPart1<flow::CompiledLocalT<flow::value::PlainArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledLocalT<flow::value::PlainArena, flow::NoEnv>> &);
template Status stepPart1<flow::CompiledLocalT<flow::value::TrackedArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledLocalT<flow::value::TrackedArena, flow::NoEnv>> &);
template Status stepPart1<flow::CompiledLocalT<flow::value::ShadowArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledLocalT<flow::value::ShadowArena, flow::NoEnv>> &);
template Status stepPart1<flow::CompiledFastLocalT<flow::value::PlainArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledFastLocalT<flow::value::PlainArena, flow::NoEnv>> &);
template Status stepPart1<flow::CompiledFastLocalT<flow::value::TrackedArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledFastLocalT<flow::value::TrackedArena, flow::NoEnv>> &);
template Status stepPart1<flow::CompiledFastLocalT<flow::value::ShadowArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledFastLocalT<flow::value::ShadowArena, flow::NoEnv>> &);

} // namespace stappler::flow::gen::corpus_call_recursive
