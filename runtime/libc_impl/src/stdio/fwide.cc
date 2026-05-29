#include <wchar.h>
#include <stdlib.h>
#include "../../include/__impl_file.h"
#include "../../include/__impl_libc.h"

__SPRT_C_FUNC int fwide(FILE *f, int mode) __SPRT_NOEXCEPT {
	FLOCK(f);
	if (mode) {
		if (!f->locale) {
			f->locale = sprt::__get_default_locale_struct();
		}
		if (!f->mode) {
			f->mode = mode > 0 ? 1 : -1;
		}
	}
	mode = f->mode;
	FUNLOCK(f);
	return mode;
}
