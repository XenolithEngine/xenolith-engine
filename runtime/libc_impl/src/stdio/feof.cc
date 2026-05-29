#include "../../include/__impl_file.h"

#undef feof

__SPRT_C_FUNC int feof(FILE *f) __SPRT_NOEXCEPT {
	FLOCK(f);
	int ret = !!(f->flags & F_EOF);
	FUNLOCK(f);
	return ret;
}

weak_alias(feof, feof_unlocked);
weak_alias(feof, _IO_feof_unlocked);
