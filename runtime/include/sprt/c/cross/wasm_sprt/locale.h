// wasm uses the freestanding locale representation (a refcounted __locale_struct
// wrapper), same as the Windows freestanding libc. The minimal C/C.UTF-8 backend
// lives in libc_impl/src/wasm/locale.cc.
typedef struct __freestanding_locale_struct *__SPRT_ID(locale_t);
