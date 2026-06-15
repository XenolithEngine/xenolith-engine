/**
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

// The dependency-graph / recipe executor for Makefile. This file is part of the
// makefile unity build (included from SPMakefile.cpp). It implements the public
// introspection/export API and the side-effecting execute() path; both share the
// graph-resolution, pattern-matching and automatic-variable machinery so that "what
// would run" (export) and "what runs" (execute) never diverge.

#include "SPFilesystem.h"
#include "SPMakefile.h"

#include <stdio.h>

namespace STAPPLER_VERSIONIZED stappler::makefile {

// Copy a node list into pool storage as a stable SpanView for a BuildNode.
static SpanView<BuildNode *> Executor_copyNodes(memory::pool_t *pool,
		const Vector<BuildNode *> &v) {
	if (v.empty()) {
		return SpanView<BuildNode *>();
	}
	auto arr = (BuildNode **)memory::pool::palloc(pool, v.size() * sizeof(BuildNode *));
	for (size_t i = 0; i < v.size(); ++i) { arr[i] = v[i]; }
	return SpanView<BuildNode *>(arr, v.size());
}

// Peel the leading recipe-line prefixes (@ silent, - ignore-errors, + always-run) off
// an (already expanded) recipe line; advances `line` past them.
static void Executor_decodeRecipePrefix(StringView &line, bool &silent, bool &ignoreErr,
		bool &always) {
	line.skipChars<StringView::WhiteSpace>();
	bool more = true;
	while (more && !line.empty()) {
		switch (line[0]) {
		case '@':
			silent = true;
			++line;
			break;
		case '-':
			ignoreErr = true;
			++line;
			break;
		case '+':
			always = true;
			++line;
			break;
		default: more = false; break;
		}
	}
}

// === target lookup / classification ======================================================

void Makefile::applySpecialTarget(Target *t, ErrorReporter &err) {
	auto mark = [&](void (*set)(Target *)) {
		for (auto p = t->prerequisitesList; p; p = p->next) { set(getOrCreateTarget(p->name)); }
	};

	if (t->name == ".PHONY") {
		mark([](Target *x) { x->isPhony = true; });
	} else if (t->name == ".PRECIOUS") {
		mark([](Target *x) { x->isPrecious = true; });
	} else if (t->name == ".SECONDARY") {
		mark([](Target *x) { x->isSecondary = true; });
	} else if (t->name == ".INTERMEDIATE") {
		mark([](Target *x) { x->isIntermediate = true; });
	} else if (t->name == ".SUFFIXES") {
		for (auto p = t->prerequisitesList; p; p = p->next) { _suffixes.emplace_back(p->name); }
	} else if (t->name == ".DEFAULT") {
		_dotDefault = t;
	}
	// .NOTPARALLEL/.ONESHELL/.DELETE_ON_ERROR/.SILENT/.IGNORE: recognized, not yet acted on
}

Target *Makefile::getTarget(StringView name) const {
	auto it = _targets.find(name);
	return it != _targets.end() ? it->second : nullptr;
}

void Makefile::foreachTarget(const Callback<void(Target *)> &cb) const {
	for (auto &it : _targets) {
		auto t = it.second;
		if (!t->isSpecial && !t->isPattern) {
			cb(t);
		}
	}
}

Vector<Target *> Makefile::getTargets() const {
	Vector<Target *> ret;
	foreachTarget([&](Target *t) { ret.emplace_back(t); });
	return ret;
}

// === pattern-rule resolution =============================================================

bool Makefile::resolveImplicit(Target *t, Vector<StringView> &prereqs,
		Vector<StringView> &orderOnly) {
	if (t->hasRecipe()) {
		return false;
	}

	// pick the matching pattern rule (with a recipe) that yields the shortest stem
	Target *best = nullptr;
	StringView bestStem;
	for (auto pr : _patternRules) {
		if (!pr->hasRecipe()) {
			continue;
		}
		auto info = getPatternComponents(pr->name);
		StringView stem;
		if (matchPattern(info, t->name, stem)) {
			if (!best || stem.size() < bestStem.size()) {
				best = pr;
				bestStem = stem;
			}
		}
	}

	if (!best) {
		return false;
	}

	t->patternSource = best;
	t->patternRules = best->rulesList;
	t->stem = bestStem.pdup(_pool);

	auto subst = [&](Prerequisite *list, Vector<StringView> &out) {
		for (auto p = list; p; p = p->next) {
			auto info = getPatternComponents(p->name);
			if (info.isPattern) {
				out.emplace_back(StringView(toString(info.start, t->stem, info.end)).pdup(_pool));
			} else {
				out.emplace_back(p->name);
			}
		}
	};
	subst(best->prerequisitesList, prereqs);
	subst(best->orderOnlyList, orderOnly);
	return true;
}

void Makefile::getPrerequisites(Target *t, const Callback<void(StringView)> &cb) {
	perform([&] {
		for (auto p = t->prerequisitesList; p; p = p->next) { cb(p->name); }
		Vector<StringView> prereqs, orderOnly;
		if (resolveImplicit(t, prereqs, orderOnly)) {
			for (auto &n : prereqs) { cb(n); }
		}
		return true;
	});
}

void Makefile::getOrderOnly(Target *t, const Callback<void(StringView)> &cb) {
	perform([&] {
		for (auto p = t->orderOnlyList; p; p = p->next) { cb(p->name); }
		Vector<StringView> prereqs, orderOnly;
		if (resolveImplicit(t, prereqs, orderOnly)) {
			for (auto &n : orderOnly) { cb(n); }
		}
		return true;
	});
}

Vector<Target *> Makefile::getTransitivePrerequisites(Target *t, bool includeOrderOnly) {
	return perform([&] {
		Vector<Target *> ret;
		Set<Target *> seen;
		Vector<Target *> stack;
		seen.emplace(t);
		stack.emplace_back(t);
		while (!stack.empty()) {
			auto cur = stack.back();
			stack.pop_back();
			auto visit = [&](StringView name) {
				auto pt = getOrCreateTarget(name);
				if (seen.emplace(pt).second) {
					ret.emplace_back(pt);
					stack.emplace_back(pt);
				}
			};
			getPrerequisites(cur, [&](StringView n) { visit(n); });
			if (includeOrderOnly) {
				getOrderOnly(cur, [&](StringView n) { visit(n); });
			}
		}
		return ret;
	});
}

// === build plan (topological order + cycle detection) ====================================

BuildNode *Makefile::buildPlanNode(Target *t, ErrorReporter &err, Map<Target *, BuildNode *> &memo,
		Vector<BuildNode *> &order, bool &cycle) {
	auto mit = memo.find(t);
	if (mit != memo.end()) {
		return mit->second;
	}
	if (t->mark == Target::Mark::Gray) {
		err.reportError(toString("Circular dependency detected involving target '", t->name, "'"));
		cycle = true;
		return nullptr;
	}
	t->mark = Target::Mark::Gray;

	Vector<StringView> synthPre, synthOo;
	resolveImplicit(t, synthPre, synthOo);

	auto node = new (_pool) BuildNode(t);
	node->stem = t->stem;
	node->rules = t->effectiveRules();
	node->phony = t->isPhony;

	Vector<BuildNode *> pre, oo;
	auto addChild = [&](StringView name, Vector<BuildNode *> &dst) -> bool {
		auto ct = getOrCreateTarget(name);
		auto cn = buildPlanNode(ct, err, memo, order, cycle);
		if (cycle) {
			return false;
		}
		if (cn) {
			dst.emplace_back(cn);
		}
		return true;
	};

	for (auto p = t->prerequisitesList; p; p = p->next) {
		if (!addChild(p->name, pre)) {
			return nullptr;
		}
	}
	for (auto &n : synthPre) {
		if (!addChild(n, pre)) {
			return nullptr;
		}
	}
	for (auto p = t->orderOnlyList; p; p = p->next) {
		if (!addChild(p->name, oo)) {
			return nullptr;
		}
	}
	for (auto &n : synthOo) {
		if (!addChild(n, oo)) {
			return nullptr;
		}
	}

	node->prerequisites = Executor_copyNodes(_pool, pre);
	node->orderOnly = Executor_copyNodes(_pool, oo);

	t->mark = Target::Mark::Black;
	memo.emplace(t, node);
	order.emplace_back(node);
	return node;
}

Vector<BuildNode *> Makefile::buildPlan(Target *goal, ErrorReporter &err) {
	return perform([&] {
		Vector<BuildNode *> order;
		if (!goal) {
			err.reportError("buildPlan: no goal target");
			return order;
		}
		for (auto &it : _targets) { it.second->mark = Target::Mark::White; }
		Map<Target *, BuildNode *> memo;
		bool cycle = false;
		buildPlanNode(goal, err, memo, order, cycle);
		if (cycle) {
			order.clear();
		}
		return order;
	});
}

Vector<BuildNode *> Makefile::buildPlan(StringView goalName, ErrorReporter &err) {
	auto t = getTarget(goalName);
	if (!t) {
		err.reportError(toString("buildPlan: unknown goal '", goalName, "'"));
		return Vector<BuildNode *>();
	}
	return buildPlan(t, err);
}

BuildNode *Makefile::makeShallowNode(Target *t) {
	Vector<StringView> synthPre, synthOo;
	resolveImplicit(t, synthPre, synthOo);

	auto node = new (_pool) BuildNode(t);
	node->stem = t->stem;
	node->rules = t->effectiveRules();
	node->phony = t->isPhony;

	Vector<BuildNode *> pre, oo;
	for (auto p = t->prerequisitesList; p; p = p->next) {
		pre.emplace_back(new (_pool) BuildNode(getOrCreateTarget(p->name)));
	}
	for (auto &n : synthPre) { pre.emplace_back(new (_pool) BuildNode(getOrCreateTarget(n))); }
	for (auto p = t->orderOnlyList; p; p = p->next) {
		oo.emplace_back(new (_pool) BuildNode(getOrCreateTarget(p->name)));
	}
	for (auto &n : synthOo) { oo.emplace_back(new (_pool) BuildNode(getOrCreateTarget(n))); }
	node->prerequisites = Executor_copyNodes(_pool, pre);
	node->orderOnly = Executor_copyNodes(_pool, oo);
	return node;
}

// === filesystem state / out-of-date ======================================================

void Makefile::statTarget(Target *t, bool force) {
	if (t->isPhony) {
		return;
	}
	if (force || !t->hasStat) {
		t->fileExists = false;
		t->mtimeMicros = 0;

		auto path = _engine.getAbsolutePath(t->name);
		filesystem::Stat st;
		if (!path.empty() && filesystem::stat(FileInfo{path}, st)) {
			t->fileExists = true;
			t->mtimeMicros = st.mtime.toMicros();
		}
		t->hasStat = true;
	}
}

bool Makefile::isOutOfDate(Target *t, ErrorReporter &err) {
	return perform([&] {
		if (t->isPhony) {
			return true;
		}
		// force a fresh stat: isOutOfDate is a public query that must reflect the current
		// state of the filesystem, even if the target/prereqs were stat'd (and cached) by an
		// earlier query before the files changed on disk.
		statTarget(t, true);
		if (!t->fileExists) {
			return true;
		}
		bool stale = false;
		auto check = [&](StringView name) {
			if (stale) {
				return;
			}
			auto pt = getOrCreateTarget(name);
			if (pt->isPhony) {
				stale = true;
				return;
			}
			statTarget(pt, true);
			if (!pt->fileExists || pt->mtimeMicros > t->mtimeMicros) {
				stale = true;
			}
		};
		getPrerequisites(t, [&](StringView n) { check(n); });
		return stale;
	});
}

bool Makefile::nodeOutOfDate(BuildNode *node) const {
	if (node->phony) {
		return true;
	}
	if (!node->exists) {
		return true;
	}
	for (auto p : node->prerequisites) {
		if (p->phony || !p->exists || p->mtimeMicros > node->mtimeMicros) {
			return true;
		}
	}
	return false;
}

// === automatic variables =================================================================

void Makefile::setAutoVars(BuildNode *node, ErrorReporter &err) {
	auto t = node->target;

	statTarget(t);
	uint64_t targetMtime = t->mtimeMicros;
	bool targetExists = t->fileExists;

	StringStream dedup; // $^ (deduplicated)
	StringStream dup; // $+ (with duplicates)
	StringStream changed; // $? (newer-than-target)
	StringStream order; // $| (order-only)
	Set<StringView> seen;
	StringView first; // $<
	bool firstSet = false, dedupFirst = true, dupFirst = true, changedFirst = true,
		 orderFirst = true;

	for (auto pn : node->prerequisites) {
		StringView name = pn->name;
		if (!firstSet) {
			first = name;
			firstSet = true;
		}
		if (dupFirst) {
			dupFirst = false;
		} else {
			dup << ' ';
		}
		dup << name;

		if (seen.emplace(name).second) {
			if (dedupFirst) {
				dedupFirst = false;
			} else {
				dedup << ' ';
			}
			dedup << name;
		}

		statTarget(pn->target);
		bool newer =
				!targetExists || !pn->target->fileExists || pn->target->mtimeMicros > targetMtime;
		if (newer) {
			if (changedFirst) {
				changedFirst = false;
			} else {
				changed << ' ';
			}
			changed << name;
		}
	}

	for (auto on : node->orderOnly) {
		if (orderFirst) {
			orderFirst = false;
		} else {
			order << ' ';
		}
		order << on->name;
	}

	_engine.set("@", Origin::Automatic, t->name);
	_engine.set("*", Origin::Automatic, node->stem); // stem; empty for explicit rules
	_engine.set("<", Origin::Automatic, first.pdup(_pool));
	_engine.set("^", Origin::Automatic, StringView(dedup.weak()).pdup(_pool));
	_engine.set("+", Origin::Automatic, StringView(dup.weak()).pdup(_pool));
	_engine.set("?", Origin::Automatic, StringView(changed.weak()).pdup(_pool));
	_engine.set("|", Origin::Automatic, StringView(order.weak()).pdup(_pool));
}

void Makefile::clearAutoVars() {
	_engine.clear("@", Origin::Automatic);
	_engine.clear("*", Origin::Automatic);
	_engine.clear("<", Origin::Automatic);
	_engine.clear("^", Origin::Automatic);
	_engine.clear("+", Origin::Automatic);
	_engine.clear("?", Origin::Automatic);
	_engine.clear("|", Origin::Automatic);
}

void Makefile::pushTargetVars(Target *t, Vector<TargetVarSave> &saved, ErrorReporter &err) {
	for (auto v = t->variablesList; v; v = v->next) {
		// Snapshot the current global value for an exact restore on pop.
		TargetVarSave s(v->name);
		if (auto cur = _engine.getIfDefined(v->name)) {
			s.existed = true;
			s.saved = *cur;
		}
		saved.emplace_back(s);

		// Apply at Origin::File: set() honours overridability, so a command-line / override
		// global is left intact (command-line beats target-specific, matching GNU); the
		// snapshot/restore stays correct whether or not the apply took effect.
		if (v->op == "=") {
			if (v->value) {
				_engine.set(v->name, Origin::File, v->value);
			} else {
				_engine.set(v->name, Origin::File, StringView());
			}
		} else if (v->op == ":=" || v->op == "::=" || v->op == ":::=") {
			_engine.set(v->name, Origin::File,
					v->value ? _engine.resolve(v->value, err).pdup(_pool) : StringView());
		} else if (v->op == "?=") {
			if (!s.existed) {
				if (v->value) {
					_engine.set(v->name, Origin::File, v->value);
				} else {
					_engine.set(v->name, Origin::File, StringView());
				}
			}
		} else if (v->op == "+=") {
			// Own-recipe scope: combine eagerly into a simple value. This does NOT mutate the
			// snapshot's Stmt, so restore is exact (the trade-off is that a target-specific '+='
			// loses laziness — acceptable, the recipe expands it immediately anyway).
			StringStream combined;
			bool hasOld = false;
			if (s.existed) {
				StringView oldVal;
				if (s.saved.type == Variable::Type::String) {
					oldVal = s.saved.str;
				} else if (s.saved.type == Variable::Type::Stmt && s.saved.stmt) {
					oldVal = _engine.resolve(s.saved.stmt, err);
				}
				if (!oldVal.empty()) {
					combined << oldVal;
					hasOld = true;
				}
			}
			StringView newVal = v->value ? _engine.resolve(v->value, err) : StringView();
			if (hasOld && !newVal.empty()) {
				combined << " ";
			}
			combined << newVal;
			_engine.set(v->name, Origin::File, StringView(combined.weak()).pdup(_pool));
		}
	}
}

void Makefile::popTargetVars(Vector<TargetVarSave> &saved) {
	// Restore in reverse so repeated assignments to the same name unwind to the original global.
	for (size_t i = saved.size(); i > 0; --i) {
		auto &s = saved[i - 1];
		if (s.existed) {
			_engine.forceSet(s.name, s.saved);
		} else {
			_engine.forceErase(s.name);
		}
	}
	saved.clear();
}

bool Makefile::withTargetScope(Target *t, const Callback<void()> &fn, ErrorReporter &err) {
	if (!t) {
		return false;
	}
	return perform([&] {
		auto node = makeShallowNode(t);
		setAutoVars(node, err);
		Vector<TargetVarSave> saved;
		pushTargetVars(node->target, saved, err);
		fn();
		popTargetVars(saved);
		clearAutoVars();
		return true;
	});
}

// === recipe export (no execution) ========================================================

void Makefile::exportRecipeLines(Target *t, const RecipeLineCallback &cb, ErrorReporter &err) {
	perform([&] {
		auto node = makeShallowNode(t);
		setAutoVars(node, err);
		Vector<TargetVarSave> saved;
		pushTargetVars(node->target, saved, err); // after setAutoVars: a target var may use $@
		for (auto r = node->rules; r; r = r->next) {
			auto expanded = _engine.resolve(r->rule, err);
			StringView line(expanded);
			bool silent = false, ignoreErr = false, always = false;
			Executor_decodeRecipePrefix(line, silent, ignoreErr, always);
			cb(line, silent, ignoreErr, always);
		}
		popTargetVars(saved);
		clearAutoVars();
		return true;
	});
}

void Makefile::exportRecipe(Target *t, const Callback<void(StringView)> &out, ErrorReporter &err) {
	exportRecipeLines(t, [&](StringView line, bool, bool, bool) { out(line); }, err);
}

// === execution ===========================================================================

bool Makefile::runRecipe(BuildNode *node, ErrorReporter &err) {
	for (auto r = node->rules; r; r = r->next) {
		auto expanded = _engine.resolve(r->rule, err);
		StringView line(expanded);
		bool silent = false, ignoreErr = false, always = false;
		Executor_decodeRecipePrefix(line, silent, ignoreErr, always);
		if (line.empty()) {
			continue;
		}

		bool echo = !silent && !_buildOptions.silent;
		auto custom = _engine.getCustomOutput();
		if (echo) {
			if (custom) {
				(*custom) << line << "\n";
			} else {
				fprintf(stdout, "%.*s\n", int(line.size()), line.data());
			}
		}

		if (_buildOptions.dryRun && !always) {
			continue;
		}

		String cmd = line.str<Interface>();
		FILE *fp = popen(cmd.data(), "r");
		if (!fp) {
			err.reportError(toString("Failed to run recipe: '", cmd, '\''));
			if (!ignoreErr) {
				return false;
			}
			continue;
		}

		char buf[1_KiB];
		while (fgets(buf, sizeof(buf), fp)) {
			if (custom) {
				(*custom) << StringView(buf);
			} else {
				fputs(buf, stdout);
			}
		}

		int status = pclose(fp);
		if (status != 0 && !ignoreErr) {
			err.reportError(toString("Recipe for target '", node->target->name,
					"' failed with status ", status));
			return false;
		}
	}
	return true;
}

BuildResult Makefile::execute(Target *goal, ErrorReporter &err) {
	return perform([&]() -> BuildResult {
		if (!goal) {
			err.reportError("execute: no goal target");
			return BuildResult::NoRule;
		}

		auto plan = buildPlan(goal, err);
		if (plan.empty()) {
			return BuildResult::Cycle;
		}

		// initial stat for every node (topological order => updated as we build)
		for (auto node : plan) {
			statTarget(node->target);
			node->exists = node->target->fileExists;
			node->mtimeMicros = node->target->mtimeMicros;
		}

		bool builtAny = false;
		bool failed = false;

		for (auto node : plan) {
			if (failed && !_buildOptions.keepGoing) {
				break;
			}

			node->outOfDate = nodeOutOfDate(node);
			if (!node->outOfDate) {
				continue;
			}

			if (!node->rules) {
				// nothing to run: ok if the file exists or the target is phony,
				// otherwise there is no rule to make it
				if (!node->exists && !node->phony) {
					err.reportError(toString("No rule to make target '", node->target->name, "'"));
					failed = true;
				}
				continue;
			}

			setAutoVars(node, err);
			Vector<TargetVarSave> saved;
			pushTargetVars(node->target, saved, err); // after setAutoVars: target var may use $@
			bool ok = runRecipe(node, err);
			popTargetVars(saved);
			clearAutoVars();

			if (ok) {
				builtAny = true;
				statTarget(node->target, true); // force refresh: the recipe just wrote the file
				node->exists = node->target->fileExists;
				node->mtimeMicros = node->target->mtimeMicros;
			} else {
				failed = true;
			}
		}

		if (failed) {
			return BuildResult::Failed;
		}
		return builtAny ? BuildResult::Built : BuildResult::UpToDate;
	});
}

BuildResult Makefile::execute(StringView goalName, ErrorReporter &err) {
	auto t = getTarget(goalName);
	if (!t) {
		err.reportError(toString("execute: unknown goal '", goalName, "'"));
		return BuildResult::NoRule;
	}
	return execute(t, err);
}

} // namespace stappler::makefile
