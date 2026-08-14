/* Force-included only when building libc++abi/libunwind against Embox libc.
   Embox declares abort() without _Noreturn, so libunwind's
   add_compile_flags_if_supported(-Werror=return-type) fails on
   Registers.hpp after _LIBUNWIND_ABORT (which is abort() in a do-while). */
#ifdef __cplusplus
extern "C" {
#endif
_Noreturn void abort(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#ifndef PTHREAD_RWLOCK_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER {}
#endif
#endif

/* Embox default wchar.h omits several POSIX wide-string helpers libc++ wraps. */
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
wchar_t *wcschr(const wchar_t *, wchar_t);
wchar_t *wcsrchr(const wchar_t *, wchar_t);
wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
wchar_t *wcsstr(const wchar_t *, const wchar_t *);
#ifdef __cplusplus
}
#endif
