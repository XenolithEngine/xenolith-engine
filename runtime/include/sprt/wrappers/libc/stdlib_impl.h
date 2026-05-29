SPRT_FUNC_BEGIN
int atoi(const char *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_atoi(value);
}
#endif

SPRT_FUNC_BEGIN
long atol(const char *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_atol(value);
}
#endif

SPRT_FUNC_BEGIN
long long atoll(const char *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_atoll(value);
}
#endif

SPRT_FUNC_BEGIN
double atof(const char *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_atof(value);
}
#endif

SPRT_FUNC_BEGIN
float strtof(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtof(str, value);
}
#endif

SPRT_FUNC_BEGIN
double strtod(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtod(str, value);
}
#endif

SPRT_FUNC_BEGIN
long double strtold(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtold(str, value);
}
#endif

SPRT_FUNC_BEGIN
long strtol(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtol(str, endp, value);
}
#endif

SPRT_FUNC_BEGIN
unsigned long strtoul(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtoul(str, endp, value);
}
#endif

SPRT_FUNC_BEGIN
long long strtoll(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtoll(str, endp, value);
}
#endif

SPRT_FUNC_BEGIN
unsigned long long strtoull(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtoull(str, endp, value);
}
#endif

SPRT_FUNC_BEGIN
int rand(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_rand();
}
#endif

SPRT_FUNC_BEGIN
void srand(unsigned value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_srand(value);
}
#endif

SPRT_FUNC_BEGIN
void *malloc(size_t value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_malloc(value);
}
#endif

SPRT_FUNC_BEGIN
void *calloc(size_t a, size_t value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_calloc(a, value);
}
#endif

SPRT_FUNC_BEGIN
void *realloc(void *ptr, size_t value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_realloc(ptr, value);
}
#endif

SPRT_FUNC_BEGIN
void free(void *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_free(value);
}
#endif

SPRT_FUNC_BEGIN
void free_sized(void *value, size_t size) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_free_sized(value, size);
}
#endif

SPRT_FUNC_BEGIN
void free_aligned_sized(void *value, size_t alignment, size_t size) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_free_aligned_sized(value, alignment, size);
}
#endif

/*
	WARNING: use aligned_free to safely free aligned memory.
	It's MSVC requirement, but safe to follow this rule everywhere
*/
SPRT_FUNC_BEGIN
void *aligned_alloc(size_t a, size_t value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_aligned_alloc(a, value);
}
#endif

SPRT_FUNC_BEGIN
void aligned_free(void *memblock) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_aligned_free(memblock);
}
#endif

SPRT_FUNC_BEGIN
__SPRT_NORETURN void abort(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_abort();
}
#endif

SPRT_FUNC_BEGIN
int atexit(void (*value)(void)) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_atexit(value);
}
#endif

SPRT_FUNC_BEGIN
__SPRT_NORETURN void exit(int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_exit(value);
}
#endif

SPRT_FUNC_BEGIN
__SPRT_NORETURN void _Exit(int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt__Exit(value);
}
#endif

SPRT_FUNC_BEGIN
int at_quick_exit(void (*value)(void)) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_at_quick_exit(value);
}
#endif

SPRT_FUNC_BEGIN
__SPRT_NORETURN void quick_exit(int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_quick_exit(value);
}
#endif

SPRT_FUNC_BEGIN
char *getenv(const char *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getenv(value);
}
#endif

SPRT_FUNC_BEGIN
int system(const char *value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_system(value);
}
#endif

SPRT_FUNC_BEGIN
void *bsearch(const void *a, const void *b, size_t c, size_t d,
		int (*value)(const void *, const void *)) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_bsearch(a, b, c, d, value);
}
#endif

SPRT_FUNC_BEGIN
void qsort(void *a, size_t b, size_t c, int (*value)(const void *, const void *)) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_qsort(a, b, c, value);
}
#endif

SPRT_FUNC_BEGIN
void qsort_r(void *a, size_t b, size_t c, int (*cmp)(const void *, const void *, void *),
		void *ctx) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_qsort_r(a, b, c, cmp, ctx);
}
#endif

#if !defined(__cplusplus) || __STDC_HOSTED__ == 0
SPRT_FUNC_BEGIN
int abs(int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_abs(value);
}
#endif

SPRT_FUNC_BEGIN
long labs(long value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_labs(value);
}
#endif

SPRT_FUNC_BEGIN
long long llabs(long long value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_llabs(value);
}
#endif
#endif // __cplusplus

SPRT_FUNC_BEGIN
div_t div(int a, int value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_div(a, value);
}
#endif

SPRT_FUNC_BEGIN
ldiv_t ldiv(long a, long value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ldiv(a, value);
}
#endif

SPRT_FUNC_BEGIN
lldiv_t lldiv(long long a, long long value) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_lldiv(a, value);
}
#endif

SPRT_FUNC_BEGIN
int setenv(const char *n, const char *v, int r) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_setenv(n, v, r);
}
#endif

SPRT_FUNC_BEGIN
int unsetenv(const char *n) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_unsetenv(n);
}
#endif

SPRT_FUNC_BEGIN
int mkstemp(char *tpl) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_mkstemp(tpl);
}
#endif

SPRT_FUNC_BEGIN
int mkostemp(char *tpl, int n) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_mkostemp(tpl, n);
}
#endif

SPRT_FUNC_BEGIN
char *mkdtemp(char *tpl) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_mkdtemp(tpl);
}
#endif

SPRT_FUNC_BEGIN
int getsubopt(char **opts, char *const *toks, char **vals) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getsubopt(opts, toks, vals);
}
#endif

SPRT_FUNC_BEGIN
int rand_r(unsigned *v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_rand_r(v);
}
#endif

SPRT_FUNC_BEGIN
char *realpath(const char *__SPRT_RESTRICT path, char *__SPRT_RESTRICT out) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_realpath(path, out);
}
#endif

SPRT_FUNC_BEGIN
long int random(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_random();
}
#endif

SPRT_FUNC_BEGIN
void srandom(unsigned int seed) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_srandom(seed);
}
#endif

SPRT_FUNC_BEGIN
long strtol_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int base,
		__SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtol_l(str, endp, base, loc);
}
#endif

SPRT_FUNC_BEGIN
long long strtoll_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int base,
		__SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtoll_l(str, endp, base, loc);
}
#endif

SPRT_FUNC_BEGIN
unsigned long strtoul_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int base,
		__SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtoul_l(str, endp, base, loc);
}
#endif

SPRT_FUNC_BEGIN
unsigned long long strtoull_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int base, __SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtoull_l(str, endp, base, loc);
}
#endif

SPRT_FUNC_BEGIN
float strtof_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtof_l(str, endp, loc);
}
#endif

SPRT_FUNC_BEGIN
double strtod_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtod_l(str, endp, loc);
}
#endif

SPRT_FUNC_BEGIN
long double strtold_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_strtold_l(str, endp, loc);
}
#endif

SPRT_FUNC_BEGIN
size_t mbstowcs(wchar_t *__dst, const char *__src, size_t __n) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __SPRT_ID(mbstowcs)(__dst, __src, __n);
}
#endif

SPRT_FUNC_BEGIN
int mbtowc(wchar_t *__wc_ptr, const char *__s, size_t __n) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __SPRT_ID(mbtowc)(__wc_ptr, __s, __n);
}
#endif

SPRT_FUNC_BEGIN
int wctomb(char *__dst, wchar_t __wc) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __SPRT_ID(wctomb)(__dst, __wc);
}
#endif

SPRT_FUNC_BEGIN
size_t wcstombs(char *__dst, const wchar_t *__src, size_t __n) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __SPRT_ID(wcstombs)(__dst, __src, __n);
}
#endif
