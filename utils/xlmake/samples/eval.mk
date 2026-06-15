# Sample makefile for xlmake: $(eval) of a multi-line define block.
# The `hello` rule is created at parse time from the template below.
# Inspect with:  xlmake -f eval.mk --recipe hello   (or --prerequisites world)

define RULE
hello:
	@echo hi from eval
endef

$(eval $(RULE))

world: hello
	@echo world depends on hello
