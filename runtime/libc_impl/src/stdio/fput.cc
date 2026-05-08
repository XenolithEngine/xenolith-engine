#include <stdio.h>
#include <string.h>
#include "../../include/__impl_file.h"

static int locking_putc(int c, FILE *f) {
	auto tok = f->__lock();
	c = __putc_unlocked(c, f);
	f->__unlock(tok);
	return c;
}

static inline int do_putc(int c, FILE *f) {
	if (f->__lock_pid < 0 || f->__lock_pid == __sprt_pthread_get_id_np()) {
		return __putc_unlocked(c, f);
	}
	return locking_putc(c, f);
}

weak_alias(putc_unlocked, fputc_unlocked);
weak_alias(putc_unlocked, _IO_putc_unlocked);

__SPRT_C_FUNC int putc_unlocked(int c, FILE *f) __SPRT_NOEXCEPT { return __putc_unlocked(c, f); }

__SPRT_C_FUNC int fputc(int c, FILE *f) __SPRT_NOEXCEPT { return do_putc(c, f); }

__SPRT_C_FUNC int fputs(const char *__restrict s, FILE *__restrict f) __SPRT_NOEXCEPT {
	size_t l = strlen(s);
	return (fwrite(s, 1, l, f) == l) - 1;
}

weak_alias(fputs, fputs_unlocked);
