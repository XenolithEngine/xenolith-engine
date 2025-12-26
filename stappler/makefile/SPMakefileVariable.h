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

#ifndef CORE_MAKEFILE_SPMAKEFILEVARIABLE_H_
#define CORE_MAKEFILE_SPMAKEFILEVARIABLE_H_

#include "SPMakefileError.h"
#include "SPMakefileStmt.h"
#include "SPMakefileBlock.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

class VariableEngine;

struct Variable;

enum class EngineFlags : uint32_t {
	None,
	Pedantic = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(EngineFlags)

struct SP_PUBLIC Function : memory::AllocPool {
	using Fn = bool (*)(const Callback<void(StringView)> &, void *, VariableEngine &,
			SpanView<StmtValue *>);

	StringView name;
	uint32_t minArgs = 0;
	uint32_t maxArgs = 0;
	void *userdata = nullptr;
	Fn fn = nullptr;

	Function() = default;
	Function(StringView n, uint32_t nmin, uint32_t nmax, void *udata, Fn f)
	: name(n), minArgs(nmin), maxArgs(nmax), userdata(udata), fn(f) { }
};

struct SP_PUBLIC VariableCallback : memory::AllocPool {
	using Fn = bool (*)(void *, const Callback<void(StringView)> &, StringView);

	Origin origin;
	void *userdata = nullptr;
	Fn fn;

	VariableCallback(Origin o, void *u, Fn f) : origin(o), userdata(u), fn(f) { }
};

struct SP_PUBLIC Variable {
	enum class Type {
		Stmt,
		String,
		Function
	};

	Origin origin;
	Type type = Type::Stmt;

	union {
		Stmt *stmt;
		StringView str;
		Function *fn;
	};

	Variable(Origin o, Stmt *s) : origin(o), type(Type::Stmt), stmt(s) { }
	Variable(Origin o, StringView s) : origin(o), type(Type::String), str(s) { }
	Variable(Origin o, Function *f) : origin(o), type(Type::Function), fn(f) { }

	void set(Origin o, Stmt *s) {
		origin = o;
		type = Type::Stmt;
		stmt = s;
	}
	void set(Origin o, StringView s) {
		origin = o;
		type = Type::String;
		str = s;
	}
	void set(Origin o, Function *f) {
		origin = o;
		type = Type::Function;
		fn = f;
	}

	bool isOverridableBy(Origin) const;
};

struct SP_PUBLIC CallContext {
	CallContext *prev = nullptr;
	ErrorReporter *err = nullptr;
	StringView functionName;
	mutable StringView userName;
	Function *fn = nullptr;
	SpanView<StmtValue *> args;
	mutable StringView *expandedArgs = nullptr;
	Map<StringView, StringView> *contextVars = nullptr;
	memory::pool_t *pool = nullptr;
};

class SP_PUBLIC VariableEngine : public AllocBase {
public:
	using Output = const Callback<void(StringView)> &;

	bool init(memory::pool_t *);

	const Variable *getIfDefined(StringView) const;

	// If var is not defined, try to resolve in with SubstitutionCallback
	const Variable *get(StringView);

	const Variable *set(StringView, Origin o, Stmt *s);
	const Variable *set(StringView, Origin o, StringView);

	const Variable *set(StringView, Origin o, Function *);

	bool clear(StringView, Origin o);

	void addSubstitutionCallback(Origin, VariableCallback::Fn, void *);
	void addSubstitutionCallback(VariableCallback *);

	void setRootPath(StringView);

	StringView resolve(StmtValue *, char chain, ErrorReporter &err, memory::pool_t * = nullptr);
	StringView resolve(Stmt *, ErrorReporter &err, memory::pool_t * = nullptr);

	void resolve(Output, StmtValue *, char chain, ErrorReporter &err);
	void resolve(Output, Stmt *, ErrorReporter &err);

	bool call(Output, StringView, SpanView<StmtValue *>, ErrorReporter &err);

	void substitute(Output, StringView, ErrorReporter &err);

	const CallContext *getCallContext() const { return _callContext; }

	memory::pool_t *getPool() const { return _pool; }

	void pushBlock(Block *);
	void popBlock();

	Block *getCurrentBlock() const { return _currentBlock; }

	void setCustomOutput(const Callback<void(StringView)> *v) { _customOutput = v; }
	const Callback<void(StringView)> *getCustomOutput() const { return _customOutput; }

	EngineFlags getFlags() const { return _flags; }

	StringView getAbsolutePath(StringView) const;

protected:
	bool call(const Callback<void(StringView)> &out, StringView fn, StmtType type, StmtValue *args,
			ErrorReporter &err);

	bool checkRecursion(StringView, Stmt *, ErrorReporter &err);

	memory::pool_t *_pool = nullptr;
	Block *_currentBlock = nullptr;
	EngineFlags _flags = EngineFlags::None;

	CallContext _rootContext;
	CallContext *_callContext = nullptr;
	Map<StringView, Variable> _variables;
	Vector<VariableCallback *> _varCallbacks;
	Vector<Stmt *> _subStack;

	StringView _rootPath;
	const Callback<void(StringView)> *_customOutput;
};

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILEVARIABLE_H_ */
