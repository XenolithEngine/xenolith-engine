#include "../../include/__impl_file.h"

int __overflow(FILE *f, int _c) {
	unsigned char c = _c;
	if (!f->wend && __towrite(f)) {
		return EOF;
	}
	if (f->wpos != f->wend && c != f->lbf) {
		return *f->wpos++ = c;
	}
	if (f->write(f, &c, 1) != 1) {
		return EOF;
	}
	return c;
}

int __uflow(FILE *f) {
	unsigned char c;
	if (!__toread(f) && f->read(f, &c, 1) == 1) {
		return c;
	}
	return EOF;
}
