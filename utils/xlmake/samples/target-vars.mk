# Sample makefile for xlmake: target-specific variables.
# `prog : CFLAGS = -g` overrides CFLAGS only while expanding prog's own recipe; `other`
# still sees the global value. `appnd` uses `+=` over the global. Inspect with e.g.
#   xlmake -f target-vars.mk --recipe prog     -> cc -g -o prog foo.o
#   xlmake -f target-vars.mk --recipe other    -> cc -O2 -o other foo.o
#   xlmake -f target-vars.mk --recipe appnd    -> echo -O2 -Wall
# (own-recipe scope only: a target's variable does not propagate to its prerequisites.)

CFLAGS := -O2

prog : CFLAGS = -g
prog : foo.o
	cc $(CFLAGS) -o $@ $^

other : foo.o
	cc $(CFLAGS) -o $@ $^

appnd : CFLAGS += -Wall
appnd : ; echo $(CFLAGS)

# multiple targets sharing one assignment, and a private (inert here) form
dbg1 dbg2 : MODE := debug
dbg1 : ; echo $(MODE)

priv : private LOCAL = secret
priv : ; echo $(LOCAL)
