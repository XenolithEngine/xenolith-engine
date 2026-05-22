#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDIO_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDIO_H_

#include <sprt/c/__sprt_stdarg.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/bits/__sprt_null.h>
#include <sprt/wrappers/libc/stddef.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef EOF
#define EOF __SPRT_EOF
#endif

#ifndef NULL
#define NULL __SPRT_NULL
#endif

#ifndef _IOFBF
#define _IOFBF __SPRT_IOFBF
#define _IOLBF __SPRT_IOLBF
#define _IONBF __SPRT_IONBF
#endif

#ifndef SEEK_SET
#define SEEK_SET __SPRT_SEEK_SET
#define SEEK_CUR __SPRT_SEEK_CUR
#define SEEK_END __SPRT_SEEK_END
#endif

#ifdef __SPRT_BUFSIZ
#define BUFSIZ __SPRT_BUFSIZ
#endif

#ifdef __SPRT_FILENAME_MAX
#define FILENAME_MAX __SPRT_FILENAME_MAX
#endif

#ifdef __SPRT_L_tmpnam
#define L_tmpnam __SPRT_L_tmpnam
#endif

#ifdef __SPRT_UNGET
#define UNGET __SPRT_UNGET
#endif

#ifndef _TRUNCATE
#define _TRUNCATE ((__SPRT_ID(size_t))-1)
#endif

#ifndef fopen64
#define fopen64 fopen
#define freopen64 freopen
#endif

#if __STDC_HOSTED__ == 1
#define stdin  __SPRT_ID(stdin_impl)()
#define stdout  __SPRT_ID(stdout_impl)()
#define stderr  __SPRT_ID(stderr_impl)()
#endif

typedef __SPRT_ID(FILE) FILE;

#endif


#if defined(__cplusplus) && __STDC_HOSTED__ == 1
namespace sprt {
inline namespace _cstdio {

#define SPRT_FUNC_BEGIN SPRT_FORCEINLINE
#define SPRT_FUNC_END SPRT_NOEXCEPT
#define SPRT_FUNC_BODY 1

#include <sprt/wrappers/libc/stdio_impl.h>

#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

} // namespace _cstdio
} // namespace sprt

// export functions globally
#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD)
using namespace sprt::_cstdio;
#endif

#ifdef __SPRT_AS_STD
namespace std {
using namespace sprt::_cstdio;
}
#endif
#endif // __cplusplus


#if __STDC_HOSTED__ == 0 || (!defined(__SPRT_BUILD) && !defined(__cplusplus))

__SPRT_BEGIN_DECL

#define SPRT_FUNC_BEGIN SPRT_UMBRELLA_FUNC
#define SPRT_FUNC_END SPRT_UMBRELLA_END
#define SPRT_FUNC_BODY SPRT_UMBRELLA_REQUIRED

#include <sprt/wrappers/libc/stdio_impl.h>

#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

__SPRT_END_DECL

#endif

#define _scanf_l(fmt, loc, ...) scanf_l(loc, fmt, __VA_ARGS__)
#define _fscanf_l(stream, fmt, loc, ...) fscanf_l(stream, loc, fmt, __VA_ARGS__)
#define _sscanf_l(str, fmt, loc, ...) sscanf_l(str, loc, fmt, __VA_ARGS__)
#define _vscanf_l(fmt, loc, ...) vscanf_l(loc, fmt, __VA_ARGS__)
#define _vfscanf_l(stream, fmt, loc, ...) vfscanf_l(stream, loc, fmt, __VA_ARGS__)
#define _vsscanf_l(str, fmt, loc, ...) vsscanf_l(str, loc, fmt, __VA_ARGS__)

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDIO_H_
