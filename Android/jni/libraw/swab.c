#include "swab.h"
void swab(const void *p_src_, void *p_dst_, ssize_t n) {
	const uint8_t *p_src = p_src_;
	uint8_t *p_dst = p_dst_;
	uint8_t tmp;
	ssize_t i;

	if (n < 0)
		return;

	for (i = 0; i < n - 1; i += 2) {
		tmp = p_src[i + 0];
		p_dst[i + 0] = p_src[i + 1];
		p_dst[i + 1] = tmp;
	}
}
