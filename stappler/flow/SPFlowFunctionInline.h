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


#ifndef STAPPLER_FLOW_SPFLOWFUNCTIONINLINE_H_
#define STAPPLER_FLOW_SPFLOWFUNCTIONINLINE_H_

#include "SPFlowContext.h"

// The bodies of the operations a function contributes (SPFlowFunction.h), as templates over the
// door like the node library's: the linker registers `&fn::name<OpContext>`, and a generated unit
// calls `flow::fn::name(ctx)` on a door whose node index is a constant.
namespace STAPPLER_VERSIONIZED stappler::flow::fn {

// An input handed to the output of the same index: a container is copied, so the output owns its
// own block, and an optional container input with nothing behind it is the empty container.
template <typename Ctx>
Status passPin(Ctx &ctx, uint32_t in, uint32_t out) {
	auto &desc = ctx.getOp().getDataIn()[in];
	if (value::isContainerType(desc.type)) {
		auto st = ctx.copyInputToOutput(in, out);
		if (st == Status::ErrorNotFound) {
			return ctx.claimOutput(out) != NullAddr ? Status::Ok : Status::ErrorInvalidArguemnt;
		}
		return st;
	}
	value::Var v;
	auto st = ctx.getInput(in, v);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(out, v);
}

// A call. The first turn asks the machine to open the function and produces nothing; the machine
// hands the node back once the function's work has run out, with the outputs already written by the
// return that ran, and the second turn leaves through that return's exit.
template <typename Ctx>
Status call(Ctx &ctx) {
	value::Var phase;
	auto st = ctx.getLocal(0, phase);
	if (st != Status::Ok) {
		return st;
	}
	if (phase.i == 0) {
		st = ctx.setLocal(0, value::makeInt(1));
		if (st == Status::Ok) {
			st = ctx.setLocal(1, value::makeInt(0));
		}
		return st == Status::Ok ? ctx.call() : st;
	}

	value::Var exit;
	st = ctx.getLocal(1, exit);
	if (st == Status::Ok) {
		st = ctx.setLocal(0, value::makeInt(0));
	}
	if (st != Status::Ok) {
		return st;
	}
	// Zero is "no return ran": the function's work ran out on a path that returned nothing.
	if (exit.i > 0 && !ctx.getOp().getExecOut().empty()) {
		return ctx.fire(uint32_t(exit.i - 1));
	}
	return Status::Ok;
}

// The entry of a called body: the call's inputs, or, when the body runs as the root of a run, the
// entry's own parameters.
template <typename Ctx>
Status entry(Ctx &ctx) {
	auto count = uint32_t(ctx.getOp().getDataIn().size());
	for (uint32_t i = 0; i < count; ++i) {
		auto st = ctx.copyArgument(i, i);
		if (st == Status::ErrorNotFound) {
			st = passPin(ctx, i, i);
		}
		if (st != Status::Ok) {
			return st;
		}
	}
	if (!ctx.getOp().getExecOut().empty()) {
		return ctx.fire(uint32_t(0));
	}
	return Status::Ok;
}

// A return of a called body: its inputs become the call's outputs, and its exit the one the call
// leaves by. A body run as the root of a run has no call to return to, and returns nothing.
template <typename Ctx>
Status ret(Ctx &ctx) {
	auto count = uint32_t(ctx.getOp().getDataIn().size());
	for (uint32_t i = 0; i < count; ++i) {
		auto st = ctx.copyResult(i);
		if (st == Status::ErrorNotFound) {
			return Status::Ok;
		}
		if (st != Status::Ok) {
			return st;
		}
	}
	auto st = ctx.setResultExit(ctx.getOp().getFunctionExit());
	return st == Status::ErrorNotFound ? Status::Ok : st;
}

// The entry of a substituted body: what the call site wired into the call.
template <typename Ctx>
Status arg(Ctx &ctx) {
	auto count = uint32_t(ctx.getOp().getDataIn().size());
	for (uint32_t i = 0; i < count; ++i) {
		auto st = passPin(ctx, i, i);
		if (st != Status::Ok) {
			return st;
		}
	}
	if (!ctx.getOp().getExecOut().empty()) {
		return ctx.fire(uint32_t(0));
	}
	return Status::Ok;
}

// A return of a substituted body: its values out to what the call site's outputs fed, and its exit.
template <typename Ctx>
Status result(Ctx &ctx) {
	auto count = uint32_t(ctx.getOp().getDataIn().size());
	for (uint32_t i = 0; i < count; ++i) {
		auto st = passPin(ctx, i, i);
		if (st != Status::Ok) {
			return st;
		}
	}
	if (!ctx.getOp().getExecOut().empty()) {
		return ctx.fire(ctx.getOp().getFunctionExit());
	}
	return Status::Ok;
}

} // namespace stappler::flow::fn

#endif /* STAPPLER_FLOW_SPFLOWFUNCTIONINLINE_H_ */
