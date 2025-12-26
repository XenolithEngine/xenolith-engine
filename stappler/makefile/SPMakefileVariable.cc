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

#include "SPMakefileVariable.h"
#include "SPMakefileStmt.h"
#include "functions/SPMakefileFunction.cc"
#include "functions/SPMakefileFunctionCall.cc"
#include "functions/SPMakefileFunctionFileName.cc"
#include "functions/SPMakefileFunctionString.cc"
#include "functions/SPMakefileFunctionConditional.cc"

namespace STAPPLER_VERSIONIZED stappler::makefile {

static Pair<StringView, Function> makeFn(StringView name, uint32_t nmin, uint32_t nmax,
		Function::Fn fn) {
	return pair(name, Function(name, nmin, nmax, nullptr, fn));
}

static std::unordered_map< StringView, Function > s_functions{
	makeFn("foreach", 3, 3, Function_foreach),
	makeFn("let", 3, 3, Function_let),
	makeFn("shell", 1, 1, Function_shell),
	makeFn("call", 1, maxOf<uint32_t>(), Function_call),
	makeFn("origin", 1, 1, Function_origin),
	makeFn("flavor", 1, 1, Function_flavor),

	makeFn("error", 1, 1, Function_error),
	makeFn("warning", 1, 1, Function_warning),
	makeFn("info", 1, 1, Function_info),
	makeFn("eval", 1, 1, Function_eval),
	makeFn("print", 1, 1, Function_print),

	makeFn("subst", 3, 3, Function_subst),
	makeFn("patsubst", 3, 3, Function_patsubst),
	makeFn("strip", 1, 1, Function_strip),
	makeFn("findstring", 2, 2, Function_findstring),
	makeFn("filter", 2, 2, Function_filter),
	makeFn("filter-out", 2, 2, Function_filter_out),
	makeFn("sort", 1, 1, Function_sort),
	makeFn("word", 2, 2, Function_word),
	makeFn("wordlist", 3, 3, Function_wordlist),
	makeFn("words", 1, 1, Function_words),
	makeFn("firstword", 1, 1, Function_firstword),
	makeFn("lastword", 1, 1, Function_lastword),

	makeFn("dir", 1, maxOf<uint32_t>(), Function_dir),
	makeFn("notdir", 1, maxOf<uint32_t>(), Function_notdir),
	makeFn("suffix", 1, maxOf<uint32_t>(), Function_suffix),
	makeFn("basename", 1, maxOf<uint32_t>(), Function_basename),
	makeFn("addsuffix", 2, 2, Function_addsuffix),
	makeFn("addprefix", 2, 2, Function_addprefix),
	makeFn("join", 2, 2, Function_join),
	makeFn("wildcard", 1, 1, Function_wildcard),
	makeFn("realpath", 1, maxOf<uint32_t>(), Function_realpath),
	makeFn("abspath", 1, maxOf<uint32_t>(), Function_abspath),

	makeFn("if", 2, 3, Function_if),
	makeFn("or", 1, maxOf<uint32_t>(), Function_or),
	makeFn("and", 1, maxOf<uint32_t>(), Function_and),
};

StringView getOriginName(Origin o) {
	switch (o) {
	case Origin::Undefined: return StringView("undefined"); break;
	case Origin::Default: return StringView("default"); break;
	case Origin::Automatic: return StringView("automatic"); break;
	case Origin::Environment: return StringView("environment"); break;
	case Origin::File: return StringView("file"); break;
	case Origin::EnvironmentOverride: return StringView("environment override"); break;
	case Origin::CommandLine: return StringView("command line"); break;
	case Origin::Override: return StringView("override"); break;
	}
	return StringView("undefined");
}

bool Variable::isOverridableBy(Origin o) const {
	return toInt(o) >= toInt(origin) || o == Origin::Override;
}

bool VariableEngine::init(memory::pool_t *pool) {
	_pool = pool;

	_callContext = &_rootContext;

	set(".STAPPLER_BUILD", Origin::Override, "1");
	set("MAKE_VERSION", Origin::Override, "0.0");

	return true;
}

const Variable *VariableEngine::getIfDefined(StringView str) const {
	auto it = _variables.find(str);
	if (it != _variables.end()) {
		return &it->second;
	}
	return nullptr;
}

const Variable *VariableEngine::get(StringView str) {
	auto it = _variables.find(str);
	if (it != _variables.end()) {
		return &it->second;
	}

	BufferTemplate<Interface> buf(256);

	for (auto &it : _varCallbacks) {
		buf.clear();
		if (it->fn(it->userdata, [&](StringView str) { buf.put(str.data(), str.size()); }, str)) {
			return perform([&]() { return set(str, it->origin, buf.get().pdup(_pool)); }, _pool);
		}
	}
	return nullptr;
}

const Variable *VariableEngine::set(StringView name, Origin o, Stmt *s) {
	auto it = _variables.find(name);
	if (it != _variables.end()) {
		if (it->second.isOverridableBy(o)) {
			it->second.set(o, s);
		}
		return &it->second;
	} else {
		return &_variables.emplace(name.pdup(_pool), Variable(o, s)).first->second;
	}
}

const Variable *VariableEngine::set(StringView name, Origin o, StringView value) {
	auto it = _variables.find(name);
	if (it != _variables.end()) {
		if (it->second.isOverridableBy(o)) {
			it->second.set(o, value);
		}
		return &it->second;
	} else {
		return &_variables.emplace(name.pdup(_pool), Variable(o, value)).first->second;
	}
}

const Variable *VariableEngine::set(StringView name, Origin o, Function *f) {
	auto it = _variables.find(name);
	if (it != _variables.end()) {
		if (it->second.isOverridableBy(o)) {
			it->second.set(o, f);
		}
		return &it->second;
	} else {
		return &_variables.emplace(name.pdup(_pool), Variable(o, f)).first->second;
	}
}

bool VariableEngine::clear(StringView name, Origin o) {
	auto it = _variables.find(name);
	if (it != _variables.end()) {
		if (it->second.isOverridableBy(o)) {
			_variables.erase(it);
			return true;
		}
	}
	return false;
}

void VariableEngine::addSubstitutionCallback(Origin o, VariableCallback::Fn fn, void *udata) {
	addSubstitutionCallback(new (_pool) VariableCallback(o, udata, fn));
}

void VariableEngine::addSubstitutionCallback(VariableCallback *cb) {
	_varCallbacks.emplace_back(cb);
	std::sort(_varCallbacks.begin(), _varCallbacks.end(),
			[](VariableCallback *l, VariableCallback *r) {
		return toInt(l->origin) > toInt(r->origin);
	});
}

void VariableEngine::setRootPath(StringView str) {
	if (filepath::isAbsolute(str)) {
		_rootPath = str.pdup(_pool);
	} else {
		_rootPath = StringView(filesystem::findPath<Interface>(FileInfo{str})).pdup(_pool);
	}
}

StringView VariableEngine::resolve(StmtValue *val, char chain, ErrorReporter &err,
		memory::pool_t *pool) {
	if (!chain || !val->next) {
		if (val->isStmt) {
			return resolve(val->stmt, err, pool);
		} else {
			return val->str;
		}
	} else {
		BufferTemplate<Interface> b(256);
		resolve([&](StringView out) { b.put(out.data(), out.size()); }, val, chain, err);
		return StringView(b.get()).pdup(pool);
	}
}

StringView VariableEngine::resolve(Stmt *stmt, ErrorReporter &err, memory::pool_t *pool) {
	if (!stmt) {
		return StringView();
	}

	// optimization for a single word in statement
	if (stmt->tail == stmt->value && !stmt->value->isStmt) {
		return stmt->value->str;
	}

	BufferTemplate<Interface> b(256);

	resolve([&](StringView out) { b.put(out.data(), out.size()); }, stmt, err);

	return StringView(b.get()).pdup(pool);
}

void VariableEngine::resolve(Output out, StmtValue *val, char chain, ErrorReporter &err) {
	auto orig = val;
	while (val) {
		if (orig != val) {
			out << chain;
		}

		if (val->isStmt) {
			resolve(out, val->stmt, err);
		} else {
			out(val->str);
		}
		val = chain ? val->next : nullptr;
	}
}

void VariableEngine::resolve(Output out, Stmt *stmt, ErrorReporter &_err) {
	StmtValue *val = stmt->value;
	if (!val) {
		return;
	}

	bool spaceValue = false;

	auto isWhitespaceStarted = [](StringView s) {
		return !s.readChars<StringView::WhiteSpace>().empty();
	};
	auto isWhitespaceEnded = [](StringView s) {
		return !s.backwardReadChars<StringView::WhiteSpace>().empty();
	};

	_subStack.emplace_back(stmt);

	switch (stmt->type) {
	case StmtType::Word:
		do {
			if (val->isStmt) {
				if (val->stmt) {
					resolve(out, val->stmt, _err);
				}
			} else {
				out << val->str;
			}
			val = val->next;
		} while (val);
		break;
	case StmtType::WordList:
		do {
			if (val != stmt->value) {
				if (!spaceValue && (val->isStmt || !isWhitespaceStarted(val->str))) {
					out << " ";
				}
			}
			if (spaceValue) {
				spaceValue = false;
			}
			if (val->isStmt) {
				if (val->stmt) {
					resolve(out, val->stmt, _err);
				}
			} else {
				out << val->str;
				if (isWhitespaceEnded(val->str)) {
					spaceValue = true;
				}
			}
			val = val->next;
		} while (val);
		break;
	case StmtType::ArgumentList: {
		ErrorReporter err(stmt->loc, &_err);

		auto varName = (val->isStmt) ? resolve(val->stmt, err) : val->str;
		if (!call(out, varName, StmtType::ArgumentList, val->next, err)) {
			stmt->describe();
		}
		break;
	}
	case StmtType::Expansion: {
		auto varName = (val->isStmt) ? resolve(val->stmt, _err) : val->str;
		if (val->next) {
			ErrorReporter err(stmt->loc, &_err);

			Stmt valueRoot(stmt->loc, StmtType::WordList, val->next, val->next);
			StmtValue fakeValue(&valueRoot);

			Stmt fakeRoot(stmt->loc, StmtType::ArgumentList, &fakeValue, &fakeValue);

			// function call
			if (!call(out, varName, StmtType::Expansion, &fakeValue, err)) {
				stmt->describe();
			}
		} else {
			// substitution
			substitute(out, varName, _err);
		}
		break;
	}
	}

	_subStack.pop_back();
}

bool VariableEngine::call(Output out, StringView name, SpanView<StmtValue *> args,
		ErrorReporter &err) {
	auto it = s_functions.find(name);
	if (it == s_functions.end()) {
		err.reportError(toString("Undefined function:'", name, "'"));
		return false;
	}

	StringView expandedArgs[args.size()];

	for (uint32_t i = 0; i < args.size(); ++i) { new (&expandedArgs[i]) StringView(); }

	CallContext ctx{_callContext};
	ctx.functionName = name;
	ctx.err = &err;
	ctx.args = args;
	ctx.fn = &it->second;
	ctx.expandedArgs = expandedArgs;
	ctx.pool = memory::pool::create(_pool);

	auto ret = mem_pool::perform([&] {
		ctx.contextVars = new (ctx.pool) Map<StringView, StringView>();

		if (args.size() < ctx.fn->minArgs || args.size() > ctx.fn->maxArgs) {
			err.reportError(toString("Function '", name, "' uses from ", ctx.fn->minArgs, " to ",
					ctx.fn->maxArgs, " arguments, but ", args.size(), " provided"));
			return false;
		}

		_callContext = &ctx;

		auto success = ctx.fn->fn(out, ctx.fn->userdata, *this, args);

		_callContext = ctx.prev;
		return success;
	}, ctx.pool);

	memory::pool::destroy(ctx.pool);
	ctx.pool = nullptr;

	return ret;
}

static bool VariableEngine_MAKEFILE_LIST(const Callback<void(StringView)> &out, Block *block) {
	bool ret = false;
	if (block->outer) {
		ret = VariableEngine_MAKEFILE_LIST(out, block->outer);
	}
	if (block->type == Keyword::None) {
		if (ret) {
			out(" ");
		}
		out(block->content);
		return true;
	}
	return ret;
}

void VariableEngine::substitute(const Callback<void(StringView)> &out, StringView var,
		ErrorReporter &err) {
	var.trimChars<StringView::WhiteSpace>();
	if (var == "$") {
		out << "$";
		return;
	} else if (var == "MAKEFILE_LIST") {
		VariableEngine_MAKEFILE_LIST(out, _currentBlock);
		return;
	} else if (_callContext) {
		// try indexed args
		StringView tmp(var);
		auto val = tmp.readInteger(10);
		if (tmp.empty() && val.valid()) {
			auto n = uint32_t(val.get(0));
			auto context = _callContext;
			while (context && (context->userName.empty() || context->args.size() <= n)) {
				context = context->prev;
			}

			if (context) {
				if (context->expandedArgs[n].empty()) {
					auto tmp = _callContext;
					_callContext = context->prev;
					context->expandedArgs[n] = resolve(context->args[n], 0, err, context->pool);
					_callContext = tmp;
				}
				out(context->expandedArgs[n]);
				return;
			}
		}

		// try context var

		auto context = _callContext;
		while (context) {
			if (context->contextVars) {
				auto it = context->contextVars->find(var);
				if (it != context->contextVars->end()) {
					out(it->second);
					return;
				}
			}
			context = context->prev;
		}
	}

	if (auto v = get(var)) {
		switch (v->type) {
		case Variable::Type::String: out(v->str); break;
		case Variable::Type::Stmt:
			if (!checkRecursion(var, v->stmt, err)) {
				resolve(out, v->stmt, err);
			}
			break;
		default:
			err.reportWarning(toString("Fail to substitute function ", var, " into string"));
			break;
		}
	}
}

static uint32_t VariableEngine_parseArguments(StmtType t, StmtValue *args, StmtValue **argsBuf) {
	uint32_t count = 0;
	switch (t) {
	case StmtType::ArgumentList:
		while (args) {
			if (argsBuf) {
				argsBuf[count] = args;
			}
			++count;
			args = args->next;
		}
		break;
	case StmtType::Expansion:
		if (args) {
			if (argsBuf) {
				argsBuf[count] = args;
			}
			count = 1;
		}
		break;
	default: break;
	}
	return count;
}

StringView VariableEngine::getAbsolutePath(StringView str) const {
	if (filepath::isAbsolute(str)) {
		return StringView(filepath::reconstructPath<Interface>(str)).pdup(_pool);
	} else {
		if (!_rootPath.empty()) {
			return StringView(filepath::reconstructPath<Interface>(
									  filepath::merge<Interface>(_rootPath, str)))
					.pdup(_pool);
		} else {
			auto path = filesystem::findPath<Interface>(FileInfo{str}, filesystem::Access::Exists);
			if (!path.empty()) {
				return StringView(path).pdup(_pool);
			}
		}
	}
	return StringView();
}

bool VariableEngine::call(const Callback<void(StringView)> &out, StringView fn, StmtType type,
		StmtValue *args, ErrorReporter &err) {
	uint32_t nargs = VariableEngine_parseArguments(type, args, nullptr);

	StmtValue *argsBuf[nargs];
	VariableEngine_parseArguments(type, args, argsBuf);

	return call(out, fn, SpanView(argsBuf, nargs), err);
}

bool VariableEngine::checkRecursion(StringView name, Stmt *stmt, ErrorReporter &err) {
	if (std::find(_subStack.begin(), _subStack.end(), stmt) != _subStack.end()) {
		err.reportError(toString("Infinite recursive expansion detected: ", name), stmt);
		return true;
	} else {
		return false;
	}
}

void VariableEngine::pushBlock(Block *block) {
	block->outer = _currentBlock;
	_currentBlock = block;
}

void VariableEngine::popBlock() { _currentBlock = _currentBlock->outer; }

} // namespace stappler::makefile
