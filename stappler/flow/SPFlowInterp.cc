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

// The interpreter's non-template half: the name tables of the run vocabulary and the trace of a
// report. The machine itself - the walk, the door, the local store - is a template over two
// policies and lives in SPFlowMachine.hpp, SPFlowContext.hpp and SPFlowLocal.hpp, from where
// XSGraph.scu.cpp instantiates it for the built graph and the three arena kinds.

#include "SPFlowInterp.h"


namespace STAPPLER_VERSIONIZED stappler::flow {

StringView getRunOutcomeName(RunOutcome o) {
	switch (o) {
	case RunOutcome::Invalid: return StringView("invalid");
	case RunOutcome::Completed: return StringView("completed");
	case RunOutcome::Deadlock: return StringView("deadlock");
	case RunOutcome::OpError: return StringView("op-error");
	case RunOutcome::StepLimit: return StringView("step-limit");
	case RunOutcome::ActivationLimit: return StringView("activation-limit");
	case RunOutcome::Cancelled: return StringView("cancelled");
	}
	return StringView("?");
}

bool readRunOutcome(StringView name, RunOutcome &out) {
	for (uint32_t i = 0; i < RunOutcomeCount; ++i) {
		if (getRunOutcomeName(RunOutcome(i)) == name) {
			out = RunOutcome(i);
			return true;
		}
	}
	return false;
}

StringView getRollbackQuantumName(RollbackQuantum q) {
	switch (q) {
	case RollbackQuantum::Step: return StringView("step");
	case RollbackQuantum::Failable: return StringView("failable");
	case RollbackQuantum::Run: return StringView("run");
	}
	return StringView("?");
}

bool readRollbackQuantum(StringView name, RollbackQuantum &out) {
	for (uint32_t i = uint32_t(RollbackQuantum::Step); i <= uint32_t(RollbackQuantum::Run); ++i) {
		if (getRollbackQuantumName(RollbackQuantum(i)) == name) {
			out = RollbackQuantum(i);
			return true;
		}
	}
	return false;
}

StringView getWatchSourceName(Watch::Source s) {
	switch (s) {
	case Watch::Source::None: return StringView("none");
	case Watch::Source::NodeRecord: return StringView("node-record");
	case Watch::Source::NodeState: return StringView("node-state");
	case Watch::Source::Scene: return StringView("scene");
	}
	return StringView("?");
}

bool readWatchSource(StringView name, Watch::Source &out) {
	for (uint32_t i = uint32_t(Watch::Source::None); i <= uint32_t(Watch::Source::Scene); ++i) {
		if (getWatchSourceName(Watch::Source(i)) == name) {
			out = Watch::Source(i);
			return true;
		}
	}
	return false;
}

StringView getBreakWhenName(BreakWhen w) {
	switch (w) {
	case BreakWhen::Default: return StringView("default");
	case BreakWhen::BeforeStep: return StringView("before-step");
	case BreakWhen::AfterStep: return StringView("after-step");
	}
	return StringView("?");
}

bool readBreakWhen(StringView name, BreakWhen &out) {
	for (uint32_t i = uint32_t(BreakWhen::Default); i <= uint32_t(BreakWhen::AfterStep); ++i) {
		if (getBreakWhenName(BreakWhen(i)) == name) {
			out = BreakWhen(i);
			return true;
		}
	}
	return false;
}

StringView getBreakCompareName(BreakCompare c) {
	switch (c) {
	case BreakCompare::Any: return StringView("any");
	case BreakCompare::Equal: return StringView("equal");
	case BreakCompare::NotEqual: return StringView("not-equal");
	case BreakCompare::Less: return StringView("less");
	case BreakCompare::Greater: return StringView("greater");
	case BreakCompare::Changed: return StringView("changed");
	}
	return StringView("?");
}

bool readBreakCompare(StringView name, BreakCompare &out) {
	for (uint32_t i = uint32_t(BreakCompare::Any); i <= uint32_t(BreakCompare::Changed); ++i) {
		if (getBreakCompareName(BreakCompare(i)) == name) {
			out = BreakCompare(i);
			return true;
		}
	}
	return false;
}

StringView getWatchResolutionName(WatchResolution r) {
	switch (r) {
	case WatchResolution::Ok: return StringView("ok");
	case WatchResolution::NotAddressed: return StringView("not-addressed");
	case WatchResolution::UnknownName: return StringView("unknown-name");
	case WatchResolution::NoStore: return StringView("no-store");
	case WatchResolution::NoRecord: return StringView("no-record");
	case WatchResolution::UnknownField: return StringView("unknown-field");
	}
	return StringView("?");
}

StringView getRunStateName(RunState s) {
	switch (s) {
	case RunState::Idle: return StringView("idle");
	case RunState::Paused: return StringView("paused");
	case RunState::Finished: return StringView("finished");
	case RunState::Suspended: return StringView("suspended");
	}
	return StringView("?");
}

mem_std::String RunReport::getTrace() const {
	// Node units only. Closing an iteration is a unit of work and it is in the log, but it is the
	// interpreter's bookkeeping rather than a node running, and the trace is the order nodes ran
	// in.
	mem_std::String out;
	for (auto &it : log) {
		if (it.kind != RunStepKind::Node) {
			continue;
		}
		if (!out.empty()) {
			out.append(" ");
		}
		out.append(mem_std::toString(it.id));
	}
	return out;
}

// The five fields of interp.NodeState, resolved and checked. Not a template - the descriptor is the
// same whichever store is asking - and out of line because both stores call it once, at init.
Status resolveNodeStateFields(const value::ComponentType &type, NodeStateFields &out) {
	out.inputs = type.getField(StringView("inputs"));
	out.produced = type.getField(StringView("produced"));
	out.flags = type.getField(StringView("flags"));
	out.stallPin = type.getField(StringView("stallPin"));
	out.turns = type.getField(StringView("turns"));
	if (!out.isValid()) {
		return Status::ErrorInvalidArguemnt;
	}

	// readNodeState decodes the whole record in one arena read and picks the five fields out of the
	// bytes by their schema offsets, which is sound only while all five are Ints. Checked here
	// rather than assumed: registerCoreTypes declares them far enough away that an edit to one
	// would not obviously reach the other.
	const value::FieldDesc *ints[] = {out.inputs, out.produced, out.flags, out.stallPin, out.turns};
	for (auto it : ints) {
		if (it->type != value::VarType::Int) {
			slog().error("flow::LocalStore", "resolveNodeStateFields: interp.NodeState.", it->name,
					" is not an Int, and the state is decoded as one");
			return Status::ErrorInvalidArguemnt;
		}
	}
	return Status::Ok;
}

} // namespace stappler::flow
