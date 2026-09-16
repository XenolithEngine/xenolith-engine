// wasm uses the freestanding musl-based FILE (defined by the libc_impl stdio
// internals as __sprt_file_struct), like the Windows freestanding build. The
// Linux target instead aliases the host stdio's _IO_FILE.
typedef struct __sprt_file_struct __SPRT_ID(FILE);
