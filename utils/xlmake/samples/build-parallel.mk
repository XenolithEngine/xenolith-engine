# Sample makefile for xlmake's build mode (`xlmake -b`).
#
# `foo.txt`, `bar.txt`, `baz.txt` are independent leaves (no shared prerequisites), so a parallel
# build runs their recipes concurrently; `app` joins them and must wait for all three. Compare
# against GNU make:
#
#   xlmake -b -j4 all     vs   make -j4 all      # builds the three leaves then app
#   xlmake -b all              make all          # parallel by default (all cores)
#   xlmake -b -j1 all          make -j1 all      # serial
#   xlmake -b -n all           make -n all       # dry run: print commands, run nothing
#   xlmake -b clean            make clean
#
# A second `xlmake -b all` is a no-op (everything up to date). Recipes redirect their output to
# files, so each target's printed block is just its echoed command line(s).

all : app

app : foo.txt bar.txt baz.txt
	cat foo.txt bar.txt baz.txt > app

foo.txt :
	echo foo > foo.txt

bar.txt :
	echo bar > bar.txt

baz.txt :
	echo baz > baz.txt

clean :
	rm -f foo.txt bar.txt baz.txt app

.PHONY: all clean
