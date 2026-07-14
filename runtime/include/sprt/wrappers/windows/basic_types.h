/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_
#define SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_

#include <sprt/wrappers/windows/abi/basic_types.h>

/* Clean public names (materialized __SPRT_ values live in abi/basic_types.h) */
#define FIELD_OFFSET(type, field) __SPRT_FIELD_OFFSET(type, field)
#define UFIELD_OFFSET(type, field) __SPRT_UFIELD_OFFSET(type, field)
#define MAKEWORD(a, b) __SPRT_MAKEWORD(a, b)
#define MAKELONG(a, b) __SPRT_MAKELONG(a, b)
#define LOWORD(l) __SPRT_LOWORD(l)
#define HIWORD(l) __SPRT_HIWORD(l)
#define LOBYTE(w) __SPRT_LOBYTE(w)
#define HIBYTE(w) __SPRT_HIBYTE(w)

// Some developers expect that wchar.h functions will be defined when windows headers are
// included; kept at the api level so abi/ stays free of include_libc dependencies.
// When we build SPRT itself, it should not be defined.
#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD)
#include <wchar.h>
#endif

#endif // SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_
