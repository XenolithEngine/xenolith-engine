/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPCore.h"
#include "SPFilepath.h"
#include "SPMakefileStmt.h"
#include "SPMakefileVariable.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

static void Makefile_foreachVarName(const Callback<void(StringView)> &out, StmtValue *val);

static void Makefile_foreachVarName(const Callback<void(StringView)> &out, Stmt *val) {
	if (val->type == StmtType::Expansion) {
		if (!val->value->isStmt && !val->value->next) {
			out(val->value->str);
		} else {
			Makefile_foreachVarName(out, val->value);
		}
	} else {
		Makefile_foreachVarName(out, val->value);
	}
}

static void Makefile_foreachVarName(const Callback<void(StringView)> &out, StmtValue *val) {
	auto tmp = val;
	while (tmp) {
		if (tmp->isStmt) {
			Makefile_foreachVarName(out, tmp->stmt);
		}
		tmp = tmp->next;
	}
}

static uint32_t Function_callGetSourceArgsCount(Stmt *arg) {
	uint32_t ret = 0;
	Makefile_foreachVarName([&](StringView str) {
		auto n = str.readInteger(10).get(0);
		if (n > 0 && str.empty()) {
			ret = std::max(ret, uint32_t(n));
		}
	}, arg);
	return ret;
}

static bool Function_call(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto callContext = engine.getCallContext();
	auto name = engine.resolve(args[0], 0, *callContext->err);

	/*std::cout << name << "\n";
	uint32_t i = 0;
	for (auto &it : args) {
		std::cout << i << ": " << engine.resolve(it, 0, *engine.getCallContext()->err) << "\n";
		++i;
	}*/

	if (auto v = engine.get(name)) {
		callContext->userName = name;
		switch (v->type) {
		case Variable::Type::String:
			callContext->err->reportWarning(toString("Call with a static simple variable: '", name,
					"': consider replace :=/::= with ="));
			out << v->str;
			break;
		case Variable::Type::Stmt: {
			auto nargs = Function_callGetSourceArgsCount(v->stmt);
			if (hasFlag(engine.getFlags(), EngineFlags::Pedantic)) {
				if (args.size() < nargs + 1) {
					callContext->err->reportWarning(toString("User function '", name, "' uses ",
							nargs, " arguments, but ", args.size() - 1, " provided"));
				}
			}
			engine.resolve(out, v->stmt, *callContext->err);
			break;
		}
		case Variable::Type::Function:
			if (args.size() - 1 < v->fn->minArgs || args.size() - 1 > v->fn->maxArgs) {
				callContext->err->reportError(
						toString("User function '", name, "' uses from ", v->fn->minArgs, " to ",
								v->fn->maxArgs, " arguments, but ", args.size() - 1, " provided"));
				return false;
			}
			return perform([&] { return v->fn->fn(out, v->fn->userdata, engine, args.sub(1)); },
					engine.getPool());
			break;
		}
	} else {
		callContext->err->reportWarning(
				toString("Failed to call user function: '", name, "': variable is not defined"));
		return true;
	}
	return true;
}

} // namespace stappler::makefile
