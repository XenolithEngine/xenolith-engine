#include "stdlib.h"

__SPRT_C_FUNC void *bsearch(const void *key, const void *base, size_t nel, size_t width,
		int (*cmp)(const void *, const void *)) __SPRT_NOEXCEPT {
	void *___try;
	int sign;
	while (nel > 0) {
		___try = (char *)base + width * (nel / 2);
		sign = cmp(key, ___try);
		if (sign < 0) {
			nel /= 2;
		} else if (sign > 0) {
			base = (char *)___try + width;
			nel -= nel / 2 + 1;
		} else {
			return ___try;
		}
	}
	return NULL;
}
