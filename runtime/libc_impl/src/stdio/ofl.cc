#include "../../include/__impl_file.h"

static FILE *ofl_head;
static sprt::qmutex ofl_lock;

FILE **__ofl_lock() {
	ofl_lock.lock();
	return &ofl_head;
}

void __ofl_unlock() { ofl_lock.unlock(); }

FILE *__ofl_add(FILE *f) {
	FILE **head = __ofl_lock();
	f->next = *head;
	if (*head) {
		(*head)->prev = f;
	}
	*head = f;
	__ofl_unlock();
	return f;
}
