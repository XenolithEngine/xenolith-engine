# Sample makefile for xlmake: command-line variable assignment.
# Command-line assignments override makefile assignments, but an `override` directive wins.
#
#   xlmake -f cmdline.mk -V CC -V GREETING            # defaults
#   xlmake -f cmdline.mk CC=gcc -V CC -V GREETING     # CC overridden -> GREETING follows
#   xlmake -f cmdline.mk EXTRA=-g -V GREETING         # set an otherwise-empty var
#   xlmake -f cmdline.mk LOCKED=fromcli -V LOCKED     # 'override' keeps the makefile value

CC      := cc
CFLAGS  := -O2
EXTRA   :=

# recursive (=) so it reflects the value of CC at expansion time
GREETING = built with $(CC) $(CFLAGS) $(EXTRA)

LOCKED := makefile-value
override LOCKED := makefile-wins
