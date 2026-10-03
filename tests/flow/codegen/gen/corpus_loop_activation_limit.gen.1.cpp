// Generated from the graph "<unnamed>". Do not edit: regenerate it.
//
// Part 2 of 2: the nodes 2..4. The parts compile in parallel and are linked together; the first holds the dispatcher
// and the table of entry points.

#include "corpus_loop_activation_limit.gen.h"

// The door's own bodies: this unit instantiates them over a site of its own, which is what makes
// a node index a constant inside the step below (StaticContext).
#include "SPFlowContext.hpp"

// The operations, as templates over the door (OpDef::inlineName).
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_loop_activation_limit {

// One node each, and the index is the constant: `stepN` is the `step<N>` of the plan. Inside it
// the edge feeding a pin, the constant behind it and the offsets of the node's state and record
// are decided by the compiler, and the operation is called by name where it has an inline form.
// Everything else - the descriptor, the schema, the bindings, the decoded constants - is resolved
// when the unit is loaded, and read exactly as the interpreter reads it.

// node 2: debug.trace
template <typename Local>
static Status step2(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 2, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// node 3: math.addInt
template <typename Local>
static Status step3(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 3, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::mathAddInt(ctx);
	ctx.finish(site);
	return st;
}

// node 4: debug.trace
template <typename Local>
static Status step4(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 4, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// The nodes this part performs. Declared in the header so that the dispatcher can call it
// from another translation unit, and instantiated below for every store a run may live in.
template <typename Local>
Status stepPart1(flow::CompiledStepSiteT<Local> &site) {
	switch (site.node) {
	case 2: return step2<Local>(site);
	case 3: return step3<Local>(site);
	case 4: return step4<Local>(site);
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

} // namespace stappler::flow::gen::corpus_loop_activation_limit
