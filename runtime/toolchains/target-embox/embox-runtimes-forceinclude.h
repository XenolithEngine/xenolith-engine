/* Force-included when building libc++abi/libunwind against Embox libc.

   Everything below patches up the BARE Embox libc, and only libunwind still
   compiles against it: libcxxabi is built against the engine's own libc++ port
   (see libcxx.mk), where abort() is already _Noreturn and the wide-string
   helpers are declared with the C++ const-correct overloads — re-declaring them
   here ahead of the port's own headers is a hard error, not a fix.

   The port on the include path is the discriminator: it is passed to libcxxabi's
   TUs (LIBCXXABI_ADDITIONAL_COMPILE_FLAGS) and to nothing else, so __has_include
   answers "am I compiling libcxxabi?" at the one moment this header runs. */
#if !__has_include(<sprt/c/bits/__sprt_def.h>)

/* Embox declares abort() without _Noreturn, so libunwind's
   add_compile_flags_if_supported(-Werror=return-type) fails on
   Registers.hpp after _LIBUNWIND_ABORT (which is abort() in a do-while). */
#ifdef __cplusplus
extern "C" {
#endif
_Noreturn void abort(void);
#ifdef __cplusplus
}
#endif

/* libunwind's RWMutex.hpp initialises a pthread_rwlock_t with it. */
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

#endif /* !__has_include(<sprt/c/bits/__sprt_def.h>) */
