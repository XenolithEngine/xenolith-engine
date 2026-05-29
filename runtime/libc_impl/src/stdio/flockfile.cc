#include "../../include/__impl_file.h"

__SPRT_C_FUNC void flockfile(FILE *f) __SPRT_NOEXCEPT { sprt_plock_lock(f, 0, &f->__lock_pid); }

__SPRT_C_FUNC int ftrylockfile(FILE *f) __SPRT_NOEXCEPT {
	return sprt_plock_lock(f, 0, &f->__lock_pid);
}

__SPRT_C_FUNC void funlockfile(FILE *f) __SPRT_NOEXCEPT { sprt_plock_unlock(f, 0, &f->__lock_pid); }

__SPRT_C_FUNC void clearerr(FILE *f) __SPRT_NOEXCEPT {
	FLOCK(f);
	f->flags &= ~(F_EOF | F_ERR);
	FUNLOCK(f);
}

weak_alias(clearerr, clearerr_unlocked);
