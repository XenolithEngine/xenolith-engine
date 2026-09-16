# Sample makefile for xlmake: triggers engine diagnostic warnings.
# Run plain to see the default warnings; add -W/--pedantic to also see the
# stricter (pedantic) ones. All values are `:=`, so the warnings fire at parse time.

SIMPLE := value

# WarnCallStaticVariable (default-on): $(call) of a simple (:=) variable
USE := $(call SIMPLE)

# WarnSortEmpty (pedantic, off by default): $(sort) of an undefined (empty) variable
SORTED := $(sort $(UNDEFINED))

# WarnFilterEmpty (pedantic, off by default): $(filter) with empty patterns
PICKED := $(filter ,a b c)
