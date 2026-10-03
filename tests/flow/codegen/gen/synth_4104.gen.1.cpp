// Generated from the graph "synth". Do not edit: regenerate it.
//
// Part 2 of 2: the nodes 9..18. The parts compile in parallel and are linked together; the first holds the dispatcher
// and the table of entry points.

#include "synth_4104.gen.h"

// The door's own bodies: this unit instantiates them over a site of its own, which is what makes
// a node index a constant inside the step below (StaticContext).
#include "SPFlowContext.hpp"

// The operations, as templates over the door (OpDef::inlineName).
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::synth_4104 {

// One node each, and the index is the constant: `stepN` is the `step<N>` of the plan. Inside it
// the edge feeding a pin, the constant behind it and the offsets of the node's state and record
// are decided by the compiler, and the operation is called by name where it has an inline form.
// Everything else - the descriptor, the schema, the bindings, the decoded constants - is resolved
// when the unit is loaded, and read exactly as the interpreter reads it.

// node 9: debug.trace
template <typename Local>
static Status step9(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 9, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// node 10: flow.sequence
template <typename Local>
static Status step10(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 10, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowSequence(ctx);
	ctx.finish(site);
	return st;
}

// node 11: debug.trace
template <typename Local>
static Status step11(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 11, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// node 12: compare.equalInt
template <typename Local>
static Status step12(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 12, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::compareEqualInt(ctx);
	ctx.finish(site);
	return st;
}

// node 13: flow.branch
template <typename Local>
static Status step13(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 13, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowBranch(ctx);
	ctx.finish(site);
	return st;
}

// node 14: flow.sequence
template <typename Local>
static Status step14(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 14, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowSequence(ctx);
	ctx.finish(site);
	return st;
}

// node 15: debug.trace
template <typename Local>
static Status step15(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 15, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// node 16: math.addInt
template <typename Local>
static Status step16(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 16, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::mathAddInt(ctx);
	ctx.finish(site);
	return st;
}

// node 17: debug.trace
template <typename Local>
static Status step17(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 17, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// node 18: math.addInt
template <typename Local>
static Status step18(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 18, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::mathAddInt(ctx);
	ctx.finish(site);
	return st;
}

// The nodes this part performs. Declared in the header so that the dispatcher can call it
// from another translation unit, and instantiated below for every store a run may live in.
template <typename Local>
Status stepPart1(flow::CompiledStepSiteT<Local> &site) {
	switch (site.node) {
	case 9: return step9<Local>(site);
	case 10: return step10<Local>(site);
	case 11: return step11<Local>(site);
	case 12: return step12<Local>(site);
	case 13: return step13<Local>(site);
	case 14: return step14<Local>(site);
	case 15: return step15<Local>(site);
	case 16: return step16<Local>(site);
	case 17: return step17<Local>(site);
	case 18: return step18<Local>(site);
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

} // namespace stappler::flow::gen::synth_4104
