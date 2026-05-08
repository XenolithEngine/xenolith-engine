#include <sprt/c/bits/__sprt_uintptr_t.h>
#include <sprt/c/bits/__sprt_def.h>

__SPRT_C_FUNC void *memset(void *dst, int c, size_t len) {
	void *r = dst;
	asm volatile("rep stosb" : "+D"(dst), "+c"(len) : "a"(c) : "memory");
	return r;
}

__SPRT_C_FUNC void *memmove(void *dst, void *src, size_t len) {
	// Use uintptr_t to bypass pointer semantics:
	// (1) comparing unrelated pointers
	// (2) pointer arithmetic on null (i.e. gracefully handle null dst/src)
	// (3) pointer overflow ("one-before-the-beginning" in reversed copy)
	__sprt_uintptr_t d = (__sprt_uintptr_t)dst;
	__sprt_uintptr_t s = (__sprt_uintptr_t)src;
	if (d > s) {
		d += len - 1;
		s += len - 1;
		asm("std");
	}
	asm volatile("rep movsb; cld" : "+D"(d), "+S"(s), "+c"(len) : : "memory");
	return dst;
}

__SPRT_C_FUNC int memcmp(void *s1, void *s2, size_t len) {
	// CCa "after"  == CF=0 && ZF=0
	// CCb "before" == CF=1
	int a, b;
	asm volatile(
			"xor %%eax, %%eax\n" // CF=0, ZF=1 (i.e. CCa = CCb = 0)
			"repz cmpsb\n"
			: "+D"(s1), "+S"(s2), "+c"(len), "=@cca"(a), "=@ccb"(b)
			:
			: "ax", "memory");
	return b - a;
}

__SPRT_C_FUNC size_t strlen(char *s) {
	size_t n = -1;
	asm volatile("repne scasb" : "+D"(s), "+c"(n) : "a"(0) : "memory");
	return -n - 2;
}
