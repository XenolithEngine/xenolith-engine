// Freestanding libc: the refcounted __freestanding_locale_struct from
// libc_impl, the same one wasm and the Windows freestanding build use. Embox's
// hosted `void *` handle does not apply - there is no Embox libc here to hand
// it to.
typedef struct __freestanding_locale_struct *__SPRT_ID(locale_t);
