// Embox libc uses `struct file_struct` (named via typedef FILE in <stdio.h>)
// rather than glibc's `_IO_FILE`. The sprt runtime treats FILE as an opaque
// pointer, so forward-declaring the struct here and matching the Embox typedef
// is enough — `<stdio.h>` from the Embox libc later completes it.
struct file_struct;
typedef struct file_struct __SPRT_ID(FILE);
