#include "../../include/__impl_file.h"

#undef ferror

__SPRT_C_FUNC int ferror(FILE *f) __SPRT_NOEXCEPT {
	FLOCK(f);
	int ret = !!(f->flags & F_ERR);
	FUNLOCK(f);
	return ret;
}

weak_alias(ferror, ferror_unlocked);
weak_alias(ferror, _IO_ferror_unlocked);
