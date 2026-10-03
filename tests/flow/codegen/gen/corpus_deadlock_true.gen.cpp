// Generated from the graph "<unnamed>". Do not edit: regenerate it.

#include "corpus_deadlock_true.gen.h"

// The door's own bodies: this unit instantiates them over a site of its own, which is what makes
// a node index a constant inside the step below (StaticContext).
#include "SPFlowContext.hpp"

// The operations, as templates over the door (OpDef::inlineName).
#include "SPFlowOpsInline.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_deadlock_true {

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

// node 1: flow.branch
template <typename Local>
static Status step1(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 1, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::flowBranch(ctx);
	ctx.finish(site);
	return st;
}

// node 2: test.split (through the registry: no inline form)
template <typename Local>
static Status step2(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 2, Local> ctx;
	ctx.bind(site);
	auto invoke = site.op->getInvoke();
	auto st = invoke ? invoke(ctx) : Status::ErrorNotImplemented;
	ctx.finish(site);
	return st;
}

// node 3: debug.trace
template <typename Local>
static Status step3(flow::CompiledStepSiteT<Local> &site) {
	flow::StaticContext<Tables, 3, Local> ctx;
	ctx.bind(site);
	auto st = flow::ops::inl::debugTrace(ctx);
	ctx.finish(site);
	return st;
}

// The switch the machine enters, and the only thing in the unit that is not about one node.
template <typename Local>
static Status step(flow::CompiledStepSiteT<Local> &site) {
	switch (site.node) {
	case 0: return step0<Local>(site);
	case 1: return step1<Local>(site);
	case 2: return step2<Local>(site);
	case 3: return step3<Local>(site);
	default: break;
	}
	// A node index this part does not hold. The machine never asks for one; a unit that answered
	// something for it would be answering for a graph it is not.
	return Status::ErrorNotImplemented;
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

} // namespace stappler::flow::gen::corpus_deadlock_true

namespace STAPPLER_VERSIONIZED stappler {

template class flow::StaticGraph<flow::gen::corpus_deadlock_true::Tables>;

} // namespace stappler
