# Sample makefile for xlmake: expansion-function coverage.
# Inspect with:  xlmake -f functions.mk --print-vars   (or -V NAME)

WORDS    := alpha beta gamma delta
SECOND   := $(word 2,$(WORDS))
COUNT    := $(words $(WORDS))
OBJECTS  := $(patsubst %.c,%.o,a.c b.c c.c)
JOINED   := $(join a b c,1 2 3)
DIRS     := $(dir src/foo.c include/bar.h)
SOURCES  := $(filter %.c,a.c b.h c.c d.o)
WRAPPED  := $(foreach n,$(WORDS),[$(n)])
