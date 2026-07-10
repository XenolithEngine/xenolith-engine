// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Freestanding replacement for musl's src/internal/locale_impl.h, picked up
// ahead of it because musl-adapters/include is the first entry on the adapter
// include path (same mechanism as the bits/* overrides beside this file).
//
// musl's locale_impl.h pulls in "pthread_impl.h" to reach the thread-local
// locale (CURRENT_UTF8 / CURRENT_LOCALE), which transitively drags <pthread.h>,
// <time.h> and <signal.h> — none of which are set up for the freestanding
// wasm/Windows musl adapter (they collide with the SPRT <time.h> and want an
// arch <bits/signal.h> that does not exist for wasm32).
//
// The only consumers that reach this header from the adapter are the regex SCU's
// fnmatch.c (needs MB_CUR_MAX) and regerror.c (needs LCTRANS_CUR). This runtime
// is always UTF-8 and ships no gettext message catalog, so:
//   - MB_CUR_MAX is a constant 4 (fnmatch only tests `== 1`, i.e. "single-byte
//     locale?", so any value > 1 selects the full multibyte path via mbtowc);
//   - LCTRANS / LCTRANS_CUR translate nothing and return the message verbatim.
// Position matches musl's: fnmatch.c includes <stdlib.h> (which defines
// MB_CUR_MAX via __ctype_get_mb_cur_max()) BEFORE this header, so the
// #undef/#define below overrides it, exactly as the real locale_impl.h does.

#ifndef _LOCALE_IMPL_H
#define _LOCALE_IMPL_H

#define LCTRANS(msg, lc, loc) (msg)
#define LCTRANS_CUR(msg) (msg)

#undef MB_CUR_MAX
#define MB_CUR_MAX 4

#endif // _LOCALE_IMPL_H
