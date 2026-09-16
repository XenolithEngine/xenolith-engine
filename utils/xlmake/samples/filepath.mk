# Sample makefile for xlmake: file-name functions (validated against GNU make 4.4).
# These are purely textual (last '/', last '.'); they do not touch the filesystem.
# Inspect with:  xlmake -f filepath.mk --print-vars
# Expected values (as shown by --print-vars) are noted in the comments.

DIR_SRC    := $(dir src/foo.c)              # src/
DIR_BARE   := $(dir foo.c)                  # ./    (no slash -> "./")
DIR_MULTI  := $(dir a/b/c x/y z)            # a/b/ x/ ./

NOTDIR     := $(notdir src/foo.c bar.h)     # foo.c bar.h

SUFFIX     := $(suffix a.c b.h dir/c)       # .c .h  (includes the dot; "dir/c" has none)
SUFFIX_MUL := $(suffix foo.tar.gz)          # .gz    (only the last suffix)

BASENAME   := $(basename a.c dir/b.cpp)     # a dir/b   (drops the suffix, with its dot)
BASE_MUL   := $(basename foo.tar.gz noext)  # foo.tar noext

ADDSUFFIX  := $(addsuffix .o,a b c)         # a.o b.o c.o
ADDPREFIX  := $(addprefix obj/,a b)         # obj/a obj/b
JOIN       := $(join a b c,1 2)             # a1 b2 c

# $(abspath) canonicalizes ('.', '..') without touching the filesystem:
ABSPATH    := $(abspath /a/b/../c)          # /a/c
# $(realpath) additionally resolves symlinks and is empty for a missing path:
REALPATH   := $(realpath /a/b/../nonexistent)  # (empty)
