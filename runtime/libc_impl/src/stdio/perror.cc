#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "../../include/__impl_file.h"

__SPRT_C_FUNC void perror(const char *msg) __SPRT_NOEXCEPT {
	FILE *f = stderr;
	char *errstr = strerror(errno);

	FLOCK(f);

	/* Save stderr's orientation and encoding rule, since perror is not
	 * permitted to change them. */
	auto old_locale = f->locale;
	int old_mode = f->mode;

	if (msg && *msg) {
		fwrite(msg, sprt::strlen(msg), 1, f);
		fputc(':', f);
		fputc(' ', f);
	}
	fwrite(errstr, sprt::strlen(errstr), 1, f);
	fputc('\n', f);

	f->mode = old_mode;
	f->locale = old_locale;

	FUNLOCK(f);
}
