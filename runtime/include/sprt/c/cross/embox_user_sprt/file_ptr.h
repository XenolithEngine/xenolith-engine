// Freestanding libc: FILE is libc_impl's own struct, not a platform type.
// (embox_sprt aliases Embox's `struct file_struct`; nothing of the sort is
// even on the include path for this target.)
typedef struct __sprt_file_struct __SPRT_ID(FILE);
