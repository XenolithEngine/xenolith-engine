// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// cross/macos_sprt/{locale,langinfo,ctype}.h <-> Darwin parity.
//
// setlocale()/newlocale() categories and the nl_langinfo() item numbers are
// passed to libSystem directly. The ctype table bits matter because sprt's
// is*() macros index Darwin's _DefaultRuneLocale array with them.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <locale.h>
#include <xlocale.h>
#include <langinfo.h>
#include <ctype.h>
#include <runetype.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_locale.h>
#define SPRT_ABI_HEADER_2 <sprt/c/cross/__sprt_langinfo.h>
// The ctype table has no cross/__sprt_ctype.h umbrella of its own: sprt's
// <sprt/c/__sprt_ctype.h> defines sprt's *internal* classification bits
// (__SPRT_CTYPE_ALNUM, ...), while macos_sprt/ctype.h mirrors Darwin's
// _CTYPE_* runetype bits and is reached only through ios_sprt/ctype.h. It is
// still a Darwin ABI mirror -- sprt's is*() feed these bits to
// _DefaultRuneLocale -- so it is pinned here, included by its platform path.
#define SPRT_ABI_HEADER_3 <sprt/c/cross/macos_sprt/ctype.h>
#include "abi_check.h"

// === setlocale() categories ================================================
SPRT_CONST(LC_ALL);
SPRT_CONST(LC_COLLATE);
SPRT_CONST(LC_CTYPE);
SPRT_CONST(LC_MONETARY);
SPRT_CONST(LC_NUMERIC);
SPRT_CONST(LC_TIME);
SPRT_CONST(LC_MESSAGES);

// === newlocale() category masks ============================================
SPRT_CONST(LC_COLLATE_MASK);
SPRT_CONST(LC_CTYPE_MASK);
SPRT_CONST(LC_MESSAGES_MASK);
SPRT_CONST(LC_MONETARY_MASK);
SPRT_CONST(LC_NUMERIC_MASK);
SPRT_CONST(LC_TIME_MASK);
SPRT_CONST(LC_ALL_MASK);

// === nl_langinfo() items ===================================================
SPRT_CONST(CODESET);
SPRT_CONST(D_T_FMT);
SPRT_CONST(D_FMT);
SPRT_CONST(T_FMT);
SPRT_CONST(T_FMT_AMPM);
SPRT_CONST(AM_STR);
SPRT_CONST(PM_STR);
SPRT_CONST(DAY_1);
SPRT_CONST(ABDAY_1);
SPRT_CONST(MON_1);
SPRT_CONST(ABMON_1);
SPRT_CONST(RADIXCHAR);
SPRT_CONST(THOUSEP);
SPRT_CONST(YESEXPR);
SPRT_CONST(NOEXPR);
SPRT_CONST(CRNCYSTR);

// === Darwin runetype bits (Darwin spells them _CTYPE_*) ====================
SPRT_CONST_MAP(CTYPE_A, _CTYPE_A);
SPRT_CONST_MAP(CTYPE_C, _CTYPE_C);
SPRT_CONST_MAP(CTYPE_D, _CTYPE_D);
SPRT_CONST_MAP(CTYPE_G, _CTYPE_G);
SPRT_CONST_MAP(CTYPE_L, _CTYPE_L);
SPRT_CONST_MAP(CTYPE_P, _CTYPE_P);
SPRT_CONST_MAP(CTYPE_S, _CTYPE_S);
SPRT_CONST_MAP(CTYPE_U, _CTYPE_U);
SPRT_CONST_MAP(CTYPE_X, _CTYPE_X);
SPRT_CONST_MAP(CTYPE_B, _CTYPE_B);
SPRT_CONST_MAP(CTYPE_R, _CTYPE_R);
SPRT_CONST_MAP(CTYPE_I, _CTYPE_I);
SPRT_CONST_MAP(CTYPE_T, _CTYPE_T);
SPRT_CONST_MAP(CTYPE_Q, _CTYPE_Q);
SPRT_CONST_MAP(CTYPE_SW0, _CTYPE_SW0);
SPRT_CONST_MAP(CTYPE_SW1, _CTYPE_SW1);
SPRT_CONST_MAP(CTYPE_SW2, _CTYPE_SW2);
SPRT_CONST_MAP(CTYPE_SW3, _CTYPE_SW3);
SPRT_CONST_MAP(CTYPE_SWM, _CTYPE_SWM);
SPRT_CONST_MAP(CTYPE_SWS, _CTYPE_SWS);
