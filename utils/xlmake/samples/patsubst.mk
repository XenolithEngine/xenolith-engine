# Sample makefile for xlmake: $(patsubst) behavior (validated against GNU make 4.4).
# Inspect with:  xlmake -f patsubst.mk --print-vars
# Expected values (as shown by --print-vars; ':=' strips leading/trailing whitespace) are
# noted in the comments. EMPTYTEXT emits a *pedantic* warning -- run with -W to see it.

# basic suffix transform: a.o b.o d.h (d.h has no match, kept)
SUFFIX    := $(patsubst %.c,%.o,a.c b.c d.h)

# '%' matches the empty stem too: .o
ZEROSTEM  := $(patsubst %.c,%.o,.c)

# prefix + suffix pattern: obj/a.o x.c
PREFIXED  := $(patsubst src/%.c,obj/%.o,src/a.c x.c)

# literal pattern (no '%'): bar foox
LITERAL   := $(patsubst foo,bar,foo foox)

# literal pattern => '%' in the replacement stays literal: x%y
LIT_PCT   := $(patsubst foo,x%y,foo)

# '%'-pattern but literal replacement => stem is NOT appended: out b.h
LIT_REPL  := $(patsubst %.c,out,a.c b.h)

# only the first '%' of the replacement is the stem; the rest is literal: a.o%
REPL_PCT  := $(patsubst %.c,%.o%,a.c)

# empty replacement with a '%'-pattern drops matched words (no separators): b.h
DROP_PCT  := $(patsubst %.c,,a.c b.h c.c)

# a backslash escapes '%' so it matches literally: X a
ESCAPED   := $(patsubst \%,X,% a)

# empty text => empty result; pedantic warning ("patsubst called with empty text")
EMPTYTEXT := $(patsubst %.o,%.c,)
