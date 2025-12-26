/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef EXTRA_WEBSERVER_PUG_SPPUGCONTEXT_H_
#define EXTRA_WEBSERVER_PUG_SPPUGCONTEXT_H_

#include "SPPugExpression.h"
#include "SPPugVariable.h"
#include "SPPugTemplate.h"

namespace STAPPLER_VERSIONIZED stappler::pug {

class SP_PUBLIC Context : public memory::AllocPool {
public:
	static constexpr size_t StackSize = 256;

	using OutStream = stappler::Callback<void(StringView)>;
	using Callback = VarClass::Callback;
	using IncludeCallback = Function<bool(const StringView &, Context &, const OutStream &, Template::RunContext &)>;

	struct Mixin {
		const Template::Chunk *chunk;
		Vector<Pair<StringView, Expression *>> args;
		size_t required = 0;
	};

	struct VarScope : memory::AllocPool {
		memory::dict<String, VarStorage> namedVars;
		memory::dict<String, Mixin> mixins;
		VarScope *prev = nullptr;
	};

	struct VarList : memory::AllocPool {
		static constexpr size_t MinStaticVars = 8;

		size_t staticCount = 0;
		std::array<Var, MinStaticVars> staticList;
		Vector<Var> dynamicList;

		void emplace(Var &&);

		size_t size() const;
		Var * data();
	};

	static bool isConstExpression(const Expression &);
	static bool printConstExpr(const Expression &, const OutStream &, bool escapeOutput = false);
	static bool printAttrVar(const StringView &, const Expression &, const OutStream &, bool escapeOutput = false);
	static bool printAttrExpr(const Expression &, const OutStream &);

	Context();

	void setErrorStream(const OutStream &);

	bool print(const Expression &, const OutStream &, bool escapeOutput = false);
	bool printAttr(const StringView &name, const Expression &, const OutStream &, bool escapeOutput = false);
	bool printAttrExprList(const Expression &, const OutStream &);
	Var exec(const Expression &, const OutStream &, bool allowUndefined = false);

	void set(const StringView &name, const Value &, VarClass * = nullptr);
	void set(const StringView &name, Value &&, VarClass * = nullptr);
	void set(const StringView &name, bool isConst, const Value *, VarClass * = nullptr);

	void set(const StringView &name, VarClass *);
	void set(const StringView &name, Callback &&);

	bool setMixin(const StringView &name, const Template::Chunk *);
	const Mixin *getMixin(const StringView &name) const;

	const VarStorage *getVar(const StringView &name) const;

	VarClass * set(const StringView &name, VarClass &&);

	bool runInclude(const StringView &, const OutStream &, Template::RunContext &rctx);

	void setIncludeCallback(IncludeCallback &&);

	void pushVarScope(VarScope &);
	void popVarScope();

	void loadDefaults();

protected:
	IncludeCallback _includeCallback;
	VarScope *currentScope = nullptr;
	VarScope globalScope;
	Map<String, VarClass> classes;
};

}

#endif /* EXTRA_WEBSERVER_PUG_SPPUGCONTEXT_H_ */
