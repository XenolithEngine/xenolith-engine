/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef RUNTIME_FREESTANDING_INCLUDE_IMPL_FILE_H_
#define RUNTIME_FREESTANDING_INCLUDE_IMPL_FILE_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/runtime/thread/rmutex.h>
#include <sprt/runtime/thread/qmutex.h>
#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_unistd.h>

#include "locale.h"
#include "stdio.h"
#include "sys/sprt.h"

#define weak __attribute__((__weak__))
#define hidden __attribute__((__visibility__("hidden")))
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128

struct __sprt_file_struct {
	sprt::uint32_t flags;
	unsigned char *rpos, *rend;
	int (*close)(FILE *);
	unsigned char *wend, *wpos;
	unsigned char *mustbezero_1;
	unsigned char *wbase;
	sprt::size_t (*read)(FILE *, unsigned char *, sprt::size_t);
	sprt::size_t (*write)(FILE *, const unsigned char *, sprt::size_t);
	sprt::off_t (*seek)(FILE *, sprt::off_t, int);
	unsigned char *buf;
	sprt::size_t buf_size;
	FILE *prev, *next;
	int fd;
	void *pipe_handle;
	int mode;
	int lbf;
	void *cookie;
	sprt::off_t off;
	char *getln_buf;
	void *mustbezero_2;
	unsigned char *shend;
	sprt::off_t shlim, shcnt;
	FILE *prev_locked, *next_locked;
	locale_t locale;
	__sprt_pid_t __lock_pid;

	int __lock() {
		// do not lock files, that specifically marked with -1 or files that already locked
		if (__lock_pid < 0 || __lock_pid == __sprt_gettid()) {
			return 1;
		} else {
			return sprt_plock_lock(this, 0, &__lock_pid);
		}
	}

	void __unlock(int lockToken) {
		if (lockToken == 0) {
			sprt_plock_unlock(this, 0, &__lock_pid);
		}
	}
};


#define FLOCK(f) auto __lockToken = f->__lock()
#define FUNLOCK(f) do { f->__unlock(__lockToken); } while(0)

extern "C" {
extern FILE *volatile __stdin_used;
extern FILE *volatile __stdout_used;
extern FILE *volatile __stderr_used;
}

/*size_t __stdio_read(FILE *, unsigned char *, size_t);
size_t __stdio_write(FILE *, const unsigned char *, size_t);
size_t __stdout_write(FILE *, const unsigned char *, size_t);
off_t __stdio_seek(FILE *, off_t, int);
int __stdio_close(FILE *);*/

int __toread(FILE *);
int __towrite(FILE *);

void __stdio_exit(void);

int __overflow(FILE *, int);
int __uflow(FILE *);

int __fseeko_unlocked(FILE *, off_t, int);
off_t __ftello_unlocked(FILE *);
size_t __fwritex(const unsigned char *, size_t, FILE *);
int __putc_unlocked(int, FILE *);

FILE *__fdopen(int, const char *);
int __fmodeflags(const char *);

FILE *__ofl_add(FILE *f);
FILE **__ofl_lock(void);
void __ofl_unlock(void);

struct __pthread;
void __register_locked_file(FILE *, struct __pthread *);
void __unlist_locked_file(FILE *);
void __do_orphaned_stdio_locks(void);

void __getopt_msg(const char *, const char *, const char *, size_t);

#define __feof(f) ((f)->flags & F_EOF)
#define __ferror(f) ((f)->flags & F_ERR)

#define __getc_unlocked(f) \
	( ((f)->rpos != (f)->rend) ? *(f)->rpos++ : __uflow((f)) )

#define __putc_unlocked(c, f) \
	( (((unsigned char)(c)!=(f)->lbf && (f)->wpos!=(f)->wend)) \
	? *(f)->wpos++ = (unsigned char)(c) \
	: __overflow((f),(unsigned char)(c)) )

/* Caller-allocated FILE * operations */
FILE *__fopen_rb_ca(const char *, FILE *, unsigned char *, size_t);
int __fclose_ca(FILE *);

#endif // RUNTIME_FREESTANDING_INCLUDE_IMPL_FILE_H_
