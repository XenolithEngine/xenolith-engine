#ifndef FLOATSCAN_H
#define FLOATSCAN_H

#include <stdio.h>

// The trailing `radix` is the decimal-point character to recognise (the
// LC_NUMERIC radix; '.' for the C locale).
long double __floatscan(FILE *, int, int, int radix);

#endif
