# Sample makefile for xlmake: a multi-level dependency graph.
# Demonstrates `--prerequisites` (immediate) vs `--prerequisites --recursive`
# (the transitive closure in dependency-graph order: dependencies precede
# dependents). `builddir` is an order-only prerequisite (after the `|`); `all`
# is a phony grouping target.
#
#   xlmake -f depgraph.mk --prerequisites app
#       app: lib.a main.o
#           | order-only: builddir
#
#   xlmake -f depgraph.mk --prerequisites --recursive app
#       app: a.c common.h a.o b.c b.o lib.a main.c main.o builddir
#
#   xlmake -f depgraph.mk --prerequisites --recursive -q app
#       the out-of-date set: status cascades along normal edges (a rebuilt
#       a.o/b.o forces lib.a); order-only prerequisites (builddir) are excluded.
#       E.g. a newer common.h yields:  app: a.o b.o lib.a main.o
#
#   xlmake -f depgraph.mk --prerequisites --recursive -q all
#       by default `all` (phony) is reported as out of date. Add --phony-prereqs
#       (-P) to judge it by its prerequisites instead: when `app` and everything
#       under it is fresh, `all` does not appear; when something below is stale,
#       `all` appears alongside it.

.PHONY: all
all: app

app: lib.a main.o | builddir
	$(LD) -o $@ $^

lib.a: a.o b.o
	$(AR) rcs $@ $^

main.o: main.c common.h
a.o: a.c common.h
b.o: b.c common.h

%.o: %.c
	$(CC) -c -o $@ $<

builddir:
	mkdir -p builddir
