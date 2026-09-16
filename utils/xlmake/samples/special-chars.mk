# Sample makefile for xlmake: special characters in target and variable names.
#
# A make target or variable name may contain '+', '?', ',', '(' and ')' literally — GNU make
# accepts "libc++", "a?b", "obj/a,b.o" as ordinary names. xlmake's line-start tokenizer keeps each
# as a single word (it does NOT split "libc++" into "libc+" + "+"), and still recognizes the real
# "+=" / "?=" operators when they actually appear.
#
#   xlmake -f special-chars.mk all           vs   make -f special-chars.mk all
#   xlmake -f special-chars.mk libc++              make -f special-chars.mk libc++
#   xlmake -i -f special-chars.mk -V FLAGS         # FLAGS == "-O2 -Wall"  (+= still appends)
#   xlmake -i -f special-chars.mk -V FLAGS+        # FLAGS+ == "plus more" ('+' is also legal IN a name)

# Each prerequisite below is ONE target name, kept intact across the special character:
.PHONY: all libc++ a?b a,b a(b)c
all: libc++ a?b a,b a(b)c
	@echo "all special-char targets built"

libc++:
	@echo "built [$@]  (the trailing '++' is part of the name)"

a?b:
	@echo "built [$@]  ('?' here is literal, not the '?=' operator)"

a,b:
	@echo "built [$@]  (',' is literal, not a function-argument separator)"

a(b)c:
	@echo "built [$@]  ('(' and ')' are literal name characters)"

# '+' / '?' still form the append / conditional-assign operators when followed by '=':
FLAGS  := -O2
FLAGS  += -Wall          # append    -> "-O2 -Wall"
FLAGS  ?= ignored        # already set -> no-op

# ...and a '+' is equally legal *inside* a variable name (here a trailing '+'):
FLAGS+ := plus
FLAGS+ += more           # "plus more"
