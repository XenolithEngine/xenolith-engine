#include "../../include/__impl_file.h"
#include <stdlib.h>

__SPRT_C_FUNC int fclose(FILE *f) __SPRT_NOEXCEPT {
	int r;

	if (sprt_plock_lock(f, 0, nullptr) == -1) {
		return -1;
	}

	r = fflush(f);
	r |= f->close(f);

	sprt_plock_unlock(f, SPRT_LOCK_FLAG_RESOURCE_DIED, nullptr);

	/* Past this point, f is closed and any further explict access
	 * to it is undefined. However, it still exists as an entry in
	 * the open file list and possibly in the thread's locked files
	 * list, if it was closed while explicitly locked. Functions
	 * which process these lists must tolerate dead FILE objects
	 * (which necessarily have inactive buffer pointers) without
	 * producing any side effects. */

	if (f->flags & F_PERM) {
		return r;
	}

	FILE **head = __ofl_lock();
	if (f->prev) {
		f->prev->next = f->next;
	}
	if (f->next) {
		f->next->prev = f->prev;
	}
	if (*head == f) {
		*head = f->next;
	}
	__ofl_unlock();

	free(f->getln_buf);
	free(f);

	return r;
}
