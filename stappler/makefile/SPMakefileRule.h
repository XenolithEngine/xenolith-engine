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

#ifndef CORE_MAKEFILE_SPMAKEFILERULE_H_
#define CORE_MAKEFILE_SPMAKEFILERULE_H_

#include "SPMakefileError.h"
#include "SPMakefileStmt.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// Decomposition of a GNU-make pattern string (e.g. `%.o`, `src/%.c`) around its
// single (first unescaped) `%`. When the string has no `%`, `isPattern` is false and
// `start` holds the whole literal. `\%` escapes are resolved into `start`/`end`.
struct SP_PUBLIC PatternInfo {
	StringView start;
	StringView end;
	bool isPattern = true;
};

// Split a pattern string at its `%` into prefix/suffix (handles `\%` escapes).
SP_PUBLIC PatternInfo getPatternComponents(StringView str);

// True if `word` matches `pattern`. On a match against a `%`-pattern, `stem` is set to
// the substring captured by `%` (may be empty). For a literal pattern, `stem` is cleared.
SP_PUBLIC bool matchPattern(const PatternInfo &pattern, StringView word, StringView &stem);

struct Prerequisite : AllocBase {
	StringView name;
	Prerequisite *next = nullptr;

	Prerequisite(StringView s) : name(s) { }
};

struct Rule : AllocBase {
	Stmt *rule = nullptr;
	Rule *next = nullptr;

	Rule(Stmt *s) : rule(s) { }
};

struct Target : AllocBase {
	// transient color used by the dependency-graph DFS (cycle detection / topo order)
	enum class Mark : uint8_t {
		White,
		Gray,
		Black
	};

	StringView name;
	Prerequisite *prerequisitesList = nullptr;
	Prerequisite *prerequisitesTail = nullptr;
	Prerequisite *orderOnlyList = nullptr;
	Prerequisite *orderOnlyTail = nullptr;
	Rule *rulesList = nullptr;
	Rule *rulesTail = nullptr;

	Target(StringView s) : name(s) { }

	void addPrerequisite(StringView);
	void addOrderOnly(StringView);
	void addRule(Stmt *);

	bool hasRecipe() const { return rulesList != nullptr; }

	bool hasStat = false;

	// classification (set when the target is created / declared)
	bool isPhony = false; // listed as a prerequisite of .PHONY
	bool isPrecious = false; // .PRECIOUS
	bool isSecondary = false; // .SECONDARY
	bool isIntermediate = false; // .INTERMEDIATE
	bool isSpecial = false; // name starts with '.' (.PHONY, .DEFAULT, ...)
	bool isPattern = false; // name contains an (unescaped) '%'

	// implicit-rule instance data: set on a target whose recipe/prerequisites were
	// synthesized from a matching pattern rule
	StringView stem;
	Target *patternSource = nullptr;
	Rule *patternRules = nullptr; // effective recipe taken from the pattern rule, if any

	// transient dependency-graph state, reset at the start of each resolution pass
	Mark mark = Mark::White;

	// filesystem state cache, filled lazily during a resolution pass
	bool fileExists = false;
	uint64_t mtimeMicros = 0; // modification time in microseconds (filesystem::Stat::mtime)

	const Rule *effectiveRules() const { return rulesList ? rulesList : patternRules; }
};

// A resolved node in a build plan. Pure data, safe to hand to a library consumer: it is
// the fully-resolved view of a Target for a particular goal (pattern stems applied,
// prerequisites linked into a DAG), independent of the parser's mutable Target state.
struct SP_PUBLIC BuildNode : AllocBase {
	Target *target = nullptr;
	StringView name; // resolved output name (== target->name)
	StringView stem; // '%' stem if the recipe was matched via a pattern rule, else empty
	SpanView<BuildNode *> prerequisites; // normal prerequisites (affect out-of-date timing)
	SpanView<BuildNode *> orderOnly; // order-only prerequisites (ordering only)
	const Rule *rules = nullptr; // effective recipe (explicit or pattern), may be null
	bool phony = false;
	bool exists = false;
	bool outOfDate = false;
	uint64_t mtimeMicros = 0;

	BuildNode(Target *t) : target(t), name(t->name) { }
};

enum class BuildResult {
	UpToDate, // goal already up to date, nothing to do
	Built, // recipe(s) ran successfully
	Failed, // a recipe returned a non-zero status
	Cycle, // dependency cycle detected
	NoRule, // a required target has no rule and does not exist
};

struct BuildOptions {
	bool dryRun = false; // print recipes, do not run them
	bool keepGoing = false; // continue building independent targets after a failure
	bool silent = false; // never echo recipe lines, regardless of '@'
};

} // namespace stappler::makefile

#endif /* CORE_MAKEFILE_SPMAKEFILERULE_H_ */
