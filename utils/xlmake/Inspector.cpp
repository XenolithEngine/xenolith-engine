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

#include "Inspector.h"

namespace xlmake {

// Write `s` to stdout with path-space placeholders rendered GNU-make style: each internal 0x1F (a
// space *inside* a path, kept as a placeholder so the path stays a single word) is emitted as "\ " (a
// backslash-escaped space), exactly as GNU make prints a space-containing path in -p / target /
// prerequisite listings. Only placeholders are escaped — ordinary separator spaces in a value are
// never placeholders, so they pass through untouched — and the result is valid, round-trippable
// makefile syntax that the VSCode Makefile Tools extension parses like GNU's output. (Variable *names*
// are identifiers and never carry a placeholder.)
static void emitDecoded(StringView s) {
	if (s.find(makefile::PathSpacePlaceholder) == maxOf<size_t>()) {
		sprt::cout << s; // common case: no placeholder, emit verbatim
		return;
	}
	size_t start = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == makefile::PathSpacePlaceholder) {
			sprt::cout << StringView(s.data() + start, i - start) << "\\ ";
			start = i + 1;
		}
	}
	sprt::cout << StringView(s.data() + start, s.size() - start);
}

static void printVariable(Makefile *mk, StringView name, const Variable &v, ErrorReporter &err) {
	sprt::cout << name << " [" << getOriginName(v.origin) << ", ";
	switch (v.type) {
	case Variable::Type::String:
		sprt::cout << "simple] = ";
		emitDecoded(v.str);
		sprt::cout << "\n";
		break;
	case Variable::Type::Function: sprt::cout << "function] = <function>\n"; break;
	case Variable::Type::Stmt:
		sprt::cout << "recursive] = ";
		mk->getVariableValue(name, [&](StringView s) { emitDecoded(s); }, err);
		sprt::cout << "\n";
		break;
	}
}

void printDatabase(Makefile *mk, ErrorReporter &err) {
	// Banner — cosmetic, not parsed; emitted so anything sniffing for a GNU-make header is satisfied.
	sprt::cout << "# xlmake\n"
				  "\n" //
				  "# Make data base, printed by xlmake, GNU Make compatible format\n" //
				  "\n"; //

	// --- Variables ---
	sprt::cout << "# Variables\n\n";
	// Stappler makefile engine not saves initial text of variables, as GNU make
	// We can not provide a variable list in GNU database format
	sprt::cout << "\n";

	// --- Files (targets) ---
	// Emit ONLY explicitly-declared, non-special, non-pattern targets (exactly what getTargets()
	// returns). Prerequisite-only names stay absent, so the extension's extractor — which scans the
	// `# Files`..`# Finished Make data base` window for `name:` lines — sees precisely the declared
	// target set, with no need to reproduce GNU's `# Not a target:` blocks.
	sprt::cout << "# Files\n\n";
	for (auto t : mk->getTargets()) {
		if (t->isSpecial || t->isPattern) {
			continue;
		}
		emitDecoded(t->name);
		sprt::cout << ":";
		mk->getPrerequisites(t, [&](StringView name) {
			sprt::cout << " ";
			emitDecoded(name);
		});
		bool firstOrderOnly = true;
		mk->getOrderOnly(t, [&](StringView name) {
			if (firstOrderOnly) {
				sprt::cout << " |";
				firstOrderOnly = false;
			}
			sprt::cout << " ";
			emitDecoded(name);
		});
		sprt::cout << "\n";
		if (t->isPhony) {
			// exact text the extension scans for (one '#', two spaces)
			sprt::cout << "#  Phony target (prerequisite of .PHONY).\n";
		}
		sprt::cout << "\n";
	}

	sprt::cout << "# Finished Make data base\n";
}

int runInspect(Makefile *mk, const InspectConfig &cfg, const Vector<String> &makefilePaths,
		ErrorReporter &err) {
	bool anyAction = cfg.printVars || !cfg.vars.empty() || cfg.recipe || cfg.prereqs;

	// --- variables ---
	if (cfg.printVars) {
		// snapshot names first: expanding a recursive value may define new variables
		Vector<StringView> names;
		mk->foreachVariable([&](StringView name, const Variable &) { names.emplace_back(name); });
		for (auto name : names) {
			if (auto v = mk->getVariable(name)) {
				printVariable(mk, name, *v, err);
			}
		}
	}

	for (auto name : cfg.vars) {
		auto v = mk->getVariable(name);
		if (!v) {
			sprt::cout << name << " = <undefined>\n";
			continue;
		}
		sprt::cout << name << " = ";
		mk->getVariableValue(name, [&](StringView s) { emitDecoded(s); }, err);
		sprt::cout << "\n";
	}

	// --- target-based actions ---
	if (cfg.recipe || cfg.prereqs) {
		// populate prerequisite / pattern-derived targets so they are queryable by name
		if (auto g = mk->getDefaultGoal()) {
			mk->buildPlan(g, err);
		}

		Vector<Target *> targets;
		if (cfg.targets.empty()) {
			if (auto g = mk->getDefaultGoal()) {
				targets.emplace_back(g);
			} else {
				sprt::cerr << "xlmake: no target given and no default goal\n";
			}
		} else {
			for (auto tn : cfg.targets) {
				if (auto t = mk->getTarget(tn)) {
					targets.emplace_back(t);
				} else {
					memory::StandartInterface::StringType ns;
					sprt::cerr << "xlmake: unknown target: "
							   << makefile::decodePathSpaces(tn, ns) << "\n";
				}
			}
		}

		if (cfg.recipe) {
			for (auto t : targets) {
				if (cfg.outOfDate && !mk->isOutOfDate(t, err)) {
					continue;
				}
				emitDecoded(t->name);
				sprt::cout << ":\n";
				mk->exportRecipe(t, [&](StringView line) {
					sprt::cout << "\t";
					emitDecoded(line);
					sprt::cout << "\n";
				}, err);
			}
		}

		if (cfg.prereqs && cfg.recursive) {
			// Transitive closure in dependency-graph order. buildPlan() returns the
			// targets topologically sorted (dependencies precede dependents) with the
			// goal itself last; we drop the goal and keep its prerequisites, which
			// already include order-only deps as graph edges.
			for (auto t : targets) {
				auto plan = mk->buildPlan(t, err);

				// With -q, restrict to what would actually be rebuilt. This is
				// transitive: a node rebuilds if it is out of date itself OR any normal
				// prerequisite rebuilds (so a fresh object forces its archive/link to
				// rebuild too). Order-only edges never propagate a rebuild. The plan is
				// in post-order, so every node's prerequisites are decided before it.
				//
				// By default a phony target counts as out of date (its isOutOfDate
				// state, consistent with the non-recursive --prerequisites -q view).
				// With --phony-prereqs (-P) a phony target is instead judged by its
				// prerequisites: a phony grouping target (all/install/...) whose
				// dependencies are all fresh is NOT reported, and it does not force a
				// fresh parent to rebuild either -- its own out-of-dateness then ignores
				// phony prerequisites in the timestamp check, so their effect arrives only
				// through the cascade (p->outOfDate).
				//
				// Order-only prerequisites are excluded from the out-of-date set: they
				// only constrain ordering and never trigger a rebuild, so we report just
				// the closure reachable through normal edges. `reachable` collects that
				// closure from the goal (its absence is only consulted when -q is set).
				Set<BuildNode *> reachable;
				if (cfg.outOfDate) {
					for (auto node : plan) {
						auto planTarget = node->target;
						bool rebuild;
						if (!cfg.phonyPrereqs) {
							// default: phony counts as out of date (make semantics)
							rebuild = mk->isOutOfDate(planTarget, err);
						} else if (planTarget->isPhony) {
							rebuild = false; // decided solely by the cascade below
						} else {
							// isOutOfDate() is documented to fill the target's
							// filesystem-state cache; read it back to compare timestamps
							// while skipping phony (file-less) prerequisites.
							mk->isOutOfDate(planTarget, err);
							rebuild = !planTarget->fileExists;
							if (!rebuild) {
								for (auto p : node->prerequisites) {
									auto pt = p->target;
									if (!pt->isPhony
											&& (!pt->fileExists
													|| pt->mtimeMicros > planTarget->mtimeMicros)) {
										rebuild = true;
										break;
									}
								}
							}
						}
						if (!rebuild) {
							for (auto p : node->prerequisites) {
								if (p->outOfDate) {
									rebuild = true;
									break;
								}
							}
						}
						node->outOfDate = rebuild;
					}

					Vector<BuildNode *> stack;
					for (auto node : plan) {
						if (node->target == t) {
							for (auto p : node->prerequisites) {
								if (reachable.emplace(p).second) {
									stack.emplace_back(p);
								}
							}
							break;
						}
					}
					while (!stack.empty()) {
						auto cur = stack.back();
						stack.pop_back();
						for (auto p : cur->prerequisites) {
							if (reachable.emplace(p).second) {
								stack.emplace_back(p);
							}
						}
					}
				}

				emitDecoded(t->name);
				sprt::cout << ":";
				for (auto node : plan) {
					if (node->target == t) {
						continue; // the goal node itself
					}
					if (cfg.outOfDate
							&& (!node->outOfDate || reachable.find(node) == reachable.end())) {
						continue;
					}
					sprt::cout << " ";
					emitDecoded(node->name);
				}
				sprt::cout << "\n";
			}
		} else if (cfg.prereqs) {
			for (auto t : targets) {
				emitDecoded(t->name);
				sprt::cout << ":";
				mk->getPrerequisites(t, [&](StringView name) {
					if (cfg.outOfDate) {
						auto pt = mk->getTarget(name);
						if (pt && !mk->isOutOfDate(pt, err)) {
							return;
						}
					}
					sprt::cout << " ";
					emitDecoded(name);
				});
				sprt::cout << "\n";

				bool hasOrderOnly = false;
				mk->getOrderOnly(t, [&](StringView name) {
					if (!hasOrderOnly) {
						sprt::cout << "    | order-only:";
						hasOrderOnly = true;
					}
					sprt::cout << " ";
					emitDecoded(name);
				});
				if (hasOrderOnly) {
					sprt::cout << "\n";
				}
			}
		}
	}

	// --- overview (no explicit action) ---
	if (!anyAction) {
		sprt::cout << "makefile:";
		for (auto &p : makefilePaths) {
			sprt::cout << " ";
			emitDecoded(StringView(p.data(), p.size()));
		}
		sprt::cout << "\n";
		if (auto g = mk->getDefaultGoal()) {
			sprt::cout << "default goal: ";
			emitDecoded(g->name);
			sprt::cout << "\n";
		}
		sprt::cout << "targets:\n";
		for (auto t : mk->getTargets()) {
			sprt::cout << "  ";
			emitDecoded(t->name);
			if (t->isPhony) {
				sprt::cout << " (phony)";
			}
			sprt::cout << "\n";
		}
	}

	return 0;
}

} // namespace xlmake
