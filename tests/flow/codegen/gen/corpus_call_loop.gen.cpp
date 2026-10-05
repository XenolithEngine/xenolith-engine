// Generated from the graph "loop". Do not edit: regenerate it.
//
// Part 1 of 2: the nodes 0..2. The parts compile in parallel and are linked together; the first holds the dispatcher
// and the table of entry points.

#include "corpus_call_loop.gen.h"

// The door's own bodies: this unit instantiates them over a site of its own, which is what makes
// a node index a constant inside the step below (StaticContext).
#include "SPFlowContext.hpp"

// The operations, as templates over the door (OpDef::inlineName).
#include "SPFlowOpsInline.h"
#include "SPFlowFunctionInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_call_loop {

// One node each, and the index is the constant: `stepN` is the `step<N>` of the plan. Inside it
// the edge feeding a pin, the constant behind it and the offsets of the node's state and record
// are decided by the compiler, and the operation is called by name where it has an inline form.
// Everything else - the descriptor, the schema, the bindings, the decoded constants - is resolved
// when the unit is loaded, and read exactly as the interpreter reads it.

// node 0: flow.event
template <typename Local>
static Status step0(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 0, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowEvent(ctx);
	ctx.finish(site);
	return st;
}

// node 1: flow.forEach
template <typename Local>
static Status step1(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 1, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowForEach(ctx);
	ctx.finish(site);
	return st;
}

// node 2: fn.loop:square
template <typename Local>
static Status step2(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 2, Local> ctx;
	ctx.bind(site);
	auto st = flow::fn::call(ctx);
	ctx.finish(site);
	return st;
}

// The nodes this part performs. Declared in the header so that the dispatcher can call it
// from another translation unit, and instantiated below for every store a run may live in.
template <typename Local>
Status stepPart0(flow::CompiledStepSiteT<Local> &site) {
	switch (site.node) {
	case 0: return step0<Local>(site);
	case 1: return step1<Local>(site);
	case 2: return step2<Local>(site);
	default: break;
	}
	// A node index this part does not hold. The machine never asks for one; a unit that answered
	// something for it would be answering for a graph it is not.
	return Status::ErrorNotImplemented;
}

template Status stepPart0<flow::CompiledLocalT<flow::value::PlainArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledLocalT<flow::value::PlainArena, flow::NoEnv>> &);
template Status stepPart0<flow::CompiledLocalT<flow::value::TrackedArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledLocalT<flow::value::TrackedArena, flow::NoEnv>> &);
template Status stepPart0<flow::CompiledLocalT<flow::value::ShadowArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledLocalT<flow::value::ShadowArena, flow::NoEnv>> &);
template Status stepPart0<flow::CompiledFastLocalT<flow::value::PlainArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledFastLocalT<flow::value::PlainArena, flow::NoEnv>> &);
template Status stepPart0<flow::CompiledFastLocalT<flow::value::TrackedArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledFastLocalT<flow::value::TrackedArena, flow::NoEnv>> &);
template Status stepPart0<flow::CompiledFastLocalT<flow::value::ShadowArena, flow::NoEnv>>(flow::CompiledStepSiteT<flow::CompiledFastLocalT<flow::value::ShadowArena, flow::NoEnv>> &);

// The dispatcher, by range: the parts are contiguous and in order, so finding which one
// holds a node is a comparison rather than a second switch over every index.
template <typename Local>
static Status step(flow::CompiledStepSiteT<Local> &site) {
	if (site.node < 3u) { return stepPart0<Local>(site); }
	return stepPart1<Local>(site);
}

// One entry per store a run may live in: the arena store and the fast store, each over the three
// arena kinds a scene is ever in. In a release build ShadowArena is TrackedArena, so two slots of
// each pair name one function - which is what the loader would have handed out anyway.
const flow::CompiledStepsT<flow::NoEnv> *steps() {
	static constexpr flow::CompiledStepsT<flow::NoEnv> table = {
		.plain = &step<flow::CompiledLocalT<flow::value::PlainArena, flow::NoEnv>>,
		.tracked = &step<flow::CompiledLocalT<flow::value::TrackedArena, flow::NoEnv>>,
		.shadow = &step<flow::CompiledLocalT<flow::value::ShadowArena, flow::NoEnv>>,
		.fastPlain = &step<flow::CompiledFastLocalT<flow::value::PlainArena, flow::NoEnv>>,
		.fastTracked = &step<flow::CompiledFastLocalT<flow::value::TrackedArena, flow::NoEnv>>,
		.fastShadow = &step<flow::CompiledFastLocalT<flow::value::ShadowArena, flow::NoEnv>>,
	};
	return &table;
}

} // namespace stappler::flow::gen::corpus_call_loop

namespace STAPPLER_VERSIONIZED stappler {

template class flow::StaticGraph<flow::gen::corpus_call_loop::Tables>;

} // namespace stappler
