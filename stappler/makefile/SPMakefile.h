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

#ifndef CORE_MAKEFILE_SPMAKEFILE_H_
#define CORE_MAKEFILE_SPMAKEFILE_H_

#include "SPMakefileError.h"
#include "SPMakefileRule.h"
#include "SPMakefileVariable.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// Trust model: this module is a GNU-make-compatible build-system component and is
// designed to evaluate ONLY trusted makefiles (the project's own build scripts),
// exactly like GNU make itself. It deliberately implements full make semantics,
// including `$(shell ...)` (which runs an arbitrary command via popen) and the
// filesystem functions `$(wildcard)`/`$(realpath)`/`$(abspath)` and `include`.
//
// Consequently it is NOT a sandbox and MUST NOT be pointed at attacker-supplied
// makefile text: doing so is equivalent to executing that input. If untrusted
// makefiles ever need to be processed, run this in an OS-level sandbox or add an
// opt-in capability flag that disables `shell`/`include`/filesystem functions.
class Makefile : public memory::PoolObject {
public:
	using PathCallback = Callback<void(StringView)>;
	using IncludeCallback = void (*)(void *, StringView path, const PathCallback &);
	using LogCallback = void (*)(void *, log::LogType, StringView);

	using PoolObject::PoolObject;

	virtual ~Makefile() = default;

	bool init();

	void setLogCallback(LogCallback, void * = nullptr);
	void setIncludeCallback(IncludeCallback, void * = nullptr);
	void setRootPath(StringView);

	// Which diagnostic warnings the engine emits (see EngineFlags). Defaults to
	// EngineFlags::Default; pass EngineFlags::WarnAll to enable every warning.
	void setFlags(EngineFlags);
	EngineFlags getFlags() const;

	bool include(StringView name, StringView data, bool copyData = true, ErrorReporter * = nullptr);
	bool include(const FileInfo &, ErrorReporter * = nullptr, bool optional = false);
	bool includeFileByPath(StringView, ErrorReporter * = nullptr, bool optional = false);

	const Variable *assignSimpleVariable(StringView, Origin, StringView, bool multiline = false);
	const Variable *assignRecursiveVariable(StringView, Origin, StringView, bool multiline = false);
	const Variable *appendToVariable(StringView, Origin, StringView, bool multiline = false);

	const Variable *assignSimpleVariable(StringView, Origin, StringView, ErrorReporter &,
			bool multiline = false);
	const Variable *assignRecursiveVariable(StringView, Origin, StringView, ErrorReporter &,
			bool multiline = false);
	const Variable *appendToVariable(StringView, Origin, StringView, ErrorReporter &,
			bool multiline = false);

	const Variable *getVariable(StringView) const;

	// Enumerate every defined variable (name + raw Variable). Use getVariableValue() to
	// obtain the expanded value of a recursive variable.
	void foreachVariable(const Callback<void(StringView, const Variable &)> &) const;

	// Expand a variable's value to text (recursive variables are evaluated, like `$(NAME)`).
	void getVariableValue(StringView name, const Callback<void(StringView)> &, ErrorReporter &);

	// Expand a variable as a specific target sees it: the target's target-specific variables
	// (and its automatic variables) are in scope for the expansion. For external executors that
	// resolve variables/commands per target rather than consuming baked recipe lines.
	void getVariableValue(Target *, StringView name, const Callback<void(StringView)> &,
			ErrorReporter &);

	// content parsed as an included makefile
	// use $(print wordlist...) to output data
	bool eval(const Callback<void(StringView)> &, StringView name, StringView content);

	Target *addTarget(StringView name);
	bool addTargetPrerequisite(SpanView<Target *>, StringView decl, ErrorReporter &);

	bool undefineVariable(StringView, Origin, ErrorReporter &);

	// === Dependency-graph introspection (pure: no recipe is executed) ===

	// Look up a target by name; nullptr if it was never mentioned.
	Target *getTarget(StringView) const;

	// The default goal: the first explicitly declared, non-special, non-pattern target.
	Target *getDefaultGoal() const { return _defaultGoal; }

	// All explicitly declared targets (excludes '.'-special and '%'-pattern rules).
	Vector<Target *> getTargets() const;
	void foreachTarget(const Callback<void(Target *)> &) const;

	// Immediate prerequisites of a target, resolving a matching pattern rule when the
	// target has no explicit recipe. Each prerequisite name is passed to the callback.
	void getPrerequisites(Target *, const Callback<void(StringView)> &);
	void getOrderOnly(Target *, const Callback<void(StringView)> &);

	// Transitive closure of (normal, and optionally order-only) prerequisites,
	// deduplicated; order is unspecified.
	Vector<Target *> getTransitivePrerequisites(Target *, bool includeOrderOnly = true);

	// Resolve a goal into a topologically ordered build plan: dependencies precede
	// dependents, so the goal node is the last element. Each BuildNode is linked to its
	// prerequisite nodes. Returns an empty vector on a dependency cycle or unknown goal
	// (reported through the ErrorReporter).
	Vector<BuildNode *> buildPlan(Target *goal, ErrorReporter &);
	Vector<BuildNode *> buildPlan(StringView goalName, ErrorReporter &);

	// True if `make` would (re)build this target: phony, missing, or older than a
	// prerequisite. Fills the target's filesystem-state cache.
	bool isOutOfDate(Target *, ErrorReporter &);

	// === Recipe export (pure): fully expanded recipe text, no execution ===

	// Emit every recipe line of the target with variables, automatic variables and the
	// pattern stem substituted. Recipe-line prefixes (@, -, +) are stripped.
	void exportRecipe(Target *, const Callback<void(StringView)> &, ErrorReporter &);

	// Per-line variant exposing the decoded recipe-line prefixes.
	using RecipeLineCallback =
			Callback<void(StringView line, bool silent, bool ignoreErr, bool always)>;
	void exportRecipeLines(Target *, const RecipeLineCallback &, ErrorReporter &);

	// === Target scope (for external executors that drive their own build) ===

	// Enumerate a target's own target-specific variable assignments — name, operator token
	// ("=", ":=", "+=", ...), and the raw Variable — in source order. Introspection only.
	void foreachTargetVariable(Target *,
			const Callback<void(StringView, StringView, const Variable &)> &) const;

	// Run `fn` with `t`'s recipe context active: automatic variables ($@, $<, ...) and the
	// target's target-specific variables are pushed into the engine for the duration, then
	// restored. Inside `fn`, an external executor may expand any variable/expression as `t`
	// sees it. Do NOT call exportRecipe()/exportRecipeLines() from within `fn` (they self-
	// bracket the automatic variables and would tear down this scope). Returns false on a setup
	// error.
	bool withTargetScope(Target *, const Callback<void()> &fn, ErrorReporter &);

	// === Execution (side-effecting): runs recipes via the shell ===

	void setBuildOptions(const BuildOptions &o) { _buildOptions = o; }
	const BuildOptions &getBuildOptions() const { return _buildOptions; }

	BuildResult execute(Target *goal, ErrorReporter &);
	BuildResult execute(StringView goalName, ErrorReporter &);

protected:
	bool processMakefileContent(StringView str, ErrorReporter &);
	bool processMakefileLine(StringView str, ErrorReporter &);

	bool processIfdefLine(StringView &str, bool negative, ErrorReporter &, Block *original);
	bool processIfeqLine(StringView &str, bool negative, ErrorReporter &, Block *original);
	bool processElseLine(StringView &str, ErrorReporter &);
	bool processEndifLine(StringView &str, ErrorReporter &);

	bool processDefineLine(StringView &str, ErrorReporter &);
	bool processDefineContentLine(StringView &str, Block *, ErrorReporter &);
	bool processEndefLine(StringView &str, ErrorReporter &);
	bool processUndefineLine(StringView &str, Origin varOrigin, ErrorReporter &);
	bool processSimpleLine(StringView &str, Origin varOrigin, ErrorReporter &);

	// In a rule line `targets : <decl>`, detect and consume a target-specific variable
	// assignment (`VAR = value`, also :=, ::=, :::=, +=, ?=, and a leading `private`). Returns
	// true if it was an assignment (stored on each target via Target::addVariable); false to
	// fall through to ordinary prerequisite parsing.
	bool tryParseTargetVariable(SpanView<Target *>, StringView &decl, ErrorReporter &);

	bool processIncludeLine(StringView &str, ErrorReporter &, bool optional);

	// Looks up or lazily creates a target node (used for prerequisite names that have no
	// rule of their own); classifies it as special/pattern on creation. Unlike addTarget
	// it does not affect default-goal selection.
	Target *getOrCreateTarget(StringView name);

	// Applies special-target semantics (.PHONY/.PRECIOUS/.SUFFIXES/...) to the already
	// parsed prerequisites of a special target.
	void applySpecialTarget(Target *, ErrorReporter &);

	// If `t` has no explicit recipe, finds the best-matching pattern rule, records the
	// stem/source/recipe on `t`, and appends the stem-substituted prerequisite names.
	// Returns true if a pattern rule was matched.
	bool resolveImplicit(Target *t, Vector<StringView> &prereqs, Vector<StringView> &orderOnly);

	// Internal build-plan builder (DFS with memoization + cycle detection).
	BuildNode *buildPlanNode(Target *, ErrorReporter &, Map<Target *, BuildNode *> &memo,
			Vector<BuildNode *> &order, bool &cycle);

	// Builds a single (non-recursive) node with its immediate prerequisites linked, for
	// recipe export / per-target automatic-variable computation.
	BuildNode *makeShallowNode(Target *);

	// True if a planned node needs rebuilding (phony, missing, or older than a prereq).
	bool nodeOutOfDate(BuildNode *) const;

	// Runs a node's recipe lines via the shell; honors @/-/+ prefixes and BuildOptions.
	bool runRecipe(BuildNode *, ErrorReporter &);

	// Computes and injects this target's automatic variables into the engine; the matching
	// clearAutoVars() removes them. `outdated` lists prerequisites newer than the target.
	void setAutoVars(BuildNode *, ErrorReporter &);
	void clearAutoVars();

	// One saved global-variable snapshot, taken when a target's target-specific variables are
	// pushed into scope and restored exactly when they are popped.
	struct TargetVarSave {
		StringView name;
		bool existed = false;
		Variable saved;
		TargetVarSave(StringView n) : name(n), saved(Origin::Undefined, StringView()) { }
	};

	// Apply / restore a target's target-specific variables around recipe expansion (own-recipe
	// scope). push snapshots the affected globals and applies in source order; pop restores them
	// in reverse. The saved-state vector is a caller-local so nested/concurrent scopes are safe.
	void pushTargetVars(Target *, Vector<TargetVarSave> &, ErrorReporter &);
	void popTargetVars(Vector<TargetVarSave> &);

	// Stat the target's file (cached on the Target) for out-of-date comparisons. Pass
	// force = true to refresh a cached result, e.g. after the file may have changed on disk.
	void statTarget(Target *, bool force = false);

	uint32_t _errors = 0;

	Vector<Target *> _currentTargets;
	Map<StringView, Target *> _targets;

	Vector<Target *> _patternRules; // targets whose name contains '%'
	Target *_defaultGoal = nullptr; // first declared non-special, non-pattern target
	Target *_dotDefault = nullptr; // recipe from a .DEFAULT rule, if any
	Vector<StringView> _suffixes; // .SUFFIXES list (stored; suffix rules not yet applied)
	BuildOptions _buildOptions;

	void *_logCallbackRef = nullptr;
	LogCallback _logCallback = nullptr;

	void *_includeCallbackRef = nullptr;
	IncludeCallback _includeCallback = nullptr;

	VariableEngine _engine;
};

using MakefileRef = SharedRef<Makefile>;

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILE_H_ */
