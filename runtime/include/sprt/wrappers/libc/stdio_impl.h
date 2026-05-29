SPRT_FUNC_BEGIN
FILE *fopen(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fopen(path, mode);
}
#endif

SPRT_FUNC_BEGIN
FILE *freopen(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode,
		FILE *__SPRT_RESTRICT file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_freopen(path, mode, file);
}
#endif

SPRT_FUNC_BEGIN
int fclose(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fclose(file);
}
#endif

SPRT_FUNC_BEGIN
int remove(const char *path) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_remove(path);
}
#endif

SPRT_FUNC_BEGIN
int rename(const char *oldPath, const char *newPath) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_rename(oldPath, newPath);
}
#endif

SPRT_FUNC_BEGIN
int feof(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_feof(file);
}
#endif

SPRT_FUNC_BEGIN
int ferror(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ferror(file);
}
#endif

SPRT_FUNC_BEGIN
int fflush(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fflush(file);
}
#endif

SPRT_FUNC_BEGIN
void clearerr(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_clearerr(file);
}
#endif

SPRT_FUNC_BEGIN
int fseek(FILE *file, __SPRT_ID(off_t) pos, int when) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fseek(file, pos, when);
}
#endif

SPRT_FUNC_BEGIN
__SPRT_ID(off_t)
ftell(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ftell(file);
}
#endif

SPRT_FUNC_BEGIN
void rewind(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_rewind(file);
}
#endif

SPRT_FUNC_BEGIN
size_t fread(void *__SPRT_RESTRICT buf, size_t n, size_t count,
		FILE *__SPRT_RESTRICT file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fread(buf, n, count, file);
}
#endif

SPRT_FUNC_BEGIN
size_t fwrite(const void *__SPRT_RESTRICT buf, size_t n, size_t count,
		FILE *__SPRT_RESTRICT file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fwrite(buf, n, count, file);
}
#endif

SPRT_FUNC_BEGIN
int fgetc(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fgetc(file);
}
#endif

SPRT_FUNC_BEGIN
int getc(FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getc(file);
}
#endif

SPRT_FUNC_BEGIN
int getchar(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getchar();
}
#endif

SPRT_FUNC_BEGIN
int ungetc(int c, FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ungetc(c, file);
}
#endif

SPRT_FUNC_BEGIN
int fputc(int c, FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fputc(c, file);
}
#endif

SPRT_FUNC_BEGIN
int putc(int c, FILE *file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_putc(c, file);
}
#endif

SPRT_FUNC_BEGIN
int putchar(int c) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_putchar(c);
}
#endif

SPRT_FUNC_BEGIN
char *fgets(char *__SPRT_RESTRICT buf, int n, FILE *__SPRT_RESTRICT file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fgets(buf, n, file);
}
#endif

SPRT_FUNC_BEGIN
int fputs(const char *__SPRT_RESTRICT buf, FILE *__SPRT_RESTRICT file) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fputs(buf, file);
}
#endif

SPRT_FUNC_BEGIN
int puts(const char *str) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_puts(str);
}
#endif

SPRT_FUNC_BEGIN
int printf(const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int fprintf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vfprintf(file, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int sprintf(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsprintf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int snprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsnprintf(buf, n, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif


SPRT_FUNC_BEGIN
int vprintf(const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vprintf(fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
int vfprintf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vfprintf(file, fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
int vsprintf(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vsprintf(buf, fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
int vsnprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vsnprintf(buf, n, fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
int scanf(const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int fscanf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vfscanf(file, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int sscanf(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int sscanf_s(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int vscanf(const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vscanf(fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
int vfscanf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vfscanf(file, fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
int vsscanf(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vsscanf(buf, fmt, arg);
}
#endif

SPRT_FUNC_BEGIN
void perror(const char *err) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_perror(err);
}
#endif

SPRT_FUNC_BEGIN
int setvbuf(FILE *__SPRT_RESTRICT file, char *__SPRT_RESTRICT buf, int mode,
		size_t size) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_setvbuf(file, buf, mode, size);
}
#endif

SPRT_FUNC_BEGIN
void setbuf(FILE *__SPRT_RESTRICT file, char *__SPRT_RESTRICT buf) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_setbuf(file, buf);
}
#endif

SPRT_FUNC_BEGIN
char *tmpnam(char *buf) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_tmpnam(buf);
}
#endif

SPRT_FUNC_BEGIN
FILE *tmpfile(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_tmpfile();
}
#endif

SPRT_FUNC_BEGIN
int asprintf(char **out, const char *fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vasprintf(out, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int vasprintf(char **out, const char *fmt, __sprt_va_list list) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vasprintf(out, fmt, list);
}
#endif

SPRT_FUNC_BEGIN
FILE *fmemopen(void *__SPRT_RESTRICT ptr, size_t size,
		const char *__SPRT_RESTRICT mode) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fmemopen(ptr, size, mode);
}
#endif

#if __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FUNC_BEGIN
FILE *open_memstream(char **ptr, size_t *sz) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_open_memstream(ptr, sz);
}
#endif
#endif

SPRT_FUNC_BEGIN
FILE *fdopen(int fd, const char *mode) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fdopen(fd, mode);
}
#endif
SPRT_FUNC_BEGIN
FILE *popen(const char *str, const char *mode) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_popen(str, mode);
}
#endif
SPRT_FUNC_BEGIN
int pclose(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_pclose(f);
}
#endif
SPRT_FUNC_BEGIN
int fileno(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fileno(f);
}
#endif
SPRT_FUNC_BEGIN
int fseeko(FILE *f, off_t off, int n) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fseeko(f, off, n);
}
#endif
SPRT_FUNC_BEGIN
off_t ftello(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ftello(f);
}
#endif
SPRT_FUNC_BEGIN
int dprintf(int n, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vdprintf(n, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif
SPRT_FUNC_BEGIN
int vdprintf(int n, const char *__SPRT_RESTRICT fmt, __sprt_va_list list) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vdprintf(n, fmt, list);
}
#endif
SPRT_FUNC_BEGIN
void flockfile(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_flockfile(f);
}
#endif
SPRT_FUNC_BEGIN
int ftrylockfile(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ftrylockfile(f);
}
#endif
SPRT_FUNC_BEGIN
void funlockfile(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_funlockfile(f);
}
#endif
SPRT_FUNC_BEGIN
int getc_unlocked(FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getc_unlocked(f);
}
#endif
SPRT_FUNC_BEGIN
int getchar_unlocked(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getchar_unlocked();
}
#endif
SPRT_FUNC_BEGIN
int putc_unlocked(int c, FILE *f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_putc_unlocked(c, f);
}
#endif
SPRT_FUNC_BEGIN
int putchar_unlocked(int c) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_putchar_unlocked(c);
}
#endif
SPRT_FUNC_BEGIN
ssize_t getdelim(char **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz, int c,
		FILE *__SPRT_RESTRICT f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getdelim(ret, sz, c, f);
}
#endif
SPRT_FUNC_BEGIN
ssize_t getline(char **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz,
		FILE *__SPRT_RESTRICT f) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_getline(ret, sz, f);
}
#endif
SPRT_FUNC_BEGIN
int renameat(int oldfd, const char *oldPath, int newfd, const char *newPath) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_renameat(oldfd, oldPath, newfd, newPath);
}
#endif
SPRT_FUNC_BEGIN
char *ctermid(char *s) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_ctermid(s);
}
#endif


SPRT_FUNC_BEGIN
int scanf_l(__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vscanf_l(loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif


SPRT_FUNC_BEGIN
int fscanf_l(FILE *__SPRT_RESTRICT stream, __SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT fmt,
		...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_fscanf_l(stream, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int sscanf_l(const char *__SPRT_RESTRICT buf, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsscanf_l(buf, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int vscanf_l(__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT format,
		__SPRT_ID(va_list) ap) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vscanf_l(loc, format, ap);
}
#endif

SPRT_FUNC_BEGIN
int vfscanf_l(FILE *__SPRT_RESTRICT stream, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT format, __SPRT_ID(va_list) ap) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vfscanf_l(stream, loc, format, ap);
}
#endif

SPRT_FUNC_BEGIN
int vsscanf_l(const char *__SPRT_RESTRICT str, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT format, __SPRT_ID(va_list) ap) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vsscanf_l(str, loc, format, ap);
}
#endif

SPRT_FUNC_BEGIN
int snprintf_l(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) len,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsnprintf_l(buf, len, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int vsnprintf_l(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) len,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt,
		__SPRT_ID(va_list) list) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vsnprintf_l(buf, len, loc, fmt, list);
}
#endif

SPRT_FUNC_BEGIN
int asprintf_l(char **__SPRT_RESTRICT target, __SPRT_ID(locale_t) __SPRT_RESTRICT loc,
		const char *__SPRT_RESTRICT fmt, ...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vasprintf_l(target, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_FUNC_BEGIN
int vasprintf_l(char **__SPRT_RESTRICT target, __SPRT_ID(locale_t) __SPRT_RESTRICT loc,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) list) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_vasprintf_l(target, loc, fmt, list);
}
#endif

SPRT_FUNC_BEGIN int _snprintf_s(char *buf, size_t bufSize, size_t maxChars, const char *fmt,
		...) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_snprintf(buf, bufSize, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif
