// What the Tau platform layer of the runtime needs from the module it runs in.
//
// A Tau module reaches the kernel and the system services only through libtau, which is
// not part of the engine: libtau implements these functions for user modules and the
// runtime's Tau files call them, as the EmboxUser files make system calls. Plain C with
// no includes. A result >= 0 is success or a count, -E* an error (Linux numbers, as the
// rest of tau_sprt); nothing here touches errno. With the shared runtime (libsprt.so,
// RT-A-SharedRuntime-1) the module exports these to it, so they are never hidden.

#ifndef RUNTIME_INCLUDE_SPRT_C_CROSS_TAU_SPRT_SPRT_TAU_H_
#define RUNTIME_INCLUDE_SPRT_C_CROSS_TAU_SPRT_SPRT_TAU_H_

#ifdef __cplusplus
extern "C" {
#endif

#pragma GCC visibility push(default)

#define __SPRT_TAU_EPERM 1
#define __SPRT_TAU_ENOENT 2
#define __SPRT_TAU_EINTR 4
#define __SPRT_TAU_EIO 5
#define __SPRT_TAU_EBADF 9
#define __SPRT_TAU_EAGAIN 11
#define __SPRT_TAU_ENOMEM 12
#define __SPRT_TAU_EACCES 13
#define __SPRT_TAU_EBUSY 16
#define __SPRT_TAU_EEXIST 17
#define __SPRT_TAU_EXDEV 18
#define __SPRT_TAU_ENOTDIR 20
#define __SPRT_TAU_EISDIR 21
#define __SPRT_TAU_EINVAL 22
#define __SPRT_TAU_EMFILE 24
#define __SPRT_TAU_EFBIG 27
#define __SPRT_TAU_ENOSPC 28
#define __SPRT_TAU_EROFS 30
#define __SPRT_TAU_EMLINK 31
#define __SPRT_TAU_EPIPE 32
#define __SPRT_TAU_ENAMETOOLONG 36
#define __SPRT_TAU_ENOSYS 38
#define __SPRT_TAU_ENOTEMPTY 39
#define __SPRT_TAU_ELOOP 40
#define __SPRT_TAU_EMSGSIZE 90
#define __SPRT_TAU_ENOTSUP 95
#define __SPRT_TAU_EADDRINUSE 98
#define __SPRT_TAU_ENETDOWN 100
#define __SPRT_TAU_ECONNRESET 104
#define __SPRT_TAU_EISCONN 106
#define __SPRT_TAU_ENOTCONN 107
#define __SPRT_TAU_ETIMEDOUT 110
#define __SPRT_TAU_ECONNREFUSED 111
#define __SPRT_TAU_EHOSTUNREACH 113
#define __SPRT_TAU_EINPROGRESS 115

#define __SPRT_TAU_TIMEOUT_INFINITE (~0ull)

#define __SPRT_TAU_POLLIN 0x001
#define __SPRT_TAU_POLLOUT 0x004
#define __SPRT_TAU_POLLERR 0x008
#define __SPRT_TAU_POLLHUP 0x010

struct __sprt_tau_process {
	const char *name;
	unsigned long stack_top;
	unsigned long stack_size;
	unsigned long page_size;
	unsigned module;
	unsigned priority;
	int service_thread; // the manifest asks for a service thread for the module's rings
	unsigned cpus; // the CPUs its threads run on
};

typedef struct __sprt_tau_thread __sprt_tau_thread;
typedef struct __sprt_tau_stream __sprt_tau_stream;

// Process. init() runs first, on the StartInfo the kernel passed in x0, before any TLS
// exists; attach_main() makes `tp` the main thread's TLS pointer; with service_thread the
// runtime then gives libtau a stack and a TLS block for it.
int __sprt_tau_init(const void *start_info, struct __sprt_tau_process *out);
// The constructors of the module's shared libraries, its dependencies first, after the libc and
// before the module's own; root linked them before the module started. `self` is the base of the
// shared runtime, which ran its own already; with it the module's constructors run here too, last.
void __sprt_tau_init_libraries(int argc, char **argv, char **envp, const void *self);

// dlopen of a library root linked from the module's preload list or DT_NEEDED (SYS-A-Preload-1):
// open runs its constructors once, a NULL path gives the global scope; sym looks in a handle's
// libraries, a NULL handle in the global scope; addr names the object and symbol of an address.
// Failures leave the reason in *error; the caller serializes the calls.
void *__sprt_tau_dl_open(const char *path, const char **error);
void *__sprt_tau_dl_sym(void *handle, const char *name, const char **error);
int __sprt_tau_dl_close(void *handle, const char **error);
int __sprt_tau_dl_addr(const void *addr, const char **fname, void **fbase, const char **sname, void **saddr);

// The loaded object `index` of the module, 0 being the module (dl_iterate_phdr); 0 past the last.
int __sprt_tau_dl_object(int index, unsigned long *base, const char **name, const void **phdr,
		unsigned short *phnum);

// A thread-local template of the module or of one of its shared libraries (SYS-A-StaticTls-1),
// and where each thread's block has its copy, from the thread pointer.
struct __sprt_tau_tls_image {
	const void *image;
	unsigned long filesz;
	unsigned long memsz;
	unsigned long align;
	long offset;
};

// The templates root laid out for a module linked with shared libraries, at most `max`; 0 for a
// module without them, whose own PT_TLS the runtime finds itself.
int __sprt_tau_tls_images(struct __sprt_tau_tls_image *out, int max);
int __sprt_tau_attach_main(void *tp);
int __sprt_tau_service_start(void *stack_top, void *tp);
__attribute__((noreturn)) void __sprt_tau_exit(int status);
void __sprt_tau_debug_write(const char *str, unsigned long len);

// Memory: zeroed, readable and writable, aligned to `align` (a power of two). GUARD
// leaves the page below unmapped. An unmap of part of a mapping keeps it mapped, except for
// JIT memory: pages in the JIT window, each its own frame, whose rights protect changes.
#define __SPRT_TAU_MEM_GUARD 1u
#define __SPRT_TAU_MEM_JIT 2u
int __sprt_tau_mem_map(unsigned long len, unsigned long align, unsigned flags, void **out);
int __sprt_tau_mem_unmap(void *addr, unsigned long len);
// Rights of the pages: a mask of READ, WRITE and EXEC. On JIT memory read, read-write, read and
// execute or none; write and execute together is EACCES. Elsewhere only the rights a mapping has
// already, without EXEC (EACCES).
#define __SPRT_TAU_PROT_READ 1
#define __SPRT_TAU_PROT_WRITE 2
#define __SPRT_TAU_PROT_EXEC 4
int __sprt_tau_mem_protect(void *addr, unsigned long len, int prot);
// Bytes of the module's quota, 0 when root does not say.
unsigned long long __sprt_tau_mem_total(void);

// Time from the virtual counter, since boot.
unsigned long long __sprt_tau_clock_ns(void);
unsigned long long __sprt_tau_clock_res_ns(void);
int __sprt_tau_sleep_ns(unsigned long long ns);
void __sprt_tau_yield(void);
// The module's generator, keyed from the seed root gives each start; never waits.
int __sprt_tau_random(void *buf, unsigned long len);
// Whether a getrandom may take the generator's bytes: 0 when it may, -EAGAIN when the
// generator is not credited and NONBLOCK is set. INSECURE takes them in any case.
#define __SPRT_TAU_RANDOM_NONBLOCK 1u
#define __SPRT_TAU_RANDOM_INSECURE 2u
int __sprt_tau_random_wait(unsigned flags);

// The module's private futex: wait returns 0 when woken (or spuriously), -EAGAIN when
// *addr != expected, -ETIMEDOUT; wake returns how many it woke.
int __sprt_tau_futex_wait(volatile unsigned *addr, unsigned expected, unsigned long long timeout_ns);
int __sprt_tau_futex_wake(volatile unsigned *addr, int count);

// Threads. entry(arg) runs on `stack_top` with `tp` as its TLS pointer; returning from
// entry ends the thread. wait() returns 0 once the thread is gone, -ETIMEDOUT otherwise;
// release() frees the handle of a thread that is gone. Thread ids are never 0; the main
// thread's is 1.
int __sprt_tau_thread_create(void (*entry)(void *), void *arg, void *stack_top, void *tp, int priority,
		__sprt_tau_thread **out);
__attribute__((noreturn)) void __sprt_tau_thread_exit(void);
int __sprt_tau_thread_wait(__sprt_tau_thread *thread, unsigned long long timeout_ns);
void __sprt_tau_thread_release(__sprt_tau_thread *thread);
unsigned __sprt_tau_thread_self(void);
unsigned __sprt_tau_thread_id(const __sprt_tau_thread *thread);

// Streams of the module's services: the console for fds 0-2. NONBLOCK returns
// -EAGAIN instead of waiting. ready() never blocks and returns the POLL* bits that hold;
// io_epoch() changes whenever any stream may have become ready, and io_wait() sleeps
// until it differs from `epoch` or the timeout passes (0 or -ETIMEDOUT).
#define __SPRT_TAU_IO_NONBLOCK 1u
int __sprt_tau_console_open(int fd, __sprt_tau_stream **out);
long __sprt_tau_stream_read(__sprt_tau_stream *s, void *buf, unsigned long len, unsigned flags);
long __sprt_tau_stream_write(__sprt_tau_stream *s, const void *buf, unsigned long len, unsigned flags);
int __sprt_tau_stream_sync(__sprt_tau_stream *s);
int __sprt_tau_stream_close(__sprt_tau_stream *s);
unsigned __sprt_tau_stream_ready(__sprt_tau_stream *s, unsigned events);
unsigned __sprt_tau_io_epoch(void);
int __sprt_tau_io_wait(unsigned epoch, unsigned long long timeout_ns);

// Files of the module's namespace (tau.fs@1 through libtau). `dir` is an open directory, or
// nullptr for the namespace root; a path that starts with '/' ignores it. Open flags and
// node types are those of tau.fs; times are nanoseconds since 1970.
#define __SPRT_TAU_FS_READ 0x01
#define __SPRT_TAU_FS_WRITE 0x02
#define __SPRT_TAU_FS_CREATE 0x04
#define __SPRT_TAU_FS_EXCL 0x08
#define __SPRT_TAU_FS_TRUNC 0x10
#define __SPRT_TAU_FS_APPEND 0x20
#define __SPRT_TAU_FS_DIRECTORY 0x40
#define __SPRT_TAU_FS_NOFOLLOW 0x80

#define __SPRT_TAU_FS_REG 1
#define __SPRT_TAU_FS_DIR 2
#define __SPRT_TAU_FS_LNK 3
#define __SPRT_TAU_FS_CHR 4
#define __SPRT_TAU_FS_BLK 5
#define __SPRT_TAU_FS_FIFO 6
#define __SPRT_TAU_FS_SOCK 7

typedef struct __sprt_tau_file __sprt_tau_file;

struct __sprt_tau_fs_stat {
	unsigned long long ino;
	unsigned long long size;
	unsigned long long blocks; // of 512 bytes
	long long atime_ns;
	long long mtime_ns;
	long long ctime_ns;
	unsigned mount;
	unsigned type;
	unsigned mode; // permission bits
	unsigned nlink;
	unsigned uid;
	unsigned gid;
};

struct __sprt_tau_fs_statfs {
	unsigned long long blocks;
	unsigned long long bfree;
	unsigned long long files;
	unsigned long long ffree;
	unsigned bsize;
	unsigned name_max;
	unsigned mount;
	unsigned ro;
};

// A directory entry; namelen bytes of the name follow, the entry takes reclen bytes.
struct __sprt_tau_fs_entry {
	unsigned long long ino;
	unsigned long long cookie; // reading from it goes on after this entry
	unsigned short reclen;
	unsigned char namelen;
	unsigned char type;
};

int __sprt_tau_fs_open(__sprt_tau_file *dir, const char *path, unsigned flags, unsigned mode, __sprt_tau_file **out);
int __sprt_tau_fs_close(__sprt_tau_file *f);
long __sprt_tau_fs_pread(__sprt_tau_file *f, void *buf, unsigned long len, unsigned long long off);
// With __SPRT_TAU_FS_APPEND the data goes to the end, whatever off says.
long __sprt_tau_fs_pwrite(__sprt_tau_file *f, const void *buf, unsigned long len, unsigned long long off);
int __sprt_tau_fs_fstat(__sprt_tau_file *f, struct __sprt_tau_fs_stat *out);
int __sprt_tau_fs_statat(__sprt_tau_file *dir, const char *path, int nofollow, struct __sprt_tau_fs_stat *out);
int __sprt_tau_fs_fsync(__sprt_tau_file *f);
int __sprt_tau_fs_ftruncate(__sprt_tau_file *f, unsigned long long size);
int __sprt_tau_fs_fchmod(__sprt_tau_file *f, unsigned mode);
// A negative time stays as it is.
int __sprt_tau_fs_futimens(__sprt_tau_file *f, long long atime_ns, long long mtime_ns);
int __sprt_tau_fs_mkdir(__sprt_tau_file *dir, const char *path, unsigned mode);
int __sprt_tau_fs_unlink(__sprt_tau_file *dir, const char *path);
int __sprt_tau_fs_rmdir(__sprt_tau_file *dir, const char *path);
int __sprt_tau_fs_rename(__sprt_tau_file *dir, const char *from, __sprt_tau_file *dir2, const char *to);
int __sprt_tau_fs_link(__sprt_tau_file *dir, const char *from, __sprt_tau_file *dir2, const char *to);
int __sprt_tau_fs_symlink(const char *target, __sprt_tau_file *dir, const char *path);
long __sprt_tau_fs_readlink(__sprt_tau_file *dir, const char *path, char *buf, unsigned long len);
// Entries of the directory from *cookie into buf; *cookie moves past them and is 0 after the
// last one. Returns the bytes written.
long __sprt_tau_fs_readdir(__sprt_tau_file *f, unsigned long long *cookie, void *buf, unsigned long len);
int __sprt_tau_fs_statvfs(__sprt_tau_file *dir, const char *path, struct __sprt_tau_fs_statfs *out);

// IPv4 sockets of the module (tau.net@1 through libtau). Addresses and ports are in host
// order. With IO_NONBLOCK a call answers -EAGAIN (a connect -EINPROGRESS) instead of waiting;
// a wait ends after timeout_ns with the same answers. ready() never blocks and returns the
// POLL* bits that hold; a change of them moves io_epoch().
#define __SPRT_TAU_SOCK_STREAM 1
#define __SPRT_TAU_SOCK_DGRAM 2
#define __SPRT_TAU_MSG_PEEK 2u
#define __SPRT_TAU_MSG_TRUNC 4u
#define __SPRT_TAU_SHUT_RD 1
#define __SPRT_TAU_SHUT_WR 2

typedef struct __sprt_tau_sock __sprt_tau_sock;

struct __sprt_tau_inet {
	unsigned addr;
	unsigned short port;
};

int __sprt_tau_sock_open(int type, __sprt_tau_sock **out);
int __sprt_tau_sock_bind(__sprt_tau_sock *s, const struct __sprt_tau_inet *addr);
int __sprt_tau_sock_listen(__sprt_tau_sock *s, int backlog);
int __sprt_tau_sock_connect(__sprt_tau_sock *s, const struct __sprt_tau_inet *peer, unsigned flags,
		unsigned long long timeout_ns);
int __sprt_tau_sock_accept(__sprt_tau_sock *s, unsigned flags, unsigned long long timeout_ns, __sprt_tau_sock **out,
		struct __sprt_tau_inet *peer);
// The bytes the stack took: all of them unless the wait ended. `to` is for a datagram socket.
long __sprt_tau_sock_send(__sprt_tau_sock *s, const void *buf, unsigned long len, const struct __sprt_tau_inet *to,
		unsigned flags, unsigned long long timeout_ns);
// *flags: IO_NONBLOCK and MSG_PEEK in, MSG_TRUNC out (a datagram longer than len).
long __sprt_tau_sock_recv(__sprt_tau_sock *s, void *buf, unsigned long len, struct __sprt_tau_inet *from,
		unsigned *flags, unsigned long long timeout_ns);
int __sprt_tau_sock_shutdown(__sprt_tau_sock *s, int how);
// Sends in flight reach the stack first.
int __sprt_tau_sock_close(__sprt_tau_sock *s);
int __sprt_tau_sock_name(__sprt_tau_sock *s, int peer, struct __sprt_tau_inet *out);
// The socket's pending error, taken: 0 or -errno.
int __sprt_tau_sock_error(__sprt_tau_sock *s);
unsigned __sprt_tau_sock_ready(__sprt_tau_sock *s, unsigned events);
// The address of a host name; -ENOENT when it has none.
int __sprt_tau_resolve(const char *name, unsigned *addr, unsigned long long timeout_ns);

// The display of the module (dev.display@1 through libtau): one session with both buffers
// mapped. open() attaches, `wait` waits for a provider to start; -EBUSY: another client holds
// the display, -ENOENT: no provider runs. present() shows a rectangle of a buffer and returns
// once the buffer may be drawn into again; -ECONNRESET: the session is lost with its buffers
// (the driver died), open() again.
#define __SPRT_TAU_DISPLAY_B8G8R8A8 1

struct __sprt_tau_display_info {
	unsigned width;
	unsigned height;
	unsigned stride; // bytes of a row
	unsigned format;
	unsigned buffers;
	unsigned refresh_mhz;
};

int __sprt_tau_display_open(int wait, struct __sprt_tau_display_info *info);
// Buffer `index` of the session: stride * height bytes.
int __sprt_tau_display_buffer(unsigned index, void **data, unsigned long *size);
int __sprt_tau_display_present(unsigned index, unsigned x, unsigned y, unsigned w, unsigned h);
// Ends the session; the display keeps the last frame.
void __sprt_tau_display_close(void);

// The input of the module (dev.input@1 through libtau): one session with every event of the
// provider. open() attaches, `wait` waits for a provider to start; -EBUSY: another client holds
// the input, -ENOENT: no provider runs. next() takes an event without waiting: 1 with one, 0
// without; -ECONNRESET: the session is lost (the provider died), open() again. An event wakes
// __sprt_tau_io_wait.
#define __SPRT_TAU_INPUT_TOUCH 1 // device kinds
#define __SPRT_TAU_INPUT_POINTER 2
#define __SPRT_TAU_INPUT_KEYBOARD 3
#define __SPRT_TAU_INPUT_TABLET 4

#define __SPRT_TAU_INPUT_EV_CONNECT 1 // value: the device's kind
#define __SPRT_TAU_INPUT_EV_DISCONNECT 2
#define __SPRT_TAU_INPUT_EV_TOUCH 3 // phase, contact, x, y
#define __SPRT_TAU_INPUT_EV_POINTER 4 // x, y deltas, value buttons (bit n: button n + 1), wheel
#define __SPRT_TAU_INPUT_EV_KEY 5 // phase, value the usage of HID page 7
#define __SPRT_TAU_INPUT_EV_ABSOLUTE 6 // x, y the place, value buttons, wheel

#define __SPRT_TAU_INPUT_DOWN 1 // phases
#define __SPRT_TAU_INPUT_MOVE 2
#define __SPRT_TAU_INPUT_UP 3

#define __SPRT_TAU_INPUT_FRAME 1 // marks: the last event of a report

struct __sprt_tau_input_event {
	unsigned kind;
	unsigned phase;
	unsigned marks;
	unsigned device;
	unsigned long long time; // ns of the provider's clock
	unsigned contact;
	int x;
	int y;
	unsigned value;
	int wheel;
};

struct __sprt_tau_input_device {
	unsigned id; // the device of its events
	unsigned kind;
	unsigned contacts;
	unsigned x_max; // touch, tablet: x is 0 up to x_max
	unsigned y_max;
};

int __sprt_tau_input_open(int wait);
// The device `id` names; -ENOENT when the provider lists none.
int __sprt_tau_input_device(unsigned id, struct __sprt_tau_input_device *out);
int __sprt_tau_input_next(struct __sprt_tau_input_event *out);
// Ends the session; the provider takes a new one.
void __sprt_tau_input_close(void);

#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif

#endif // RUNTIME_INCLUDE_SPRT_C_CROSS_TAU_SPRT_SPRT_TAU_H_
