// Generated from the graph "synth". Do not edit: regenerate it.
//
// Part 1 of 2: the nodes 0..7. The parts compile in parallel and are linked together; the first holds the dispatcher
// and the table of entry points.

#include "synth_4101.gen.h"

// The door's own bodies: this unit instantiates them over a site of its own, which is what makes
// a node index a constant inside the step below (StaticContext).
#include "SPFlowContext.hpp"

// The operations, as templates over the door (OpDef::inlineName).
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::synth_4101 {

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

// node 1: value.int
template <typename Local>
static Status step1(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 1, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::valuePassthrough(ctx);
	ctx.finish(site);
	return st;
}

// node 2: flow.forEach
template <typename Local>
static Status step2(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 2, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowForEach(ctx);
	ctx.finish(site);
	return st;
}

// node 3: flow.sequence
template <typename Local>
static Status step3(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 3, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowSequence(ctx);
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

// node 5: math.addInt
template <typename Local>
static Status step5(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 5, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::mathAddInt(ctx);
	ctx.finish(site);
	return st;
}

// node 6: debug.trace
template <typename Local>
static Status step6(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 6, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// node 7: value.string
template <typename Local>
static Status step7(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 7, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::valueString(ctx);
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
	case 3: return step3<Local>(site);
	case 4: return step4<Local>(site);
	case 5: return step5<Local>(site);
	case 6: return step6<Local>(site);
	case 7: return step7<Local>(site);
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
	if (site.node < 8u) { return stepPart0<Local>(site); }
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

} // namespace stappler::flow::gen::synth_4101

namespace STAPPLER_VERSIONIZED stappler {

template class flow::StaticGraph<flow::gen::synth_4101::Tables>;

} // namespace stappler
