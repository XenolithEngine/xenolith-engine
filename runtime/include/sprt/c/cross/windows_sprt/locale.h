typedef struct __crt_locale_pointers {
	struct __crt_locale_data *locinfo;
	struct __crt_multibyte_data *mbcinfo;
} __crt_locale_pointers;

typedef __crt_locale_pointers *__SPRT_ID(locale_t);
