# Sample makefile for xlmake: recipe-less "aggregator" targets.
#
# A target that has prerequisites but NO recipe of its own is valid in make: building it just
# builds its prerequisites. It is not an error, and it does NOT have to be .PHONY. Only a name that
# was never declared as a rule's target — and has no matching file — is "No rule to make target".
#
#   xlmake -f aggregator.mk all      vs   make -f aggregator.mk all     # builds stage1 + stage2
#   xlmake -f aggregator.mk world          make -f aggregator.mk world  # 'world' -> 'all' -> stages
#   xlmake -i -f aggregator.mk --prerequisites -r world                 # the whole graph
#
# 'all' and 'world' carry no recipe; the stage targets do the actual work.
all: stage1 stage2

# 'world' is a recipe-less aggregator of another recipe-less aggregator — still fine.
world: all

stage1:
	@echo "building stage1"

stage2:
	@echo "building stage2"

# Counter-example (left commented so this makefile still builds cleanly): a prerequisite that is
# never declared and has no file on disk is a genuine error. Uncommenting the line below makes
# `xlmake -f aggregator.mk all` print:  *** No rule to make target 'ghost'
#
# all: ghost
