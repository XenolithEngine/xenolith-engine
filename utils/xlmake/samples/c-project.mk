# Sample makefile for xlmake: a small C project.
# Exercises variables, a pattern rule with automatic variables, and .PHONY.

CC     := cc
CFLAGS := -O2 -Wall
SRCDIR := src
OBJS   := foo.o bar.o baz.o

all: app

app: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean
clean:
	rm -f $(OBJS) app
