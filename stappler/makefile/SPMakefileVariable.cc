/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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
#include "functions/SPMakefileFunctionExtension.cc"

namespace STAPPLER_VERSIONIZED stappler::makefile {

static Pair<StringView, Function> makeFn(StringView name, uint32_t nmin, uint32_t nmax,
		Function::Fn fn) {
	return pair(name, Function(name, nmin, nmax, nullptr, fn));
}

static sprt::__malloc_unordered_map< StringView, Function > s_functions{
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

	// extended (non-GNU) file I/O
	makeFn("xl_cat", 1, 1, Function_xl_cat),
	makeFn("xl_write", 2, 2, Function_xl_write),
	makeFn("xl_append", 2, 2, Function_xl_append),
	makeFn("xl_mkdir", 1, 1, Function_xl_mkdir),
	makeFn("xl_make_path", 1, 1, Function_xl_make_path),
	makeFn("xl_make_plain", 1, 1, Function_xl_make_plain),
	makeFn("xl_reverse", 1, 1, Function_xl_reverse),
	makeFn("xl_uniq", 1, 1, Function_xl_uniq),
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
	// Report a GNU-compatible version (the engine targets GNU make 4.4 semantics) with origin
	// "default", so a makefile may still override it — matching GNU, where MAKE_VERSION is a default.
	set("MAKE_VERSION", Origin::Default, "4.4.1");

	return true;
}

const Variable *VariableEngine::getIfDefined(StringView str) const {
	auto it = _variables.find(str);
	if (it != _variables.end()) {
		return &it->second;
	}
	return nullptr;
}

void VariableEngine::foreachVariable(const Callback<void(StringView, const Variable &)> &cb) const {
	for (auto &it : _variables) { cb(it.first, it.second); }
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

void VariableEngine::forceSet(StringView name, const Variable &v) {
	auto it = _variables.find(name);
	if (it != _variables.end()) {
		it->second = v;
	} else {
		_variables.emplace(name.pdup(_pool), v);
	}
}

void VariableEngine::forceErase(StringView name) {
	auto it = _variables.find(name);
	if (it != _variables.end()) {
		_variables.erase(it);
	}
}

void VariableEngine::addSubstitutionCallback(Origin o, VariableCallback::Fn fn, void *udata) {
	addSubstitutionCallback(new (_pool) VariableCallback(o, udata, fn));
}

void VariableEngine::addSubstitutionCallback(VariableCallback *cb) {
	sprt::emplace_ordered(_varCallbacks, cb, [](VariableCallback *l, VariableCallback *r) {
		return toInt(l->origin) > toInt(r->origin);
	});
}

void VariableEngine::setExportFlag(StringView name, bool exported) {
	auto it = _exportFlags.find(name);
	if (it != _exportFlags.end()) {
		it->second = exported;
	} else {
		_exportFlags.emplace(name.pdup(_pool), exported);
	}
}

int VariableEngine::getExportFlag(StringView name) const {
	auto it = _exportFlags.find(name);
	if (it != _exportFlags.end()) {
		return it->second ? 1 : 0;
	}
	return -1;
}

void VariableEngine::setEvalCallback(EvalFn fn, void *udata) {
	_evalFn = fn;
	_evalUserdata = udata;
}

bool VariableEngine::evalText(StringView content, ErrorReporter &err, const FileLocation *stmtLoc) {
	if (!_evalFn) {
		err.reportError("$(eval ...) is not supported: no eval callback registered");
		return false;
	}

	StringView blockName("eval");
	if (stmtLoc) {
		auto str = mem_pool::toString("eval(", stmtLoc->filename, ":", stmtLoc->lineno, ")");
		blockName = StringView(str).pdup();
	}
	return _evalFn(_evalUserdata, blockName, content);
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

	auto tmp = _currentBuffer;

	BufferTemplate<Interface> b(256);
	_currentBuffer = &b;

	resolve([&](StringView out) {
		b.put(out.data(), out.size()); //
	}, stmt, err);

	_currentBuffer = tmp;

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
		if (stmt->multiline) {
			// A multiline (`define`) WordList stores its whitespace — newlines and recipe
			// indentation — as explicit tokens. Emit everything verbatim, with no synthetic
			// word separators, so the value round-trips exactly (e.g. for $(eval)). Nested
			// $(...) expansions still resolve normally.
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
		}
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

			// The single argument spans every value after the name (val->next .. stmt->tail).
			// Use the real tail, not val->next: otherwise the single-word optimization in
			// resolve(Stmt*) sees value==tail and, when val->next is a plain string, returns
			// just that token and drops the rest of the argument.
			Stmt valueRoot(stmt->loc, StmtType::WordList, val->next, stmt->tail);
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

	// pool-backed (was a stack VLA sized by the argument count; a crafted makefile
	// with a huge argument list could overflow the stack before the maxArgs check)
	Vector<StringView> expandedArgs;
	expandedArgs.resize(args.size());

	CallContext ctx{_callContext};
	ctx.functionName = name;
	ctx.err = &err;
	ctx.args = args;
	ctx.fn = &it->second;
	ctx.expandedArgs = expandedArgs.data();
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

void VariableEngine::substitute(const Callback<void(StringView)> &out, StringView var,
		ErrorReporter &err) {
	var.trimChars<StringView::WhiteSpace>();
	if (var == "$") {
		out << "$";
		return;
	}
	// MAKEFILE_LIST is not handled here: it is maintained as a real, immediately-resolved variable
	// (appendMakefileList(), called as each makefile is parsed), so it expands like any other
	// variable — crucially also after parsing, when there is no block stack (e.g. recipe export).

	// Directory/file modifiers of an automatic variable: $(@D) $(@F) $(<D) $(<F) and so on.
	// The base autos (@ < ^ + ? * |) are injected as plain variables of Origin::Automatic
	// while a recipe is expanded; only intercept the two-character form when the base is
	// actually one of them so a real variable is never shadowed.
	if (var.size() == 2 && (var[1] == 'D' || var[1] == 'F')) {
		switch (var[0]) {
		case '@':
		case '<':
		case '^':
		case '+':
		case '?':
		case '*':
		case '|':
			if (auto v = getIfDefined(StringView(var.data(), 1))) {
				if (v->origin == Origin::Automatic && v->type == Variable::Type::String) {
					bool wantDir = (var[1] == 'D');
					bool first = true;
					v->str.split<StringView::WhiteSpace>([&](StringView tok) {
						if (first) {
							first = false;
						} else {
							out << ' ';
						}
						if (wantDir) {
							auto d = filepath::root(tok);
							out << (d.empty() ? StringView(".") : d);
						} else {
							out << filepath::lastComponent(tok);
						}
					});
					return;
				}
			}
			break;
		default: break;
		}
	}

	if (_callContext) {
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
			if (warnEnabled(EngineFlags::WarnSubstituteFunction)) {
				err.reportWarning(toString("Fail to substitute function ", var, " into string"));
			}
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

StringView encodePathSpaces(StringView in, mem_std::Interface::StringType &storage) {
	if (in.find(' ') == maxOf<size_t>()) {
		return in;
	}
	storage.assign(in.data(), in.size());
	for (auto &c : storage) {
		if (c == ' ') {
			c = PathSpacePlaceholder;
		}
	}
	return StringView(storage.data(), storage.size());
}

StringView decodePathSpaces(StringView in, mem_std::Interface::StringType &storage) {
	if (in.find(PathSpacePlaceholder) == maxOf<size_t>()) {
		return in;
	}
	storage.assign(in.data(), in.size());
	for (auto &c : storage) {
		if (c == PathSpacePlaceholder) {
			c = ' ';
		}
	}
	return StringView(storage.data(), storage.size());
}

void decodePathSpacesForShell(const Callback<void(StringView)> &out, StringView in, bool noEscape) {
	if (in.find(PathSpacePlaceholder) == maxOf<size_t>()) {
		out(in); // no placeholder: hand over the whole input in one chunk
		return;
	}
	bool inSingle = false; // POSIX single-quote state (cmd.exe does not honor ' as a quote)
	bool inDouble = false;
	size_t run = 0; // start of the current verbatim span (flushed whole before each replacement)
	for (size_t i = 0; i < in.size(); ++i) {
		char c = in[i];
		if (c == PathSpacePlaceholder) {
			if (i > run) {
				out(in.sub(run, i - run));
			}
			if (noEscape || inSingle || inDouble) {
				out(StringView(" ")); // author quotes (or opted out) -- a literal space
			} else {
#if SPRT_WINDOWS
				out(StringView("\" \""));
#else
				out(StringView("\\ "));
#endif
			}
			run = i + 1;
			continue;
		}
#if !SPRT_WINDOWS
		if (c == '\'' && !inDouble) {
			inSingle = !inSingle;
		} else if (c == '"' && !inSingle) {
			inDouble = !inDouble;
		}
#else
		if (c == '"') {
			inDouble = !inDouble;
		}
#endif
	}
	if (run < in.size()) {
		out(in.sub(run, in.size() - run)); // trailing verbatim span
	}
}

StringView VariableEngine::getAbsolutePath(StringView str) const {
	// A path arrives in the make-visible form, where any space inside it is PathSpacePlaceholder.
	// Decode it back to a real space first: everything below (toPosixPath/merge/reconstruct/findPath)
	// and every consumer of the result (stat, ::realpath, file open) needs the real filesystem path.
	// $(realpath)/$(abspath) re-encode their result via emitResolvedPath.
	mem_std::Interface::StringType spaceStorage;
	str = decodePathSpaces(str, spaceStorage);

	// A path may arrive in the platform-native form — on Windows that includes the `C:/dir` form the
	// path functions emit (and `C:\dir`, `c:/dir`). Normalize it to the internal posix form (`/c/dir`)
	// so the posix-based logic below recognizes a drive-rooted path as absolute instead of mistaking
	// it for a relative path and merging it onto the root. toPosixPath is a no-op on POSIX builds.
	mem_std::Interface::StringType posixStorage;
	str = filesystem::toPosixPath(str, posixStorage);

	auto keepRoot = [](StringView ret) -> StringView {
		// GNU make keeps "/" as "/". StringView::merge trims "/" to empty, and
		// reconstructPath(".") drops the dot, so joining onto the filesystem root
		// would otherwise yield an empty path — $(abspath .) / $(realpath .) fail
		// and defaults.mk's recursive sp_realpath blows the wasm stack.
		return ret.empty() ? StringView("/") : ret;
	};
	if (filepath::isAbsolute(str)) {
		auto ret = StringView(filepath::reconstructPath<Interface>(str)).pdup(_pool);
		ret.backwardSkipChars<StringView::Chars<'/'>>();
		return keepRoot(ret);
	}
	StringView base = _rootPath;
	if (base.empty()) {
		auto path = filesystem::findPath<Interface>(FileInfo{str}, filesystem::Access::Exists);
		if (!path.empty()) {
			return StringView(path).pdup(_pool);
		}
		return StringView();
	}
	if (str == "." || str == "./" || str.empty()) {
		auto ret = base;
		ret.backwardSkipChars<StringView::Chars<'/'>>();
		return keepRoot(ret);
	}
	auto rec = StringView(filepath::reconstructPath<Interface>(
								  filepath::merge<Interface>(base, str)))
					   .pdup(_pool);
	rec.backwardSkipChars<StringView::Chars<'/'>>();
	return keepRoot(rec);
}

bool VariableEngine::call(const Callback<void(StringView)> &out, StringView fn, StmtType type,
		StmtValue *args, ErrorReporter &err) {
	uint32_t nargs = VariableEngine_parseArguments(type, args, nullptr);

	// pool-backed (was a stack VLA sized by the parsed argument count)
	Vector<StmtValue *> argsBuf;
	argsBuf.resize(nargs);
	VariableEngine_parseArguments(type, args, argsBuf.data());

	return call(out, fn, SpanView(argsBuf.data(), nargs), err);
}

bool VariableEngine::checkRecursion(StringView name, Stmt *stmt, ErrorReporter &err) {
	if (sprt::find(_subStack.begin(), _subStack.end(), stmt) != _subStack.end()) {
		err.reportError(toString("Infinite recursive expansion detected: ", name), stmt);
		return true;
	} else {
		return false;
	}
}

void VariableEngine::appendMakefileList(StringView name) {
	// Accumulate makefile names into MAKEFILE_LIST as each file begins parsing (GNU make
	// semantics). Keeping it a real simple variable means $(MAKEFILE_LIST) — and idioms like
	// $(lastword $(MAKEFILE_LIST)) — resolve immediately and stay valid once parsing is over and
	// the block stack is gone (recipe / recursive-variable expansion), instead of crashing.
	// A makefile path may contain a space (e.g. ".../runtime 2/Makefile"); encode it so the list stays
	// one word per file and $(lastword)/$(dir $(lastword …)) keep resolving the whole path.
	mem_std::Interface::StringType spaceStorage;
	name = encodePathSpaces(name, spaceStorage);

	StringView value;
	auto cur = getIfDefined("MAKEFILE_LIST");
	if (cur && cur->type == Variable::Type::String && !cur->str.empty()) {
		value = StringView(mem_pool::toString(cur->str, " ", name)).pdup(_pool);
	} else {
		value = name.pdup(_pool);
	}
	forceSet("MAKEFILE_LIST", Variable(Origin::File, value));
}

void VariableEngine::pushBlock(Block *block) {
	block->outer = _currentBlock;
	_currentBlock = block;
}

void VariableEngine::popBlock() {
	_currentBlock = _currentBlock->outer; //
}

} // namespace stappler::makefile
