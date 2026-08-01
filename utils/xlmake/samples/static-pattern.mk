# Sample makefile for xlmake: static pattern rules and globs in directory components.
#
# A static pattern rule — `targets… : target-pattern : prereq-patterns…` — applies ONE recipe to an
# explicit list of targets, matching each name against the target pattern to get its stem. The stem
# is `$*` in the recipe and replaces every '%' in the prerequisite patterns. It is recognized by the
# SECOND rule ':' whose left side holds a '%' — which is what keeps `t: VAR = value` (target
# variable) and `t: ; cmd` (inline recipe) from being read as one.
#
# The sample also exercises $(wildcard) with a glob in a NON-final path component (`*/`,`*/*.mk`),
# which is how a build discovers per-directory description files.
#
#   xlmake -f static-pattern.mk all          vs   make -f static-pattern.mk all
#   xlmake -f static-pattern.mk target-beta       # one target of the list
#   xlmake -i -f static-pattern.mk --recipe target-beta   # inspect the expanded recipe

NAMES := alpha beta gamma

GOALS := $(addprefix target-,$(NAMES))
CLEANS := $(addsuffix -clean,$(GOALS))

# Static pattern rule with NO prerequisites: the stem is the only thing carried into the recipe.
$(GOALS): target-%:
	@echo "build   stem=$*  name=$@"

# The same targets with a suffix pattern — '%' need not be at the end.
$(CLEANS): target-%-clean:
	@echo "clean   stem=$*  name=$@"

# Static pattern rule WITH prerequisite patterns: each '%' below becomes that target's own stem,
# while a prerequisite without '%' (here `src/shared.h`) is shared by all of them.
OBJS := $(addprefix obj/,$(addsuffix .o,$(NAMES)))

$(OBJS): obj/%.o: src/%.c src/shared.h
	@echo "compile $@  stem=$*  first=$<  all=[$^]"

# A glob may sit in any path component, not just the last one.
DIRS := $(wildcard src/*/)
MKS := $(wildcard */*.mk)

.PHONY: all show $(GOALS) $(CLEANS)

all: show $(GOALS) $(CLEANS)
	@echo "static-pattern sample done"

show:
	@echo "wildcard src/*/  -> $(words $(DIRS)) entry(ies)"
	@echo "wildcard */*.mk  -> $(words $(MKS)) entry(ies)"
